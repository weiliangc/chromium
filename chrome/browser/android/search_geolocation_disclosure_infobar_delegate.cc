// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/search_geolocation_disclosure_infobar_delegate.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/ui/android/infobars/search_geolocation_disclosure_infobar.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

SearchGeolocationDisclosureInfoBarDelegate::
    ~SearchGeolocationDisclosureInfoBarDelegate() {}

// static
void SearchGeolocationDisclosureInfoBarDelegate::Create(
    content::WebContents* web_contents) {
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);
  // Add the new delegate.
  infobar_service->AddInfoBar(
      base::MakeUnique<SearchGeolocationDisclosureInfoBar>(
          base::WrapUnique(new SearchGeolocationDisclosureInfoBarDelegate())));
}

base::string16 SearchGeolocationDisclosureInfoBarDelegate::GetMessageText()
    const {
  return l10n_util::GetStringUTF16(
      IDS_SEARCH_GEOLOCATION_DISCLOSURE_INFOBAR_TEXT);
};

SearchGeolocationDisclosureInfoBarDelegate::
    SearchGeolocationDisclosureInfoBarDelegate()
    : infobars::InfoBarDelegate() {}

infobars::InfoBarDelegate::Type
SearchGeolocationDisclosureInfoBarDelegate::GetInfoBarType() const {
  return PAGE_ACTION_TYPE;
}

infobars::InfoBarDelegate::InfoBarIdentifier
SearchGeolocationDisclosureInfoBarDelegate::GetIdentifier() const {
  return SEARCH_GEOLOCATION_DISCLOSURE_INFOBAR_DELEGATE;
}

int SearchGeolocationDisclosureInfoBarDelegate::GetIconId() const {
  return IDR_ANDROID_INFOBAR_GEOLOCATION;
}
