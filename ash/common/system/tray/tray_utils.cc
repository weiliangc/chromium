// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/tray/tray_utils.h"

#include "ash/common/ash_constants.h"
#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/session/session_state_delegate.h"
#include "ash/common/shelf/wm_shelf_util.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_item_view.h"
#include "ash/common/system/tray/tray_popup_label_button_border.h"
#include "ash/common/wm_shell.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"

namespace ash {

namespace {

class BorderlessLabelButton : public views::LabelButton {
 public:
  BorderlessLabelButton(views::ButtonListener* listener,
                        const base::string16& text)
      : LabelButton(listener, text) {
    if (MaterialDesignController::IsSystemTrayMenuMaterial()) {
      SetInkDropMode(views::InkDropHostView::InkDropMode::ON);
      set_has_ink_drop_action_on_click(true);
      set_ink_drop_base_color(kTrayPopupInkDropBaseColor);
      set_ink_drop_visible_opacity(kTrayPopupInkDropRippleOpacity);
      const int kHorizontalPadding = 20;
      SetBorder(views::CreateEmptyBorder(gfx::Insets(0, kHorizontalPadding)));
      // TODO(tdanderson): Update focus rect for material design. See
      // crbug.com/615892
    } else {
      SetBorder(std::unique_ptr<views::Border>(new TrayPopupLabelButtonBorder));
      SetFocusPainter(views::Painter::CreateSolidFocusPainter(
          kFocusBorderColor, gfx::Insets(1, 1, 2, 2)));
      set_animate_on_state_change(false);
    }
    SetHorizontalAlignment(gfx::ALIGN_CENTER);
    SetFocusForPlatform();
  }

  ~BorderlessLabelButton() override {}

  // views::LabelButton:
  int GetHeightForWidth(int width) const override {
    if (MaterialDesignController::IsSystemTrayMenuMaterial())
      return kMenuButtonSize - 2 * kTrayPopupInkDropInset;

    return LabelButton::GetHeightForWidth(width);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BorderlessLabelButton);
};

}  // namespace

views::LabelButton* CreateTrayPopupBorderlessButton(
    views::ButtonListener* listener,
    const base::string16& text) {
  return new BorderlessLabelButton(listener, text);
}

views::LabelButton* CreateTrayPopupButton(views::ButtonListener* listener,
                                          const base::string16& text) {
  if (!MaterialDesignController::IsSystemTrayMenuMaterial())
    return CreateTrayPopupBorderlessButton(listener, text);

  auto* button = views::MdTextButton::Create(listener, text);
  button->SetProminent(true);
  return button;
}

void SetupLabelForTray(views::Label* label) {
  if (MaterialDesignController::IsShelfMaterial()) {
    // The text is drawn on an transparent bg, so we must disable subpixel
    // rendering.
    label->SetSubpixelRenderingEnabled(false);
    label->SetFontList(gfx::FontList().Derive(2, gfx::Font::NORMAL,
                                              gfx::Font::Weight::MEDIUM));
  } else {
    label->SetFontList(
        gfx::FontList().Derive(1, gfx::Font::NORMAL, gfx::Font::Weight::BOLD));
    label->SetShadows(gfx::ShadowValues(
        1,
        gfx::ShadowValue(gfx::Vector2d(0, 1), 0, SkColorSetARGB(64, 0, 0, 0))));
    label->SetAutoColorReadabilityEnabled(false);
    label->SetEnabledColor(SK_ColorWHITE);
    label->SetBackgroundColor(SkColorSetARGB(0, 255, 255, 255));
  }
}

void SetTrayImageItemBorder(views::View* tray_view, ShelfAlignment alignment) {
  if (MaterialDesignController::IsShelfMaterial())
    return;

  const int tray_image_item_padding = GetTrayConstant(TRAY_IMAGE_ITEM_PADDING);
  if (IsHorizontalAlignment(alignment)) {
    tray_view->SetBorder(views::CreateEmptyBorder(0, tray_image_item_padding, 0,
                                                  tray_image_item_padding));
  } else {
    tray_view->SetBorder(views::CreateEmptyBorder(
        tray_image_item_padding,
        kTrayImageItemHorizontalPaddingVerticalAlignment,
        tray_image_item_padding,
        kTrayImageItemHorizontalPaddingVerticalAlignment));
  }
}

void SetTrayLabelItemBorder(TrayItemView* tray_view, ShelfAlignment alignment) {
  if (MaterialDesignController::IsShelfMaterial())
    return;

  if (IsHorizontalAlignment(alignment)) {
    tray_view->SetBorder(views::CreateEmptyBorder(
        0, kTrayLabelItemHorizontalPaddingBottomAlignment, 0,
        kTrayLabelItemHorizontalPaddingBottomAlignment));
  } else {
    // Center the label for vertical launcher alignment.
    int horizontal_padding =
        std::max(0, (tray_view->GetPreferredSize().width() -
                     tray_view->label()->GetPreferredSize().width()) /
                        2);
    tray_view->SetBorder(views::CreateEmptyBorder(
        kTrayLabelItemVerticalPaddingVerticalAlignment, horizontal_padding,
        kTrayLabelItemVerticalPaddingVerticalAlignment, horizontal_padding));
  }
}

void GetAccessibleLabelFromDescendantViews(
    views::View* view,
    std::vector<base::string16>& out_labels) {
  ui::AXNodeData temp_node_data;
  view->GetAccessibleNodeData(&temp_node_data);
  if (!temp_node_data.GetStringAttribute(ui::AX_ATTR_NAME).empty())
    out_labels.push_back(temp_node_data.GetString16Attribute(ui::AX_ATTR_NAME));

  // Do not descend into static text labels which may compute their own labels
  // recursively.
  if (temp_node_data.role == ui::AX_ROLE_STATIC_TEXT)
    return;

  for (int i = 0; i < view->child_count(); ++i)
    GetAccessibleLabelFromDescendantViews(view->child_at(i), out_labels);
}

bool CanOpenWebUISettings(LoginStatus status) {
  // TODO(tdanderson): Consider moving this into WmShell, or introduce a
  // CanShowSettings() method in each delegate type that has a
  // ShowSettings() method.
  return status != LoginStatus::NOT_LOGGED_IN &&
         status != LoginStatus::LOCKED &&
         !WmShell::Get()->GetSessionStateDelegate()->IsInSecondaryLoginScreen();
}

views::Separator* CreateVerticalSeparator() {
  views::Separator* separator =
      new views::Separator(views::Separator::HORIZONTAL);
  separator->SetPreferredSize(kHorizontalSeparatorHeight);
  separator->SetColor(kHorizontalSeparatorColor);
  return separator;
}

}  // namespace ash
