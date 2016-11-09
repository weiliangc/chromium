// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/tray/tray_popup_utils.h"

#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/system/tray/fixed_sized_image_view.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_popup_label_button.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Creates a layout manager that positions Views vertically. The Views will be
// stretched horizontally and centered vertically.
std::unique_ptr<views::LayoutManager> CreateDefaultCenterLayoutManager() {
  // TODO(bruthig): Use constants instead of magic numbers.
  views::BoxLayout* box_layout =
      new views::BoxLayout(views::BoxLayout::kVertical, 4, 8, 4);
  box_layout->set_main_axis_alignment(
      views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_STRETCH);
  return std::unique_ptr<views::LayoutManager>(box_layout);
}

// Creates a layout manager that positions Views horizontally. The Views will be
// centered along the horizontal and vertical axis.
std::unique_ptr<views::LayoutManager> CreateDefaultEndsLayoutManager() {
  views::BoxLayout* box_layout =
      new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0);
  box_layout->set_main_axis_alignment(
      views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_STRETCH);
  return std::unique_ptr<views::LayoutManager>(box_layout);
}

}  // namespace

TriView* TrayPopupUtils::CreateDefaultRowView() {
  TriView* tri_view = new TriView(0 /* padding_between_items */);

  tri_view->SetInsets(
      gfx::Insets(0, GetTrayConstant(TRAY_POPUP_ITEM_LEFT_INSET), 0,
                  GetTrayConstant(TRAY_POPUP_ITEM_RIGHT_INSET)));
  tri_view->SetMinCrossAxisSize(GetTrayConstant(TRAY_POPUP_ITEM_HEIGHT));

  ConfigureDefaultLayout(tri_view, TriView::Container::START);
  ConfigureDefaultLayout(tri_view, TriView::Container::CENTER);
  ConfigureDefaultLayout(tri_view, TriView::Container::END);

  return tri_view;
}

std::unique_ptr<views::LayoutManager> TrayPopupUtils::CreateLayoutManager(
    TriView::Container container) {
  switch (container) {
    case TriView::Container::START:
    case TriView::Container::END:
      return CreateDefaultEndsLayoutManager();
    case TriView::Container::CENTER:
      return CreateDefaultCenterLayoutManager();
  }
  // Required by some compilers.
  NOTREACHED();
  return nullptr;
}

views::Label* TrayPopupUtils::CreateDefaultLabel() {
  views::Label* label = new views::Label();
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetBorder(
      views::Border::CreateEmptyBorder(0, kTrayPopupLabelHorizontalPadding, 0,
                                       kTrayPopupLabelHorizontalPadding));
  return label;
}

views::ImageView* TrayPopupUtils::CreateMainImageView() {
  return new FixedSizedImageView(
      GetTrayConstant(TRAY_POPUP_ITEM_MAIN_IMAGE_CONTAINER_WIDTH),
      GetTrayConstant(TRAY_POPUP_ITEM_HEIGHT));
}

views::ImageView* TrayPopupUtils::CreateMoreImageView() {
  views::ImageView* image = new FixedSizedImageView(
      GetTrayConstant(TRAY_POPUP_ITEM_MORE_IMAGE_CONTAINER_WIDTH),
      GetTrayConstant(TRAY_POPUP_ITEM_HEIGHT));
  image->EnableCanvasFlippingForRTLUI(true);
  return image;
}

void TrayPopupUtils::ConfigureDefaultLayout(TriView* tri_view,
                                            TriView::Container container) {
  switch (container) {
    case TriView::Container::START:
      tri_view->SetMinSize(
          TriView::Container::START,
          gfx::Size(GetTrayConstant(TRAY_POPUP_ITEM_MIN_START_WIDTH), 0));
      break;
    case TriView::Container::CENTER:
      tri_view->SetFlexForContainer(TriView::Container::CENTER, 1.f);
      break;
    case TriView::Container::END:
      tri_view->SetMinSize(
          TriView::Container::END,
          gfx::Size(GetTrayConstant(TRAY_POPUP_ITEM_MIN_END_WIDTH), 0));
      break;
  }

  tri_view->SetContainerLayout(container, CreateLayoutManager(container));
}

}  // namespace ash
