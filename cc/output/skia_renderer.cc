// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/skia_renderer.h"

#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/math_util.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/output_surface.h"
#include "cc/output/output_surface_frame.h"
#include "cc/output/render_surface_filters.h"
#include "cc/output/renderer_settings.h"
#include "cc/quads/debug_border_draw_quad.h"
#include "cc/quads/picture_draw_quad.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/quads/tile_draw_quad.h"
#include "cc/resources/scoped_resource.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "skia/ext/opacity_filter_canvas.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImageFilter.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPoint.h"
#include "third_party/skia/include/core/SkShader.h"
#include "third_party/skia/include/effects/SkLayerRasterizer.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/transform.h"

namespace cc {
namespace {

static inline bool IsScalarNearlyInteger(SkScalar scalar) {
  return SkScalarNearlyZero(scalar - SkScalarRoundToScalar(scalar));
}

bool IsScaleAndIntegerTranslate(const SkMatrix& matrix) {
  return IsScalarNearlyInteger(matrix[SkMatrix::kMTransX]) &&
         IsScalarNearlyInteger(matrix[SkMatrix::kMTransY]) &&
         SkScalarNearlyZero(matrix[SkMatrix::kMSkewX]) &&
         SkScalarNearlyZero(matrix[SkMatrix::kMSkewY]) &&
         SkScalarNearlyZero(matrix[SkMatrix::kMPersp0]) &&
         SkScalarNearlyZero(matrix[SkMatrix::kMPersp1]) &&
         SkScalarNearlyZero(matrix[SkMatrix::kMPersp2] - 1.0f);
}

}  // anonymous namespace

SkiaRenderer::SkiaRenderer(const RendererSettings* settings,
                           OutputSurface* output_surface,
                           ResourceProvider* resource_provider)
    : DirectRenderer(settings, output_surface, resource_provider) {}

SkiaRenderer::~SkiaRenderer() {}

bool SkiaRenderer::CanPartialSwap() {
  auto* context_provider = output_surface_->context_provider();
  return context_provider->ContextCapabilities().post_sub_buffer;
}

void SkiaRenderer::BeginDrawingFrame(DrawingFrame* frame) {
  TRACE_EVENT0("cc", "SkiaRenderer::BeginDrawingFrame");

  // ----- BEGIN COPYPASTA FROM GL RENDERER
  bool use_sync_query_ = false;
  scoped_refptr<ResourceProvider::Fence> read_lock_fence;
  if (use_sync_query_) {
#if 0
    // Block until oldest sync query has passed if the number of pending queries
    // ever reach kMaxPendingSyncQueries.
    if (pending_sync_queries_.size() >= kMaxPendingSyncQueries) {
      LOG(ERROR) << "Reached limit of pending sync queries.";

      pending_sync_queries_.front()->Wait();
      DCHECK(!pending_sync_queries_.front()->IsPending());
    }

    while (!pending_sync_queries_.empty()) {
      if (pending_sync_queries_.front()->IsPending())
        break;

      available_sync_queries_.push_back(PopFront(&pending_sync_queries_));
    }

    current_sync_query_ = available_sync_queries_.empty()
                              ? base::MakeUnique<SyncQuery>(gl_)
                              : PopFront(&available_sync_queries_);

    read_lock_fence = current_sync_query_->Begin();
#endif
  } else {
    read_lock_fence = make_scoped_refptr(new ResourceProvider::SynchronousFence(
        output_surface_->context_provider()->ContextGL()));
  }
  resource_provider_->SetReadLockFence(read_lock_fence.get());

  // Insert WaitSyncTokenCHROMIUM on quad resources prior to drawing the frame,
  // so that drawing can proceed without GL context switching interruptions.
  ResourceProvider* resource_provider = resource_provider_;
  for (const auto& pass : *frame->render_passes_in_draw_order) {
    for (auto* quad : pass->quad_list) {
      for (ResourceId resource_id : quad->resources)
        resource_provider->WaitSyncTokenIfNeeded(resource_id);
    }
  }
  // ----- END COPYPASTA FROM GL RENDERER
}

void SkiaRenderer::FinishDrawingFrame(DrawingFrame* frame) {
  TRACE_EVENT0("cc", "SkiaRenderer::FinishDrawingFrame");
  current_framebuffer_surface_lock_ = nullptr;
  current_framebuffer_lock_ = nullptr;
  current_canvas_ = nullptr;

  swap_buffer_rect_ = frame->root_damage_rect;
}

void SkiaRenderer::SwapBuffers(std::vector<ui::LatencyInfo> latency_info) {
  DCHECK(visible_);
  TRACE_EVENT0("cc,benchmark", "SkiaRenderer::SwapBuffers");
  OutputSurfaceFrame output_frame;
  output_frame.latency_info = std::move(latency_info);
  output_frame.size = surface_size_for_swap_buffers();
  if (use_partial_swap_) {
    swap_buffer_rect_.Intersect(gfx::Rect(surface_size_for_swap_buffers()));
  } else {
    if (!swap_buffer_rect_.IsEmpty() || !allow_empty_swap_)
      swap_buffer_rect_ = gfx::Rect(surface_size_for_swap_buffers());
  }

  output_frame.sub_buffer_rect = swap_buffer_rect_;
  output_surface_->SwapBuffers(std::move(output_frame));

  swap_buffer_rect_ = gfx::Rect();
}

bool SkiaRenderer::FlippedFramebuffer(const DrawingFrame* frame) const {
  return false;
}

void SkiaRenderer::EnsureScissorTestEnabled() {
  is_scissor_enabled_ = true;
  SetClipRect(scissor_rect_);
}

void SkiaRenderer::EnsureScissorTestDisabled() {
  // There is no explicit notion of enabling/disabling scissoring in software
  // rendering, but the underlying effect we want is to clear any existing
  // clipRect on the current SkCanvas. This is done by setting clipRect to
  // the viewport's dimensions.
  if (!current_canvas_)
    return;
  is_scissor_enabled_ = false;
  SkISize size = current_canvas_->getBaseLayerSize();
  SetClipRect(gfx::Rect(size.width(), size.height()));
}

void SkiaRenderer::BindFramebufferToOutputSurface(DrawingFrame* frame) {
  DCHECK(!output_surface_->HasExternalStencilTest());
  current_framebuffer_lock_ = nullptr;

  // TODO(enne): Probably don't need to recreate this every frame?
  GrBackendRenderTargetDesc desc;
  desc.fWidth = frame->device_viewport_size.width();
  desc.fHeight = frame->device_viewport_size.height();
  desc.fConfig = kRGBA_8888_GrPixelConfig;
  desc.fOrigin = kBottomLeft_GrSurfaceOrigin;
  desc.fSampleCnt = 1;
  desc.fStencilBits = 8;
  desc.fRenderTargetHandle = 0;

  GrContext* gr_context = output_surface_->context_provider()->GrContext();

  // Copypasta from resource_provider
  // Use unknown pixel geometry to disable LCD text.
  bool use_distance_field_text = false;
  bool can_use_lcd_text = true;
  uint32_t flags =
      use_distance_field_text ? SkSurfaceProps::kUseDistanceFieldFonts_Flag : 0;
  SkSurfaceProps surface_props(flags, kUnknown_SkPixelGeometry);
  if (can_use_lcd_text) {
    // LegacyFontHost will get LCD text and skia figures out what type to use.
    surface_props =
        SkSurfaceProps(flags, SkSurfaceProps::kLegacyFontHost_InitType);
  }
  root_surface_ =
      SkSurface::MakeFromBackendRenderTarget(gr_context, desc, &surface_props);
  root_canvas_ = root_surface_->getCanvas();

  current_canvas_ = root_canvas_;
}

bool SkiaRenderer::BindFramebufferToTexture(DrawingFrame* frame,
                                            const ScopedResource* texture) {
  DCHECK(texture->id());

  // Explicitly release lock, otherwise we can crash when try to lock
  // same texture again.
  current_framebuffer_surface_lock_ = nullptr;
  current_framebuffer_lock_ = nullptr;
  current_framebuffer_lock_ =
      base::WrapUnique(new ResourceProvider::ScopedWriteLockGL(
          resource_provider_, texture->id(), false));

  current_framebuffer_surface_lock_ =
      base::WrapUnique(new ResourceProvider::ScopedSkSurfaceProvider(
          output_surface_->context_provider(), current_framebuffer_lock_.get(),
          false, false, true, 0));

  current_canvas_ =
      current_framebuffer_surface_lock_->sk_surface()->getCanvas();
  return true;
}

void SkiaRenderer::SetScissorTestRect(const gfx::Rect& scissor_rect) {
  is_scissor_enabled_ = true;
  scissor_rect_ = scissor_rect;
  SetClipRect(scissor_rect);
}

void SkiaRenderer::SetClipRect(const gfx::Rect& rect) {
  if (!current_canvas_)
    return;
  // Skia applies the current matrix to clip rects so we reset it temporary.
  SkMatrix current_matrix = current_canvas_->getTotalMatrix();
  current_canvas_->resetMatrix();
  current_canvas_->clipRect(gfx::RectToSkRect(rect), SkRegion::kReplace_Op);
  current_canvas_->setMatrix(current_matrix);
}

void SkiaRenderer::ClearCanvas(SkColor color) {
  if (!current_canvas_)
    return;
  current_canvas_->clear(color);
}

void SkiaRenderer::ClearFramebuffer(DrawingFrame* frame) {
  if (frame->current_render_pass->has_transparent_background) {
    ClearCanvas(SkColorSetARGB(0, 0, 0, 0));
  } else {
#ifndef NDEBUG
    // On DEBUG builds, opaque render passes are cleared to blue
    // to easily see regions that were not drawn on the screen.
    ClearCanvas(SkColorSetARGB(255, 0, 0, 255));
#endif
  }
}

void SkiaRenderer::PrepareSurfaceForPass(
    DrawingFrame* frame,
    SurfaceInitializationMode initialization_mode,
    const gfx::Rect& render_pass_scissor) {
  switch (initialization_mode) {
    case SURFACE_INITIALIZATION_MODE_PRESERVE:
      EnsureScissorTestDisabled();
      return;
    case SURFACE_INITIALIZATION_MODE_FULL_SURFACE_CLEAR:
      EnsureScissorTestDisabled();
      ClearFramebuffer(frame);
      break;
    case SURFACE_INITIALIZATION_MODE_SCISSORED_CLEAR:
      SetScissorTestRect(render_pass_scissor);
      ClearFramebuffer(frame);
      break;
  }
}

// TODO(enne): doot dee doo
bool SkiaRenderer::IsSoftwareResource(ResourceId resource_id) const {
  switch (resource_provider_->GetResourceType(resource_id)) {
    case ResourceProvider::RESOURCE_TYPE_GPU_MEMORY_BUFFER:
    case ResourceProvider::RESOURCE_TYPE_GL_TEXTURE:
      return true;
    case ResourceProvider::RESOURCE_TYPE_BITMAP:
      return false;
  }

  LOG(FATAL) << "Invalid resource type.";
  return false;
}

void SkiaRenderer::DoDrawQuad(DrawingFrame* frame,
                              const DrawQuad* quad,
                              const gfx::QuadF* draw_region) {
  if (!current_canvas_)
    return;
  if (draw_region) {
    current_canvas_->save();
  }

  TRACE_EVENT0("cc", "SkiaRenderer::DoDrawQuad");
  gfx::Transform quad_rect_matrix;
  QuadRectTransform(&quad_rect_matrix,
                    quad->shared_quad_state->quad_to_target_transform,
                    gfx::RectF(quad->rect));
  gfx::Transform contents_device_transform =
      frame->window_matrix * frame->projection_matrix * quad_rect_matrix;
  contents_device_transform.FlattenTo2d();
  SkMatrix sk_device_matrix;
  gfx::TransformToFlattenedSkMatrix(contents_device_transform,
                                    &sk_device_matrix);
  current_canvas_->setMatrix(sk_device_matrix);

  current_paint_.reset();
  if (settings_->force_antialiasing ||
      !IsScaleAndIntegerTranslate(sk_device_matrix)) {
    // TODO(danakj): Until we can enable AA only on exterior edges of the
    // layer, disable AA if any interior edges are present. crbug.com/248175
    bool all_four_edges_are_exterior =
        quad->IsTopEdge() && quad->IsLeftEdge() && quad->IsBottomEdge() &&
        quad->IsRightEdge();
    if (settings_->allow_antialiasing &&
        (settings_->force_antialiasing || all_four_edges_are_exterior))
      current_paint_.setAntiAlias(true);
    current_paint_.setFilterQuality(kLow_SkFilterQuality);
  }

  if (quad->ShouldDrawWithBlending() ||
      quad->shared_quad_state->blend_mode != SkXfermode::kSrcOver_Mode) {
    current_paint_.setAlpha(quad->shared_quad_state->opacity * 255);
    current_paint_.setBlendMode(
        static_cast<SkBlendMode>(quad->shared_quad_state->blend_mode));
  } else {
    current_paint_.setBlendMode(SkBlendMode::kSrc);
  }

  if (draw_region) {
    gfx::QuadF local_draw_region(*draw_region);
    SkPath draw_region_clip_path;
    local_draw_region -=
        gfx::Vector2dF(quad->visible_rect.x(), quad->visible_rect.y());
    local_draw_region.Scale(1.0f / quad->visible_rect.width(),
                            1.0f / quad->visible_rect.height());
    local_draw_region -= gfx::Vector2dF(0.5f, 0.5f);

    SkPoint clip_points[4];
    QuadFToSkPoints(local_draw_region, clip_points);
    draw_region_clip_path.addPoly(clip_points, 4, true);

    current_canvas_->clipPath(draw_region_clip_path, SkRegion::kIntersect_Op,
                              false);
  }

  switch (quad->material) {
    case DrawQuad::DEBUG_BORDER:
      DrawDebugBorderQuad(frame, DebugBorderDrawQuad::MaterialCast(quad));
      break;
    case DrawQuad::PICTURE_CONTENT:
      DrawPictureQuad(frame, PictureDrawQuad::MaterialCast(quad));
      break;
    case DrawQuad::RENDER_PASS:
      DrawRenderPassQuad(frame, RenderPassDrawQuad::MaterialCast(quad));
      break;
    case DrawQuad::SOLID_COLOR:
      DrawSolidColorQuad(frame, SolidColorDrawQuad::MaterialCast(quad));
      break;
    case DrawQuad::TEXTURE_CONTENT:
      DrawTextureQuad(frame, TextureDrawQuad::MaterialCast(quad));
      break;
    case DrawQuad::TILED_CONTENT:
      DrawTileQuad(frame, TileDrawQuad::MaterialCast(quad));
      break;
    case DrawQuad::SURFACE_CONTENT:
      // Surface content should be fully resolved to other quad types before
      // reaching a direct renderer.
      NOTREACHED();
      break;
    case DrawQuad::INVALID:
    case DrawQuad::YUV_VIDEO_CONTENT:
    case DrawQuad::STREAM_VIDEO_CONTENT:
      DrawUnsupportedQuad(frame, quad);
      NOTREACHED();
      break;
  }

  current_canvas_->resetMatrix();
  if (draw_region) {
    current_canvas_->restore();
  }
}

void SkiaRenderer::DrawDebugBorderQuad(const DrawingFrame* frame,
                                       const DebugBorderDrawQuad* quad) {
  // We need to apply the matrix manually to have pixel-sized stroke width.
  SkPoint vertices[4];
  gfx::RectFToSkRect(QuadVertexRect()).toQuad(vertices);
  SkPoint transformed_vertices[4];
  current_canvas_->getTotalMatrix().mapPoints(transformed_vertices, vertices,
                                              4);
  current_canvas_->resetMatrix();

  current_paint_.setColor(quad->color);
  current_paint_.setAlpha(quad->shared_quad_state->opacity *
                          SkColorGetA(quad->color));
  current_paint_.setStyle(SkPaint::kStroke_Style);
  current_paint_.setStrokeWidth(quad->width);
  current_canvas_->drawPoints(SkCanvas::kPolygon_PointMode, 4,
                              transformed_vertices, current_paint_);
}

void SkiaRenderer::DrawPictureQuad(const DrawingFrame* frame,
                                   const PictureDrawQuad* quad) {
  SkMatrix content_matrix;
  content_matrix.setRectToRect(gfx::RectFToSkRect(quad->tex_coord_rect),
                               gfx::RectFToSkRect(QuadVertexRect()),
                               SkMatrix::kFill_ScaleToFit);
  current_canvas_->concat(content_matrix);

  const bool needs_transparency =
      SkScalarRoundToInt(quad->shared_quad_state->opacity * 255) < 255;
  const bool disable_image_filtering =
      disable_picture_quad_image_filtering_ || quad->nearest_neighbor;

  TRACE_EVENT0("cc", "SkiaRenderer::DrawPictureQuad");

  RasterSource::PlaybackSettings playback_settings;
  playback_settings.playback_to_shared_canvas = true;
  // Indicates whether content rasterization should happen through an
  // ImageHijackCanvas, which causes image decodes to be managed by an
  // ImageDecodeController. PictureDrawQuads are used for resourceless software
  // draws, while a GPU ImageDecodeController may be in use by the compositor
  // providing the RasterSource. So we disable the image hijack canvas to avoid
  // trying to use the GPU ImageDecodeController while doing a software draw.
  playback_settings.use_image_hijack_canvas = false;
  if (needs_transparency || disable_image_filtering) {
    // TODO(aelias): This isn't correct in all cases. We should detect these
    // cases and fall back to a persistent bitmap backing
    // (http://crbug.com/280374).
    // TODO(vmpstr): Fold this canvas into playback and have raster source
    // accept a set of settings on playback that will determine which canvas to
    // apply. (http://crbug.com/594679)
    skia::OpacityFilterCanvas filtered_canvas(current_canvas_,
                                              quad->shared_quad_state->opacity,
                                              disable_image_filtering);
    quad->raster_source->PlaybackToCanvas(
        &filtered_canvas, quad->content_rect, quad->content_rect,
        quad->contents_scale, playback_settings);
  } else {
    quad->raster_source->PlaybackToCanvas(
        current_canvas_, quad->content_rect, quad->content_rect,
        quad->contents_scale, playback_settings);
  }
}

void SkiaRenderer::DrawSolidColorQuad(const DrawingFrame* frame,
                                      const SolidColorDrawQuad* quad) {
  gfx::RectF visible_quad_vertex_rect = MathUtil::ScaleRectProportional(
      QuadVertexRect(), gfx::RectF(quad->rect), gfx::RectF(quad->visible_rect));
  current_paint_.setColor(quad->color);
  current_paint_.setAlpha(quad->shared_quad_state->opacity *
                          SkColorGetA(quad->color));
  current_canvas_->drawRect(gfx::RectFToSkRect(visible_quad_vertex_rect),
                            current_paint_);
}

void SkiaRenderer::DrawTextureQuad(const DrawingFrame* frame,
                                   const TextureDrawQuad* quad) {
  if (!IsSoftwareResource(quad->resource_id())) {
    DrawUnsupportedQuad(frame, quad);
    return;
  }

  // TODO(skaslev): Add support for non-premultiplied alpha.
  ResourceProvider::ScopedReadLockSkImage lock(resource_provider_,
                                               quad->resource_id());
  const SkImage* image = lock.sk_image();
  if (!image)
    return;
  gfx::RectF uv_rect = gfx::ScaleRect(
      gfx::BoundingRect(quad->uv_top_left, quad->uv_bottom_right),
      image->width(), image->height());
  gfx::RectF visible_uv_rect = MathUtil::ScaleRectProportional(
      uv_rect, gfx::RectF(quad->rect), gfx::RectF(quad->visible_rect));
  SkRect sk_uv_rect = gfx::RectFToSkRect(visible_uv_rect);
  gfx::RectF visible_quad_vertex_rect = MathUtil::ScaleRectProportional(
      QuadVertexRect(), gfx::RectF(quad->rect), gfx::RectF(quad->visible_rect));
  SkRect quad_rect = gfx::RectFToSkRect(visible_quad_vertex_rect);

  if (quad->y_flipped)
    current_canvas_->scale(1, -1);

  bool blend_background =
      quad->background_color != SK_ColorTRANSPARENT && !image->isOpaque();
  bool needs_layer = blend_background && (current_paint_.getAlpha() != 0xFF);
  if (needs_layer) {
    current_canvas_->saveLayerAlpha(&quad_rect, current_paint_.getAlpha());
    current_paint_.setAlpha(0xFF);
  }
  if (blend_background) {
    SkPaint background_paint;
    background_paint.setColor(quad->background_color);
    current_canvas_->drawRect(quad_rect, background_paint);
  }
  current_paint_.setFilterQuality(
      quad->nearest_neighbor ? kNone_SkFilterQuality : kLow_SkFilterQuality);
  current_canvas_->drawImageRect(image, sk_uv_rect, quad_rect, &current_paint_);
  if (needs_layer)
    current_canvas_->restore();
}

void SkiaRenderer::DrawTileQuad(const DrawingFrame* frame,
                                const TileDrawQuad* quad) {
  // |resource_provider_| can be NULL in resourceless software draws, which
  // should never produce tile quads in the first place.
  DCHECK(resource_provider_);
  DCHECK(IsSoftwareResource(quad->resource_id()));

  ResourceProvider::ScopedReadLockSkImage lock(resource_provider_,
                                               quad->resource_id());
  if (!lock.sk_image())
    return;
  gfx::RectF visible_tex_coord_rect = MathUtil::ScaleRectProportional(
      quad->tex_coord_rect, gfx::RectF(quad->rect),
      gfx::RectF(quad->visible_rect));
  gfx::RectF visible_quad_vertex_rect = MathUtil::ScaleRectProportional(
      QuadVertexRect(), gfx::RectF(quad->rect), gfx::RectF(quad->visible_rect));

  SkRect uv_rect = gfx::RectFToSkRect(visible_tex_coord_rect);
  current_paint_.setFilterQuality(
      quad->nearest_neighbor ? kNone_SkFilterQuality : kLow_SkFilterQuality);
  current_canvas_->drawImageRect(lock.sk_image(), uv_rect,
                                 gfx::RectFToSkRect(visible_quad_vertex_rect),
                                 &current_paint_);
}

void SkiaRenderer::DrawRenderPassQuad(const DrawingFrame* frame,
                                      const RenderPassDrawQuad* quad) {
#if 0
  ScopedResource* content_texture =
      render_pass_textures_[quad->render_pass_id].get();
  DCHECK(content_texture);
  DCHECK(content_texture->id());
  DCHECK(IsSoftwareResource(content_texture->id()));

  ResourceProvider::ScopedReadLockGL lock(resource_provider_,
                                                content_texture->id());
  if (!lock.valid())
    return;

  SkRect dest_rect = gfx::RectFToSkRect(QuadVertexRect());
  SkRect dest_visible_rect = gfx::RectFToSkRect(
      MathUtil::ScaleRectProportional(QuadVertexRect(), gfx::RectF(quad->rect),
                                      gfx::RectF(quad->visible_rect)));
  SkRect content_rect = SkRect::MakeWH(quad->rect.width(), quad->rect.height());

  const SkBitmap* content = lock.sk_bitmap();

  sk_sp<SkImage> filter_image;
  if (!quad->filters.IsEmpty()) {
    sk_sp<SkImageFilter> filter = RenderSurfaceFilters::BuildImageFilter(
        quad->filters, gfx::SizeF(content_texture->size()));
    if (filter) {
      SkIRect result_rect;
      // TODO(ajuma): Apply the filter in the same pass as the content where
      // possible (e.g. when there's no origin offset). See crbug.com/308201.
      filter_image =
          ApplyImageFilter(filter.get(), quad, *content, &result_rect);
      if (result_rect.isEmpty()) {
        return;
      }
      if (filter_image) {
        gfx::RectF rect = gfx::SkRectToRectF(SkRect::Make(result_rect));
        dest_rect = dest_visible_rect =
            gfx::RectFToSkRect(MathUtil::ScaleRectProportional(
                QuadVertexRect(), gfx::RectF(quad->rect), rect));
        content_rect =
            SkRect::MakeWH(result_rect.width(), result_rect.height());
      }
    }
  }

  SkMatrix content_mat;
  content_mat.setRectToRect(content_rect, dest_rect,
                            SkMatrix::kFill_ScaleToFit);

  sk_sp<SkShader> shader;
  if (!filter_image) {
    shader =
        SkShader::MakeBitmapShader(*content, SkShader::kClamp_TileMode,
                                   SkShader::kClamp_TileMode, &content_mat);
  } else {
    shader = filter_image->makeShader(SkShader::kClamp_TileMode,
                                      SkShader::kClamp_TileMode, &content_mat);
  }

  std::unique_ptr<ResourceProvider::ScopedReadLockGL> mask_lock;
  if (quad->mask_resource_id()) {
    mask_lock = std::unique_ptr<ResourceProvider::ScopedReadLockGL>(
        new ResourceProvider::ScopedReadLockGL(resource_provider_,
                                                     quad->mask_resource_id()));

    if (!mask_lock->valid())
      return;

    const SkBitmap* mask = mask_lock->sk_bitmap();

    // Scale normalized uv rect into absolute texel coordinates.
    SkRect mask_rect =
        gfx::RectFToSkRect(gfx::ScaleRect(quad->MaskUVRect(),
                                          quad->mask_texture_size.width(),
                                          quad->mask_texture_size.height()));

    SkMatrix mask_mat;
    mask_mat.setRectToRect(mask_rect, dest_rect, SkMatrix::kFill_ScaleToFit);

    SkPaint mask_paint;
    mask_paint.setShader(
        SkShader::MakeBitmapShader(*mask, SkShader::kClamp_TileMode,
                                   SkShader::kClamp_TileMode, &mask_mat));

    SkLayerRasterizer::Builder builder;
    builder.addLayer(mask_paint);

    current_paint_.setRasterizer(builder.detach());
  }

  // If we have a background filter shader, render its results first.
  sk_sp<SkShader> background_filter_shader =
      GetBackgroundFilterShader(frame, quad, SkShader::kClamp_TileMode);
  if (background_filter_shader) {
    SkPaint paint;
    paint.setShader(std::move(background_filter_shader));
    paint.setRasterizer(sk_ref_sp(current_paint_.getRasterizer()));
    current_canvas_->drawRect(dest_visible_rect, paint);
  }
  current_paint_.setShader(std::move(shader));
  current_canvas_->drawRect(dest_visible_rect, current_paint_);
#endif
}

void SkiaRenderer::DrawUnsupportedQuad(const DrawingFrame* frame,
                                       const DrawQuad* quad) {
#ifdef NDEBUG
  current_paint_.setColor(SK_ColorWHITE);
#else
  current_paint_.setColor(SK_ColorMAGENTA);
#endif
  current_paint_.setAlpha(quad->shared_quad_state->opacity * 255);
  current_canvas_->drawRect(gfx::RectFToSkRect(QuadVertexRect()),
                            current_paint_);
}

void SkiaRenderer::CopyCurrentRenderPassToBitmap(
    DrawingFrame* frame,
    std::unique_ptr<CopyOutputRequest> request) {
#if 0
  gfx::Rect copy_rect = frame->current_render_pass->output_rect;
  if (request->has_area())
    copy_rect.Intersect(request->area());
  gfx::Rect window_copy_rect = MoveFromDrawToWindowSpace(frame, copy_rect);

  std::unique_ptr<SkBitmap> bitmap(new SkBitmap);
  bitmap->setInfo(SkImageInfo::MakeN32Premul(window_copy_rect.width(),
                                             window_copy_rect.height()));
  current_canvas_->readPixels(
      bitmap.get(), window_copy_rect.x(), window_copy_rect.y());

  request->SendBitmapResult(std::move(bitmap));
#endif
}

void SkiaRenderer::DidChangeVisibility() {
  if (visible_)
    output_surface_->EnsureBackbuffer();
  else
    output_surface_->DiscardBackbuffer();
}

void SkiaRenderer::FinishDrawingQuadList() {
  current_canvas_->flush();
}

bool SkiaRenderer::ShouldApplyBackgroundFilters(
    const RenderPassDrawQuad* quad) const {
  if (quad->background_filters.IsEmpty())
    return false;

  // TODO(hendrikw): Look into allowing background filters to see pixels from
  // other render targets.  See crbug.com/314867.

  return true;
}

// If non-null, auto_bounds will be filled with the automatically-computed
// destination bounds. If null, the output will be the same size as the
// input bitmap.
sk_sp<SkImage> SkiaRenderer::ApplyImageFilter(SkImageFilter* filter,
                                              const RenderPassDrawQuad* quad,
                                              const SkBitmap& to_filter,
                                              SkIRect* auto_bounds) const {
  // if (!filter)
  //   return nullptr;

  // SkMatrix local_matrix;
  // local_matrix.setTranslate(quad->filters_origin.x(), quad->filters_origin.y());
  // local_matrix.postScale(quad->filters_scale.x(), quad->filters_scale.y());
  // SkIRect dst_rect;
  // if (auto_bounds) {
  //   dst_rect =
  //       filter->filterBounds(gfx::RectToSkIRect(quad->rect), local_matrix,
  //                            SkImageFilter::kForward_MapDirection);
  //   *auto_bounds = dst_rect;
  // } else {
  //   dst_rect = to_filter.bounds();
  // }

  // SkImageInfo dst_info =
  //     SkImageInfo::MakeN32Premul(dst_rect.width(), dst_rect.height());
  // sk_sp<SkSurface> surface = SkSurface::MakeRaster(dst_info);

  // if (!surface) {
  //   return nullptr;
  // }

  // SkPaint paint;
  // // Treat subnormal float values as zero for performance.
  // ScopedSubnormalFloatDisabler disabler;
  // paint.setImageFilter(filter->makeWithLocalMatrix(local_matrix));
  // surface->getCanvas()->translate(-dst_rect.x(), -dst_rect.y());
  // surface->getCanvas()->drawBitmap(to_filter, quad->rect.x(), quad->rect.y(),
  //                                  &paint);

  // return surface->makeImageSnapshot();
  return nullptr;
}

SkBitmap SkiaRenderer::GetBackdropBitmap(const gfx::Rect& bounding_rect) const {
  SkBitmap bitmap;
  bitmap.setInfo(SkImageInfo::MakeN32Premul(bounding_rect.width(),
                                            bounding_rect.height()));
  current_canvas_->readPixels(&bitmap, bounding_rect.x(), bounding_rect.y());
  return bitmap;
}

gfx::Rect SkiaRenderer::GetBackdropBoundingBoxForRenderPassQuad(
    const DrawingFrame* frame,
    const RenderPassDrawQuad* quad,
    const gfx::Transform& contents_device_transform) const {
  DCHECK(ShouldApplyBackgroundFilters(quad));
  gfx::Rect backdrop_rect = gfx::ToEnclosingRect(
      MathUtil::MapClippedRect(contents_device_transform, QuadVertexRect()));

  SkMatrix matrix;
  matrix.setScale(quad->filters_scale.x(), quad->filters_scale.y());
  backdrop_rect =
      quad->background_filters.MapRectReverse(backdrop_rect, matrix);

  backdrop_rect.Intersect(MoveFromDrawToWindowSpace(
      frame, frame->current_render_pass->output_rect));

  return backdrop_rect;
}

sk_sp<SkShader> SkiaRenderer::GetBackgroundFilterShader(
    const DrawingFrame* frame,
    const RenderPassDrawQuad* quad,
    SkShader::TileMode content_tile_mode) const {
#if 0
  if (!ShouldApplyBackgroundFilters(quad))
    return nullptr;

  gfx::Transform quad_rect_matrix;
  QuadRectTransform(&quad_rect_matrix,
                    quad->shared_quad_state->quad_to_target_transform,
                    gfx::RectF(quad->rect));
  gfx::Transform contents_device_transform =
      frame->window_matrix * frame->projection_matrix * quad_rect_matrix;
  contents_device_transform.FlattenTo2d();

  gfx::Rect backdrop_rect = GetBackdropBoundingBoxForRenderPassQuad(
      frame, quad, contents_device_transform);

  // Figure out the transformations to move it back to pixel space.
  gfx::Transform contents_device_transform_inverse;
  if (!contents_device_transform.GetInverse(&contents_device_transform_inverse))
    return nullptr;

  SkMatrix filter_backdrop_transform =
      contents_device_transform_inverse.matrix();
  filter_backdrop_transform.preTranslate(backdrop_rect.x(), backdrop_rect.y());

  // Draw what's behind, and apply the filter to it.
  SkBitmap backdrop_bitmap = GetBackdropBitmap(backdrop_rect);

  sk_sp<SkImageFilter> filter = RenderSurfaceFilters::BuildImageFilter(
      quad->background_filters,
      gfx::SizeF(backdrop_bitmap.width(), backdrop_bitmap.height()));
  sk_sp<SkImage> filter_backdrop_image =
      ApplyImageFilter(filter.get(), quad, backdrop_bitmap, nullptr);

  if (!filter_backdrop_image)
    return nullptr;

  return filter_backdrop_image->makeShader(content_tile_mode, content_tile_mode,
                                           &filter_backdrop_transform);
#else
  return nullptr;
#endif
}

}  // namespace cc
