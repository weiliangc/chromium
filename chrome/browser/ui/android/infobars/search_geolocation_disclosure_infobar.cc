// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/search_geolocation_disclosure_infobar.h"

#include "base/android/jni_string.h"
#include "chrome/browser/android/search_geolocation_disclosure_infobar_delegate.h"
#include "jni/SearchGeolocationDisclosureInfoBar_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

SearchGeolocationDisclosureInfoBar::SearchGeolocationDisclosureInfoBar(
    std::unique_ptr<SearchGeolocationDisclosureInfoBarDelegate> delegate)
    : InfoBarAndroid(std::move(delegate)) {}

SearchGeolocationDisclosureInfoBar::~SearchGeolocationDisclosureInfoBar() {
}

ScopedJavaLocalRef<jobject>
SearchGeolocationDisclosureInfoBar::CreateRenderInfoBar(JNIEnv* env) {
  ScopedJavaLocalRef<jstring> message_text =
      base::android::ConvertUTF16ToJavaString(
          env, GetDelegate()->GetMessageText());
  return Java_SearchGeolocationDisclosureInfoBar_show(
      env, GetEnumeratedIconId(), message_text);
}

void SearchGeolocationDisclosureInfoBar::ProcessButton(int action) {
  if (!owner())
    return;  // We're closing; don't call anything, it might access the owner.

  RemoveSelf();
}

SearchGeolocationDisclosureInfoBarDelegate*
SearchGeolocationDisclosureInfoBar::GetDelegate() {
  return static_cast<SearchGeolocationDisclosureInfoBarDelegate*>(delegate());
}
