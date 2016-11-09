// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/arc_support_host.h"

#include <string>

#include "ash/common/system/chromeos/devicetype_utils.h"
#include "base/i18n/timezone.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/arc/arc_auth_service.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/user_manager/known_user.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/display/screen.h"

namespace {
constexpr char kAction[] = "action";
constexpr char kArcManaged[] = "arcManaged";
constexpr char kData[] = "data";
constexpr char kDeviceId[] = "deviceId";
constexpr char kActionInitialize[] = "initialize";
constexpr char kActionSetMetricsMode[] = "setMetricsMode";
constexpr char kActionBackupAndRestoreMode[] = "setBackupAndRestoreMode";
constexpr char kActionLocationServiceMode[] = "setLocationServiceMode";
constexpr char kActionSetWindowBounds[] = "setWindowBounds";
constexpr char kActionCloseWindow[] = "closeWindow";

// Action to show a page. The message should have "page" field, which is one of
// IDs for section div elements.
constexpr char kActionShowPage[] = "showPage";
constexpr char kPage[] = "page";

// Action to show the error page. The message should have "errorMessage",
// which is a localized error text, and "shouldShowSendFeedback" boolean value.
constexpr char kActionShowErrorPage[] = "showErrorPage";
constexpr char kErrorMessage[] = "errorMessage";
constexpr char kShouldShowSendFeedback[] = "shouldShowSendFeedback";

// The preference update should have those two fields.
constexpr char kEnabled[] = "enabled";
constexpr char kManaged[] = "managed";

// The JSON data sent from the extension should have at least "event" field.
// Each event data is defined below.
// The key of the event type.
constexpr char kEvent[] = "event";

// "onWindowClosed" is fired when the extension window is closed.
// No data will be provided.
constexpr char kEventOnWindowClosed[] = "onWindowClosed";

// "onAuthSucceeded" is fired when successfully done to LSO authorization in
// extension.
// The auth token is passed via "code" field.
constexpr char kEventOnAuthSucceeded[] = "onAuthSucceeded";
constexpr char kCode[] = "code";

// "onAgree" is fired when a user clicks "Agree" button.
// The message should have the following three fields:
// - isMetricsEnabled
// - isBackupRestoreEnabled
// - isLocationServiceEnabled
constexpr char kEventOnAgreed[] = "onAgreed";
constexpr char kIsMetricsEnabled[] = "isMetricsEnabled";
constexpr char kIsBackupRestoreEnabled[] = "isBackupRestoreEnabled";
constexpr char kIsLocationServiceEnabled[] = "isLocationServiceEnabled";

// "onSendFeedbackClicked" is fired when a user clicks "Send Feedback" button.
constexpr char kEventOnSendFeedbackClicked[] = "onSendFeedbackClicked";

}  // namespace

// static
const char ArcSupportHost::kHostAppId[] = "cnbgggchhmkkdmeppjobngjoejnihlei";

// static
const char ArcSupportHost::kStorageId[] = "arc_support";

ArcSupportHost::ArcSupportHost() = default;

ArcSupportHost::~ArcSupportHost() {
  if (message_host_)
    DisconnectMessageHost();
}

void ArcSupportHost::AddObserver(Observer* observer) {
  DCHECK(!observer_);
  observer_ = observer;
}

void ArcSupportHost::Close() {
  if (!message_host_) {
    VLOG(2) << "ArcSupportHost::Close() is called "
            << "but message_host_ is not available.";
    return;
  }

  base::DictionaryValue message;
  message.SetString(kAction, kActionCloseWindow);
  message_host_->SendMessage(message);

  // Disconnect immediately, so that onWindowClosed event will not be
  // delivered to here.
  DisconnectMessageHost();
}

void ArcSupportHost::ShowPage(UIPage page, const base::string16& status) {
  if (!message_host_) {
    VLOG(2) << "ArcSupportHost::ShowPage() is called "
            << "but message_host_ is not available.";
    return;
  }

  base::DictionaryValue message;
  if (page == UIPage::ERROR || page == UIPage::ERROR_WITH_FEEDBACK) {
    message.SetString(kAction, kActionShowErrorPage);
    message.SetString(kErrorMessage, status);
    message.SetBoolean(kShouldShowSendFeedback,
                       page == UIPage::ERROR_WITH_FEEDBACK);
  } else {
    message.SetString(kAction, kActionShowPage);
    switch (page) {
      case UIPage::NO_PAGE:
        message.SetString(kPage, "none");
        break;
      case UIPage::TERMS:
        message.SetString(kPage, "terms");
        break;
      case UIPage::LSO_PROGRESS:
        message.SetString(kPage, "lso-loading");
        break;
      // Skip LSO. LSO and LSO_LOADING should be merged well.
      // TODO(hidehiko): Do it.
      case UIPage::START_PROGRESS:
        message.SetString(kPage, "arc-loading");
        break;
      default:
        NOTREACHED();
        return;
    }
  }
  message_host_->SendMessage(message);
}

