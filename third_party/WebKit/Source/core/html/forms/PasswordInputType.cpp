/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/html/forms/PasswordInputType.h"

#include "core/InputTypeNames.h"
#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/forms/FormController.h"
#include "core/layout/LayoutTextControlSingleLine.h"
#include "public/platform/InterfaceProvider.h"
#include "public/platform/modules/sensitive_input_visibility/sensitive_input_visibility_service.mojom-blink.h"
#include "wtf/Assertions.h"
#include "wtf/PassRefPtr.h"

namespace blink {

InputType* PasswordInputType::create(HTMLInputElement& element) {
  return new PasswordInputType(element);
}

void PasswordInputType::countUsage() {
  countUsageIfVisible(UseCounter::InputTypePassword);
  if (element().fastHasAttribute(HTMLNames::maxlengthAttr))
    countUsageIfVisible(UseCounter::InputTypePasswordMaxLength);
}

const AtomicString& PasswordInputType::formControlType() const {
  return InputTypeNames::password;
}

bool PasswordInputType::shouldSaveAndRestoreFormControlState() const {
  return false;
}

FormControlState PasswordInputType::saveFormControlState() const {
  // Should never save/restore password fields.
  NOTREACHED();
  return FormControlState();
}

void PasswordInputType::restoreFormControlState(const FormControlState&) {
  // Should never save/restore password fields.
  NOTREACHED();
}

bool PasswordInputType::shouldRespectListAttribute() {
  return false;
}

void PasswordInputType::enableSecureTextInput() {
  if (element().document().frame())
    element().document().setUseSecureKeyboardEntryWhenActive(true);
}

void PasswordInputType::disableSecureTextInput() {
  if (element().document().frame())
    element().document().setUseSecureKeyboardEntryWhenActive(false);
}

void PasswordInputType::onAttachWithLayoutObject() {
  Document& document = element().document();
  DCHECK(document.frame());
  if (document.isSecureContext()) {
    // The browser process only cares about passwords on pages where the
    // top-level URL is not secure. Secure contexts must have a top-level
    // URL that is secure, so there is no need to send notifications for
    // password fields in secure contexts.
    return;
  }

  document.incrementPasswordCount();
  if (document.passwordCount() > 1) {
    // Only send a message on the first visible password field; the
    // browser process doesn't care about the presence of additional
    // password fields beyond that.
    return;
  }
  mojom::blink::SensitiveInputVisibilityServicePtr sensitiveInputServicePtr;
  document.frame()->interfaceProvider()->getInterface(
      mojo::GetProxy(&sensitiveInputServicePtr));
  sensitiveInputServicePtr->PasswordFieldVisibleInInsecureContext();
}

void PasswordInputType::onDetachWithLayoutObject() {
  Document& document = element().document();
  DCHECK(document.frame());
  if (document.isSecureContext()) {
    return;
  }
  document.decrementPasswordCount();
  if (document.passwordCount() > 0)
    return;

  mojom::blink::SensitiveInputVisibilityServicePtr sensitiveInputServicePtr;
  document.frame()->interfaceProvider()->getInterface(
      mojo::GetProxy(&sensitiveInputServicePtr));
  sensitiveInputServicePtr->AllPasswordFieldsInInsecureContextInvisible();
}

}  // namespace blink
