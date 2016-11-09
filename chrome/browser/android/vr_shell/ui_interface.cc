// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/ui_interface.h"

#include "chrome/browser/ui/webui/vr_shell/vr_shell_ui_message_handler.h"

namespace vr_shell {

UiInterface::UiInterface() {
  SetMode(Mode::STANDARD);
}

UiInterface::~UiInterface() {}

void UiInterface::SetUiCommandHandler(UiCommandHandler* handler) {
  handler_ = handler;
}

void UiInterface::SetMode(Mode mode) {
  updates_.SetInteger("mode", static_cast<int>(mode));
  FlushUpdates();
}

void UiInterface::SetSecureOrigin(bool secure) {
  updates_.SetBoolean("secureOrigin", static_cast<int>(secure));
  FlushUpdates();
}

void UiInterface::OnDomContentsLoaded() {
  loaded_ = true;
  FlushUpdates();
}

void UiInterface::FlushUpdates() {
  if (loaded_ && handler_) {
    handler_->SendCommandToUi(updates_);
    updates_.Clear();
  }
}

}  // namespace vr_shell