void ArcSupportHost::SetMetricsPreferenceCheckbox(bool is_enabled,
                                                  bool is_managed) {
  metrics_checkbox_ = PreferenceCheckboxData(is_enabled, is_managed);
  SendPreferenceCheckboxUpdate(kActionSetMetricsMode, metrics_checkbox_);
}

void ArcSupportHost::SetBackupAndRestorePreferenceCheckbox(bool is_enabled,
                                                           bool is_managed) {
  backup_and_restore_checkbox_ = PreferenceCheckboxData(is_enabled, is_managed);
  SendPreferenceCheckboxUpdate(kActionBackupAndRestoreMode,
                               backup_and_restore_checkbox_);
}

void ArcSupportHost::SetLocationServicesPreferenceCheckbox(bool is_enabled,
                                                           bool is_managed) {
  location_services_checkbox_ = PreferenceCheckboxData(is_enabled, is_managed);
  SendPreferenceCheckboxUpdate(kActionLocationServiceMode,
                               location_services_checkbox_);
}

void ArcSupportHost::SendPreferenceCheckboxUpdate(
    const std::string& action_name,
    const PreferenceCheckboxData& data) {
  if (!message_host_)
    return;

  base::DictionaryValue message;
  message.SetString(kAction, action_name);
  message.SetBoolean(kEnabled, data.is_enabled);
  message.SetBoolean(kManaged, data.is_managed);
  message_host_->SendMessage(message);
}

void ArcSupportHost::SetMessageHost(arc::ArcSupportMessageHost* message_host) {
  if (message_host_ == message_host)
    return;

  if (message_host_)
    DisconnectMessageHost();
  message_host_ = message_host;
  message_host_->SetObserver(this);
  display::Screen::GetScreen()->AddObserver(this);

  if (!Initialize()) {
    Close();
    return;
  }

  SendPreferenceCheckboxUpdate(kActionSetMetricsMode, metrics_checkbox_);
  SendPreferenceCheckboxUpdate(kActionBackupAndRestoreMode,
                               backup_and_restore_checkbox_);
  SendPreferenceCheckboxUpdate(kActionLocationServiceMode,
                               location_services_checkbox_);

  arc::ArcAuthService* arc_auth_service = arc::ArcAuthService::Get();
  DCHECK(arc_auth_service);
  ShowPage(arc_auth_service->ui_page(), arc_auth_service->ui_page_status());
}

void ArcSupportHost::UnsetMessageHost(
    arc::ArcSupportMessageHost* message_host) {
  if (message_host_ != message_host)
    return;
  DisconnectMessageHost();
}

void ArcSupportHost::DisconnectMessageHost() {
  DCHECK(message_host_);
  display::Screen::GetScreen()->RemoveObserver(this);
  message_host_->SetObserver(nullptr);
  message_host_ = nullptr;
}

