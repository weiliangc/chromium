// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @unrestricted
 */
WebInspector.ARIAAttributesPane = class extends WebInspector.AccessibilitySubPane {
  constructor() {
    super(WebInspector.UIString('ARIA Attributes'));

    this._noPropertiesInfo = this.createInfo(WebInspector.UIString('No ARIA attributes'));
    this._treeOutline = this.createTreeOutline();
  }

  /**
   * @override
   * @param {?WebInspector.DOMNode} node
   */
  setNode(node) {
    super.setNode(node);
    this._treeOutline.removeChildren();
    if (!this.node())
      return;
    var target = this.node().target();
    var attributes = node.attributes();
    for (var i = 0; i < attributes.length; ++i) {
      var attribute = attributes[i];
      if (WebInspector.ARIAAttributesPane._attributes.indexOf(attribute.name) < 0)
        continue;
      this._treeOutline.appendChild(new WebInspector.ARIAAttributesTreeElement(this, attribute, target));
    }

    var foundAttributes = (this._treeOutline.rootElement().childCount() !== 0);
    this._noPropertiesInfo.classList.toggle('hidden', foundAttributes);
    this._treeOutline.element.classList.toggle('hidden', !foundAttributes);
  }
};

/**
 * @unrestricted
 */
WebInspector.ARIAAttributesTreeElement = class extends TreeElement {
  /**
   * @param {!WebInspector.ARIAAttributesPane} parentPane
   * @param {!WebInspector.DOMNode.Attribute} attribute
   * @param {!WebInspector.Target} target
   */
  constructor(parentPane, attribute, target) {
    super('');

    this._parentPane = parentPane;
    this._attribute = attribute;

    this.selectable = false;
  }

  /**
   * @param {string} value
   * @return {!Element}
   */
  static createARIAValueElement(value) {
    var valueElement = createElementWithClass('span', 'monospace');
    // TODO(aboxhall): quotation marks?
    valueElement.setTextContentTruncatedIfNeeded(value || '');
    return valueElement;
  }

  /**
   * @override
   */
  onattach() {
    this._populateListItem();
    this.listItemElement.addEventListener('click', this._mouseClick.bind(this));
  }

  _populateListItem() {
    this.listItemElement.removeChildren();
    this.appendNameElement(this._attribute.name);
    this.listItemElement.createChild('span', 'separator').textContent = ':\u00A0';
    this.appendAttributeValueElement(this._attribute.value);
  }

  /**
   * @param {string} name
   */
  appendNameElement(name) {
    this._nameElement = createElement('span');
    this._nameElement.textContent = name;
    this._nameElement.classList.add('ax-name');
    this._nameElement.classList.add('monospace');
    this.listItemElement.appendChild(this._nameElement);
  }

  /**
   * @param {string} value
   */
  appendAttributeValueElement(value) {
    this._valueElement = WebInspector.ARIAAttributesTreeElement.createARIAValueElement(value);
    this.listItemElement.appendChild(this._valueElement);
  }

  /**
   * @param {!Event} event
   */
  _mouseClick(event) {
    if (event.target === this.listItemElement)
      return;

    event.consume(true);

    this._startEditing();
  }

  _startEditing() {
    var valueElement = this._valueElement;

    if (WebInspector.isBeingEdited(valueElement))
      return;

    var previousContent = valueElement.textContent;

    /**
     * @param {string} previousContent
     * @param {!Event} event
     * @this {WebInspector.ARIAAttributesTreeElement}
     */
    function blurListener(previousContent, event) {
      var text = event.target.textContent;
      this._editingCommitted(text, previousContent);
    }

    this._prompt = new WebInspector.ARIAAttributesPane.ARIAAttributePrompt(
        WebInspector.ariaMetadata().valuesForProperty(this._nameElement.textContent), this);
    this._prompt.setAutocompletionTimeout(0);
    var proxyElement = this._prompt.attachAndStartEditing(valueElement, blurListener.bind(this, previousContent));

    proxyElement.addEventListener('keydown', this._editingValueKeyDown.bind(this, previousContent), false);

    valueElement.getComponentSelection().setBaseAndExtent(valueElement, 0, valueElement, 1);
  }

  _removePrompt() {
    if (!this._prompt)
      return;
    this._prompt.detach();
    delete this._prompt;
  }

  /**
   * @param {string} userInput
   * @param {string} previousContent
   */
  _editingCommitted(userInput, previousContent) {
    this._removePrompt();

    // Make the changes to the attribute
    if (userInput !== previousContent)
      this._parentPane.node().setAttributeValue(this._attribute.name, userInput);
  }

  _editingCancelled() {
    this._removePrompt();
    this._populateListItem();
  }

  /**
   * @param {string} previousContent
   * @param {!Event} event
   */
  _editingValueKeyDown(previousContent, event) {
    if (event.handled)
      return;

    if (isEnterKey(event)) {
      this._editingCommitted(event.target.textContent, previousContent);
      event.consume();
      return;
    }

    if (event.keyCode === WebInspector.KeyboardShortcut.Keys.Esc.code || event.keyIdentifier === 'U+001B') {
      this._editingCancelled();
      event.consume();
      return;
    }
  }
};


/**
 * @unrestricted
 */
WebInspector.ARIAAttributesPane.ARIAAttributePrompt = class extends WebInspector.TextPrompt {
  /**
   * @param {!Array<string>} ariaCompletions
   * @param {!WebInspector.ARIAAttributesTreeElement} treeElement
   */
  constructor(ariaCompletions, treeElement) {
    super();
    this.initialize(this._buildPropertyCompletions.bind(this));

    this.setSuggestBoxEnabled(true);

    this._ariaCompletions = ariaCompletions;
    this._treeElement = treeElement;
  }

  /**
   * @param {!Element} proxyElement
   * @param {!Range} wordRange
   * @param {boolean} force
   * @param {function(!Array.<string>, number=)} completionsReadyCallback
   */
  _buildPropertyCompletions(proxyElement, wordRange, force, completionsReadyCallback) {
    var prefix = wordRange.toString().toLowerCase();
    if (!prefix && !force && (this._isEditingName || proxyElement.textContent.length)) {
      completionsReadyCallback([]);
      return;
    }

    var results = this._ariaCompletions.filter((value) => value.startsWith(prefix));

    completionsReadyCallback(results, 0);
  }
};

WebInspector.ARIAAttributesPane._attributes = [
  'role',
  'aria-busy',
  'aria-checked',
  'aria-disabled',
  'aria-expanded',
  'aria-grabbed',
  'aria-hidden',
  'aria-invalid',
  'aria-pressed',
  'aria-selected',
  'aria-activedescendant',
  'aria-atomic',
  'aria-autocomplete',
  'aria-controls',
  'aria-describedby',
  'aria-dropeffect',
  'aria-flowto',
  'aria-haspopup',
  'aria-label',
  'aria-labelledby',
  'aria-level',
  'aria-live',
  'aria-multiline',
  'aria-multiselectable',
  'aria-orientation',
  'aria-owns',
  'aria-posinset',
  'aria-readonly',
  'aria-relevant',
  'aria-required',
  'aria-setsize',
  'aria-sort',
  'aria-valuemax',
  'aria-valuemin',
  'aria-valuenow',
  'aria-valuetext',
];
