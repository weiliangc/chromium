// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/synchronous_compositor_observer.h"

#include <map>

#include "base/lazy_instance.h"
#include "base/stl_util.h"
#include "content/browser/android/synchronous_compositor_host.h"
#include "content/browser/bad_message.h"
#include "content/common/android/sync_compositor_messages.h"
#include "content/public/browser/render_process_host.h"
#include "ui/android/window_android.h"

namespace content {

SynchronousCompositorObserver::SynchronousCompositorObserver(int process_id)
    : BrowserMessageFilter(SyncCompositorMsgStart),
      render_process_host_(RenderProcessHost::FromID(process_id)),
      window_android_in_vsync_(nullptr) {
  DCHECK(render_process_host_);
}

SynchronousCompositorObserver::~SynchronousCompositorObserver() {
  DCHECK(compositor_host_pending_renderer_state_.empty());
  // TODO(boliu): signal pending frames.
}

void SynchronousCompositorObserver::SyncStateAfterVSync(
    ui::WindowAndroid* window_android,
    SynchronousCompositorHost* compositor_host) {
  DCHECK(!window_android_in_vsync_ ||
         window_android_in_vsync_ == window_android)
      << !!window_android_in_vsync_;
  DCHECK(compositor_host);
  DCHECK(!base::ContainsValue(compositor_host_pending_renderer_state_,
                              compositor_host));
  compositor_host_pending_renderer_state_.push_back(compositor_host);
  if (window_android_in_vsync_)
    return;
  window_android_in_vsync_ = window_android;
  window_android_in_vsync_->AddObserver(this);
}

bool SynchronousCompositorObserver::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SynchronousCompositorObserver, message)
    IPC_MESSAGE_HANDLER_GENERIC(SyncCompositorHostMsg_ReturnFrame,
                                ReceiveFrame(message))
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

bool SynchronousCompositorObserver::ReceiveFrame(const IPC::Message& message) {
  SyncCompositorHostMsg_ReturnFrame::Param param;
  if (!SyncCompositorHostMsg_ReturnFrame::Read(&message, &param))
    return false;

  int routing_id = message.routing_id();
  scoped_refptr<SynchronousCompositor::FrameFuture> future;
  {
    base::AutoLock lock(future_map_lock_);
    auto itr = future_map_.find(routing_id);
    if (itr == future_map_.end()) {
      bad_message::ReceivedBadMessage(render_process_host_,
                                      bad_message::SCO_INVALID_ARGUMENT);
      return true;
    }
    future = std::move(itr->second);
    DCHECK(future);
    future_map_.erase(itr);
  }

  auto frame_ptr = base::MakeUnique<SynchronousCompositor::Frame>();
  frame_ptr->compositor_frame_sink_id = std::get<0>(param);
  cc::CompositorFrame& compositor_frame = std::get<1>(param);
  if (compositor_frame.delegated_frame_data) {
    frame_ptr->frame.reset(new cc::CompositorFrame);
    *frame_ptr->frame = std::move(compositor_frame);
  }
  future->setFrame(std::move(frame_ptr));
  // TODO(boliu): Post metadata back to UI thread.
  return true;
}

void SynchronousCompositorObserver::SetFrameFuture(
    int routing_id,
    scoped_refptr<SynchronousCompositor::FrameFuture> frame_future) {
  // TODO(boliu): Need a sequenced id, to queue previous frames.
  DCHECK(frame_future);
  base::AutoLock lock(future_map_lock_);
  future_map_[routing_id] = std::move(frame_future);
}

void SynchronousCompositorObserver::OnCompositingDidCommit() {
  NOTREACHED();
}

void SynchronousCompositorObserver::OnRootWindowVisibilityChanged(
    bool visible) {
  NOTREACHED();
}

void SynchronousCompositorObserver::OnAttachCompositor() {
  NOTREACHED();
}

void SynchronousCompositorObserver::OnDetachCompositor() {
  NOTREACHED();
}

void SynchronousCompositorObserver::OnVSync(base::TimeTicks frame_time,
                                            base::TimeDelta vsync_period) {
  // This is called after DidSendBeginFrame for SynchronousCompositorHosts
  // belonging to this WindowAndroid, since this is added as an Observer after
  // the observer iteration has started.
  DCHECK(window_android_in_vsync_);
  window_android_in_vsync_->RemoveObserver(this);
  window_android_in_vsync_ = nullptr;

  std::vector<int> routing_ids;
  routing_ids.reserve(compositor_host_pending_renderer_state_.size());
  for (const auto host : compositor_host_pending_renderer_state_)
    routing_ids.push_back(host->routing_id());

  std::vector<SyncCompositorCommonRendererParams> params;
  params.reserve(compositor_host_pending_renderer_state_.size());

  if (!render_process_host_->Send(
          new SyncCompositorMsg_SynchronizeRendererState(routing_ids,
                                                         &params))) {
    return;
  }

  if (compositor_host_pending_renderer_state_.size() != params.size()) {
    bad_message::ReceivedBadMessage(render_process_host_,
                                    bad_message::SCO_INVALID_ARGUMENT);
    return;
  }

  for (size_t i = 0; i < compositor_host_pending_renderer_state_.size(); ++i) {
    compositor_host_pending_renderer_state_[i]->ProcessCommonParams(params[i]);
  }
  compositor_host_pending_renderer_state_.clear();
}

void SynchronousCompositorObserver::OnActivityStopped() {
  NOTREACHED();
}

void SynchronousCompositorObserver::OnActivityStarted() {
  NOTREACHED();
}

}  // namespace content