bool ArcSupportHost::Initialize() {
  DCHECK(message_host_);
  arc::ArcAuthService* arc_auth_service = arc::ArcAuthService::Get();
  if (!arc_auth_service->IsAllowed())
    return false;

  std::unique_ptr<base::DictionaryValue> loadtime_data(
      new base::DictionaryValue());
  base::string16 device_name = ash::GetChromeOSDeviceName();
  loadtime_data->SetString(
      "greetingHeader",
      l10n_util::GetStringFUTF16(IDS_ARC_OPT_IN_DIALOG_HEADER, device_name));
  loadtime_data->SetString("greetingDescription",
                           l10n_util::GetStringFUTF16(
                               IDS_ARC_OPT_IN_DIALOG_DESCRIPTION, device_name));
  loadtime_data->SetString(
      "buttonAgree",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_DIALOG_BUTTON_AGREE));
  loadtime_data->SetString(
      "buttonCancel",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_DIALOG_BUTTON_CANCEL));
  loadtime_data->SetString(
      "buttonSendFeedback",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_DIALOG_BUTTON_SEND_FEEDBACK));
  loadtime_data->SetString(
      "buttonRetry",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_DIALOG_BUTTON_RETRY));
  loadtime_data->SetString(
      "progressLsoLoading",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_DIALOG_PROGRESS_LSO));
  loadtime_data->SetString(
      "progressAndroidLoading",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_DIALOG_PROGRESS_ANDROID));
  loadtime_data->SetString(
      "authorizationFailed",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_DIALOG_AUTHORIZATION_FAILED));
  loadtime_data->SetString(
      "termsOfService",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_DIALOG_TERMS_OF_SERVICE));
  loadtime_data->SetString(
      "textMetricsEnabled",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_DIALOG_METRICS_ENABLED));
  loadtime_data->SetString(
      "textMetricsDisabled",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_DIALOG_METRICS_DISABLED));
  loadtime_data->SetString(
      "textMetricsManagedEnabled",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_DIALOG_METRICS_MANAGED_ENABLED));
  loadtime_data->SetString("textMetricsManagedDisabled",
                           l10n_util::GetStringUTF16(
                               IDS_ARC_OPT_IN_DIALOG_METRICS_MANAGED_DISABLED));
  loadtime_data->SetString(
      "textBackupRestore",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_DIALOG_BACKUP_RESTORE));
  loadtime_data->SetString(
      "textLocationService",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_LOCATION_SETTING));
  loadtime_data->SetString(
      "serverError",
      l10n_util::GetStringUTF16(IDS_ARC_SERVER_COMMUNICATION_ERROR));
  loadtime_data->SetString(
      "controlledByPolicy",
      l10n_util::GetStringUTF16(IDS_OPTIONS_CONTROLLED_SETTING_POLICY));
  loadtime_data->SetString(
      "learnMoreStatistics",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_LEARN_MORE_STATISTICS));
  loadtime_data->SetString(
      "learnMoreBackupAndRestore",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_LEARN_MORE_BACKUP_AND_RESTORE));
  loadtime_data->SetString(
      "learnMoreLocationServices",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_LEARN_MORE_LOCATION_SERVICES));
  loadtime_data->SetString(
      "overlayClose",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_LEARN_MORE_CLOSE));
  loadtime_data->SetString(
      "privacyPolicyLink",
      l10n_util::GetStringUTF16(IDS_ARC_OPT_IN_PRIVACY_POLICY_LINK));

  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  const std::string& country_code = base::CountryCodeForCurrentTimezone();
  loadtime_data->SetString("countryCode", country_code);
  loadtime_data->SetBoolean(kArcManaged, arc_auth_service->IsArcManaged());

  webui::SetLoadTimeDataDefaults(app_locale, loadtime_data.get());
  DCHECK(arc_auth_service);
  const std::string device_id = user_manager::known_user::GetDeviceId(
      multi_user_util::GetAccountIdFromProfile(arc_auth_service->profile()));
  DCHECK(!device_id.empty());
  loadtime_data->SetBoolean(
      "isOwnerProfile",
      chromeos::ProfileHelper::IsOwnerProfile(arc_auth_service->profile()));

  base::DictionaryValue message;
  message.SetString(kAction, kActionInitialize);
  message.Set(kData, std::move(loadtime_data));
  message.SetString(kDeviceId, device_id);
  message_host_->SendMessage(message);
  return true;
}

void ArcSupportHost::OnDisplayAdded(const display::Display& new_display) {}

void ArcSupportHost::OnDisplayRemoved(const display::Display& old_display) {}

void ArcSupportHost::OnDisplayMetricsChanged(const display::Display& display,
                                             uint32_t changed_metrics) {
  if (!message_host_)
    return;

  base::DictionaryValue message;
  message.SetString(kAction, kActionSetWindowBounds);
  message_host_->SendMessage(message);
}

void ArcSupportHost::OnMessage(const base::DictionaryValue& message) {
  std::string event;
  if (!message.GetString(kEvent, &event)) {
    NOTREACHED();
    return;
  }

  if (!observer_) {
    LOG(ERROR) << "No observer is found.";
    return;
  }

  if (event == kEventOnWindowClosed) {
    observer_->OnWindowClosed();
  } else if (event == kEventOnAuthSucceeded) {
    std::string code;
    if (message.GetString(kCode, &code)) {
      observer_->OnAuthSucceeded(code);
    } else {
      NOTREACHED();
    }
  } else if (event == kEventOnAgreed) {
    bool is_metrics_enabled;
    bool is_backup_restore_enabled;
    bool is_location_service_enabled;
    if (message.GetBoolean(kIsMetricsEnabled, &is_metrics_enabled) &&
        message.GetBoolean(kIsBackupRestoreEnabled,
                           &is_backup_restore_enabled) &&
        message.GetBoolean(kIsLocationServiceEnabled,
                           &is_location_service_enabled)) {
      observer_->OnTermsAgreed(is_metrics_enabled, is_backup_restore_enabled,
                               is_location_service_enabled);
    } else {
      NOTREACHED();
    }
  } else if (event == kEventOnSendFeedbackClicked) {
    observer_->OnSendFeedbackClicked();
  } else {
    LOG(ERROR) << "Unknown message: " << event;
    NOTREACHED();
  }
}
