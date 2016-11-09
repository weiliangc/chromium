// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @interface
 */
WebInspector.View = function() {};

WebInspector.View.prototype = {
  /**
   * @return {string}
   */
  viewId: function() {},

  /**
   * @return {string}
   */
  title: function() {},

  /**
   * @return {boolean}
   */
  isCloseable: function() {},

  /**
   * @return {boolean}
   */
  isTransient: function() {},

  /**
   * @return {!Promise<!Array<!WebInspector.ToolbarItem>>}
   */
  toolbarItems: function() {},

  /**
   * @return {!Promise<!WebInspector.Widget>}
   */
  widget: function() {}
};

WebInspector.View._symbol = Symbol('view');
WebInspector.View._widgetSymbol = Symbol('widget');

/**
 * @implements {WebInspector.View}
 * @unrestricted
 */
WebInspector.SimpleView = class extends WebInspector.VBox {
  /**
   * @param {string} title
   * @param {boolean=} isWebComponent
   */
  constructor(title, isWebComponent) {
    super(isWebComponent);
    this._title = title;
    /** @type {!Array<!WebInspector.ToolbarItem>} */
    this._toolbarItems = [];
    this[WebInspector.View._symbol] = this;
  }

  /**
   * @override
   * @return {string}
   */
  viewId() {
    return this._title;
  }

  /**
   * @override
   * @return {string}
   */
  title() {
    return this._title;
  }

  /**
   * @override
   * @return {boolean}
   */
  isCloseable() {
    return false;
  }

  /**
   * @override
   * @return {boolean}
   */
  isTransient() {
    return false;
  }

  /**
   * @override
   * @return {!Promise<!Array<!WebInspector.ToolbarItem>>}
   */
  toolbarItems() {
    return Promise.resolve(this.syncToolbarItems());
  }

  /**
   * @return {!Array<!WebInspector.ToolbarItem>}
   */
  syncToolbarItems() {
    return this._toolbarItems;
  }

  /**
   * @override
   * @return {!Promise<!WebInspector.Widget>}
   */
  widget() {
    return /** @type {!Promise<!WebInspector.Widget>} */ (Promise.resolve(this));
  }

  /**
   * @param {!WebInspector.ToolbarItem} item
   */
  addToolbarItem(item) {
    this._toolbarItems.push(item);
  }

  /**
   * @return {!Promise}
   */
  revealView() {
    return WebInspector.viewManager.revealView(this);
  }
};

/**
 * @implements {WebInspector.View}
 * @unrestricted
 */
WebInspector.ProvidedView = class {
  /**
   * @param {!Runtime.Extension} extension
   */
  constructor(extension) {
    this._extension = extension;
  }

  /**
   * @override
   * @return {string}
   */
  viewId() {
    return this._extension.descriptor()['id'];
  }

  /**
   * @override
   * @return {string}
   */
  title() {
    return this._extension.title();
  }

  /**
   * @override
   * @return {boolean}
   */
  isCloseable() {
    return this._extension.descriptor()['persistence'] === 'closeable';
  }

  /**
   * @override
   * @return {boolean}
   */
  isTransient() {
    return this._extension.descriptor()['persistence'] === 'transient';
  }

  /**
   * @override
   * @return {!Promise<!Array<!WebInspector.ToolbarItem>>}
   */
  toolbarItems() {
    var actionIds = this._extension.descriptor()['actionIds'];
    if (actionIds) {
      var result = [];
      for (var id of actionIds.split(',')) {
        var item = WebInspector.Toolbar.createActionButtonForId(id.trim());
        if (item)
          result.push(item);
      }
      return Promise.resolve(result);
    }

    if (this._extension.descriptor()['hasToolbar'])
      return this.widget().then(
          widget => /** @type {!WebInspector.ToolbarItem.ItemsProvider} */ (widget).toolbarItems());
    return Promise.resolve([]);
  }

  /**
   * @override
   * @return {!Promise<!WebInspector.Widget>}
   */
  widget() {
    return this._extension.instance().then(widget => {
      if (!(widget instanceof WebInspector.Widget))
        throw new Error('view className should point to a WebInspector.Widget');
      widget[WebInspector.View._symbol] = this;
      return /** @type {!WebInspector.Widget} */ (widget);
    });
  }
};

/**
 * @interface
 */
WebInspector.ViewLocation = function() {};

WebInspector.ViewLocation.prototype = {
  /**
   * @param {string} locationName
   */
  appendApplicableItems: function(locationName) {},

  /**
   * @param {!WebInspector.View} view
   * @param {?WebInspector.View=} insertBefore
   */
  appendView: function(view, insertBefore) {},

  /**
   * @param {!WebInspector.View} view
   * @param {?WebInspector.View=} insertBefore
   * @return {!Promise}
   */
  showView: function(view, insertBefore) {},

  /**
   * @param {!WebInspector.View} view
   */
  removeView: function(view) {},

  /**
   * @return {!WebInspector.Widget}
   */
  widget: function() {}
};

/**
 * @interface
 * @extends {WebInspector.ViewLocation}
 */
WebInspector.TabbedViewLocation = function() {};

WebInspector.TabbedViewLocation.prototype = {
  /**
   * @return {!WebInspector.TabbedPane}
   */
  tabbedPane: function() {},

  enableMoreTabsButton: function() {}
};

/**
 * @interface
 */
WebInspector.ViewLocationResolver = function() {};

WebInspector.ViewLocationResolver.prototype = {
  /**
   * @param {string} location
   * @return {?WebInspector.ViewLocation}
   */
  resolveLocation: function(location) {}
};

/**
 * @unrestricted
 */
WebInspector.ViewManager = class {
  constructor() {
    /** @type {!Map<string, !WebInspector.View>} */
    this._views = new Map();
    /** @type {!Map<string, string>} */
    this._locationNameByViewId = new Map();

    for (var extension of self.runtime.extensions('view')) {
      var descriptor = extension.descriptor();
      this._views.set(descriptor['id'], new WebInspector.ProvidedView(extension));
      this._locationNameByViewId.set(descriptor['id'], descriptor['location']);
    }
  }

  /**
   * @param {!Element} element
   * @param {!Array<!WebInspector.ToolbarItem>} toolbarItems
   */
  static _populateToolbar(element, toolbarItems) {
    if (!toolbarItems.length)
      return;
    var toolbar = new WebInspector.Toolbar('');
    element.insertBefore(toolbar.element, element.firstChild);
    for (var item of toolbarItems)
      toolbar.appendToolbarItem(item);
  }

  /**
   * @param {!WebInspector.View} view
   * @return {!Promise}
   */
  revealView(view) {
    var location = /** @type {?WebInspector.ViewManager._Location} */ (view[WebInspector.ViewManager._Location.symbol]);
    if (!location)
      return Promise.resolve();
    location._reveal();
    return location.showView(view);
  }

  /**
   * @param {string} viewId
   * @return {?WebInspector.View}
   */
  view(viewId) {
    return this._views.get(viewId);
  }

  /**
   * @param {string} viewId
   * @return {?WebInspector.Widget}
   */
  materializedWidget(viewId) {
    var view = this.view(viewId);
    return view ? view[WebInspector.View._widgetSymbol] : null;
  }

  /**
   * @param {string} viewId
   * @return {!Promise}
   */
  showView(viewId) {
    var view = this._views.get(viewId);
    if (!view) {
      console.error('Could not find view for id: \'' + viewId + '\' ' + new Error().stack);
      return Promise.resolve();
    }

    var locationName = this._locationNameByViewId.get(viewId);
    if (locationName === 'drawer-view')
      WebInspector.userMetrics.drawerShown(viewId);

    var location = view[WebInspector.ViewManager._Location.symbol];
    if (location) {
      location._reveal();
      return location.showView(view);
    }

    return this._resolveLocation(locationName).then(location => {
      if (!location)
        throw new Error('Could not resolve location for view: ' + viewId);
      location._reveal();
      return location.showView(view);
    });
  }

  /**
   * @param {string=} location
   * @return {!Promise<?WebInspector.ViewManager._Location>}
   */
  _resolveLocation(location) {
    if (!location)
      return /** @type {!Promise<?WebInspector.ViewManager._Location>} */ (Promise.resolve(null));

    var resolverExtensions = self.runtime.extensions(WebInspector.ViewLocationResolver)
                                 .filter(extension => extension.descriptor()['name'] === location);
    if (!resolverExtensions.length)
      throw new Error('Unresolved location: ' + location);
    var resolverExtension = resolverExtensions[0];
    return resolverExtension.instance().then(
        resolver => /** @type {?WebInspector.ViewManager._Location} */ (resolver.resolveLocation(location)));
  }

  /**
   * @param {function()=} revealCallback
   * @param {string=} location
   * @param {boolean=} restoreSelection
   * @param {boolean=} allowReorder
   * @return {!WebInspector.TabbedViewLocation}
   */
  createTabbedLocation(revealCallback, location, restoreSelection, allowReorder) {
    return new WebInspector.ViewManager._TabbedLocation(this, revealCallback, location, restoreSelection, allowReorder);
  }

  /**
   * @param {function()=} revealCallback
   * @param {string=} location
   * @return {!WebInspector.ViewLocation}
   */
  createStackLocation(revealCallback, location) {
    return new WebInspector.ViewManager._StackLocation(this, revealCallback, location);
  }

  /**
   * @param {string} location
   * @return {!Array<!WebInspector.View>}
   */
  _viewsForLocation(location) {
    var result = [];
    for (var id of this._views.keys()) {
      if (this._locationNameByViewId.get(id) === location)
        result.push(this._views.get(id));
    }
    return result;
  }
};


/**
 * @unrestricted
 */
WebInspector.ViewManager._ContainerWidget = class extends WebInspector.VBox {
  /**
   * @param {!WebInspector.View} view
   */
  constructor(view) {
    super();
    this.element.classList.add('flex-auto', 'view-container', 'overflow-auto');
    this._view = view;
    this.element.tabIndex = 0;
    this.setDefaultFocusedElement(this.element);
  }

  /**
   * @return {!Promise}
   */
  _materialize() {
    if (this._materializePromise)
      return this._materializePromise;
    var promises = [];
    promises.push(this._view.toolbarItems().then(
        WebInspector.ViewManager._populateToolbar.bind(WebInspector.ViewManager, this.element)));
    promises.push(this._view.widget().then(widget => {
      // Move focus from |this| to loaded |widget| if any.
      var shouldFocus = this.element.hasFocus();
      this.setDefaultFocusedElement(null);
      this._view[WebInspector.View._widgetSymbol] = widget;
      widget.show(this.element);
      if (shouldFocus)
        widget.focus();
    }));
    this._materializePromise = Promise.all(promises);
    return this._materializePromise;
  }
};

/**
 * @unrestricted
 */
WebInspector.ViewManager._ExpandableContainerWidget = class extends WebInspector.VBox {
  /**
   * @param {!WebInspector.View} view
   */
  constructor(view) {
    super(true);
    this.element.classList.add('flex-none');
    this.registerRequiredCSS('ui/viewContainers.css');

    this._titleElement = createElementWithClass('div', 'expandable-view-title');
    this._titleElement.textContent = view.title();
    this._titleElement.tabIndex = 0;
    this._titleElement.addEventListener('click', this._toggleExpanded.bind(this), false);
    this._titleElement.addEventListener('keydown', this._onTitleKeyDown.bind(this), false);
    this.contentElement.insertBefore(this._titleElement, this.contentElement.firstChild);

    this.contentElement.createChild('content');
    this._view = view;
    view[WebInspector.ViewManager._ExpandableContainerWidget._symbol] = this;
  }

  /**
   * @return {!Promise}
   */
  _materialize() {
    if (this._materializePromise)
      return this._materializePromise;
    var promises = [];
    promises.push(this._view.toolbarItems().then(
        WebInspector.ViewManager._populateToolbar.bind(WebInspector.ViewManager, this._titleElement)));
    promises.push(this._view.widget().then(widget => {
      this._widget = widget;
      this._view[WebInspector.View._widgetSymbol] = widget;
      widget.show(this.element);
    }));
    this._materializePromise = Promise.all(promises);
    return this._materializePromise;
  }

  /**
   * @return {!Promise}
   */
  _expand() {
    if (this._titleElement.classList.contains('expanded'))
      return this._materialize();
    this._titleElement.classList.add('expanded');
    return this._materialize().then(() => this._widget.show(this.element));
  }

  _collapse() {
    if (!this._titleElement.classList.contains('expanded'))
      return;
    this._titleElement.classList.remove('expanded');
    this._materialize().then(() => this._widget.detach());
  }

  _toggleExpanded() {
    if (this._titleElement.classList.contains('expanded'))
      this._collapse();
    else
      this._expand();
  }

  /**
   * @param {!Event} event
   */
  _onTitleKeyDown(event) {
    if (isEnterKey(event) || event.keyCode === WebInspector.KeyboardShortcut.Keys.Space.code)
      this._toggleExpanded();
  }
};

WebInspector.ViewManager._ExpandableContainerWidget._symbol = Symbol('container');

/**
 * @unrestricted
 */
WebInspector.ViewManager._Location = class {
  /**
   * @param {!WebInspector.ViewManager} manager
   * @param {!WebInspector.Widget} widget
   * @param {function()=} revealCallback
   */
  constructor(manager, widget, revealCallback) {
    this._manager = manager;
    this._revealCallback = revealCallback;
    this._widget = widget;
  }

  /**
   * @return {!WebInspector.Widget}
   */
  widget() {
    return this._widget;
  }

  _reveal() {
    if (this._revealCallback)
      this._revealCallback();
  }
};

WebInspector.ViewManager._Location.symbol = Symbol('location');

/**
 * @implements {WebInspector.TabbedViewLocation}
 * @unrestricted
 */
WebInspector.ViewManager._TabbedLocation = class extends WebInspector.ViewManager._Location {
  /**
   * @param {!WebInspector.ViewManager} manager
   * @param {function()=} revealCallback
   * @param {string=} location
   * @param {boolean=} restoreSelection
   * @param {boolean=} allowReorder
   */
  constructor(manager, revealCallback, location, restoreSelection, allowReorder) {
    var tabbedPane = new WebInspector.TabbedPane();
    if (allowReorder)
      tabbedPane.setAllowTabReorder(true);

    super(manager, tabbedPane, revealCallback);
    this._tabbedPane = tabbedPane;
    this._allowReorder = allowReorder;

    this._tabbedPane.addEventListener(WebInspector.TabbedPane.Events.TabSelected, this._tabSelected, this);
    this._tabbedPane.addEventListener(WebInspector.TabbedPane.Events.TabClosed, this._tabClosed, this);
    this._closeableTabSetting = WebInspector.settings.createSetting(location + '-closeableTabs', {});
    this._tabOrderSetting = WebInspector.settings.createSetting(location + '-tabOrder', {});
    this._tabbedPane.addEventListener(WebInspector.TabbedPane.Events.TabOrderChanged, this._persistTabOrder, this);
    if (restoreSelection)
      this._lastSelectedTabSetting = WebInspector.settings.createSetting(location + '-selectedTab', '');

    /** @type {!Map.<string, !WebInspector.View>} */
    this._views = new Map();

    if (location)
      this.appendApplicableItems(location);
  }

  /**
   * @override
   * @return {!WebInspector.Widget}
   */
  widget() {
    return this._tabbedPane;
  }

  /**
   * @override
   * @return {!WebInspector.TabbedPane}
   */
  tabbedPane() {
    return this._tabbedPane;
  }

  /**
   * @override
   */
  enableMoreTabsButton() {
    this._tabbedPane.leftToolbar().appendToolbarItem(
        new WebInspector.ToolbarMenuButton(this._appendTabsToMenu.bind(this)));
    this._tabbedPane.disableOverflowMenu();
  }

  /**
   * @override
   * @param {string} locationName
   */
  appendApplicableItems(locationName) {
    var views = this._manager._viewsForLocation(locationName);
    if (this._allowReorder) {
      var i = 0;
      var persistedOrders = this._tabOrderSetting.get();
      var orders = new Map();
      for (var view of views)
        orders.set(
            view.viewId(),
            persistedOrders[view.viewId()] || (++i) * WebInspector.ViewManager._TabbedLocation.orderStep);
      views.sort((a, b) => orders.get(a.viewId()) - orders.get(b.viewId()));
    }

    for (var view of views) {
      var id = view.viewId();
      this._views.set(id, view);
      view[WebInspector.ViewManager._Location.symbol] = this;
      if (view.isTransient())
        continue;
      if (!view.isCloseable())
        this._appendTab(view);
      else if (this._closeableTabSetting.get()[id])
        this._appendTab(view);
    }
    if (this._lastSelectedTabSetting && this._tabbedPane.hasTab(this._lastSelectedTabSetting.get()))
      this._tabbedPane.selectTab(this._lastSelectedTabSetting.get());
  }

  /**
   * @param {!WebInspector.ContextMenu} contextMenu
   */
  _appendTabsToMenu(contextMenu) {
    for (var view of this._views.values()) {
      var title = WebInspector.UIString(view.title());
      contextMenu.appendItem(title, this.showView.bind(this, view));
    }
  }

  /**
   * @param {!WebInspector.View} view
   * @param {number=} index
   */
  _appendTab(view, index) {
    this._tabbedPane.appendTab(
        view.viewId(), view.title(), new WebInspector.ViewManager._ContainerWidget(view), undefined, false,
        view.isCloseable() || view.isTransient(), index);
  }

  /**
   * @override
   * @param {!WebInspector.View} view
   * @param {?WebInspector.View=} insertBefore
   */
  appendView(view, insertBefore) {
    if (this._tabbedPane.hasTab(view.viewId()))
      return;
    view[WebInspector.ViewManager._Location.symbol] = this;
    this._manager._views.set(view.viewId(), view);
    this._views.set(view.viewId(), view);

    var index = undefined;
    var tabIds = this._tabbedPane.tabIds();
    if (this._allowReorder) {
      var orderSetting = this._tabOrderSetting.get();
      var order = orderSetting[view.viewId()];
      for (var i = 0; order && i < tabIds.length; ++i) {
        if (orderSetting[tabIds[i]] && orderSetting[tabIds[i]] > order) {
          index = i;
          break;
        }
      }
    } else if (insertBefore) {
      for (var i = 0; i < tabIds.length; ++i) {
        if (tabIds[i] === insertBefore.viewId()) {
          index = i;
          break;
        }
      }
    }
    this._appendTab(view, index);
  }

  /**
   * @override
   * @param {!WebInspector.View} view
   * @param {?WebInspector.View=} insertBefore
   * @return {!Promise}
   */
  showView(view, insertBefore) {
    this.appendView(view, insertBefore);
    this._tabbedPane.selectTab(view.viewId());
    this._tabbedPane.focus();
    return this._materializeWidget(view);
  }

  /**
   * @param {!WebInspector.View} view
   * @override
   */
  removeView(view) {
    if (!this._tabbedPane.hasTab(view.viewId()))
      return;

    delete view[WebInspector.ViewManager._Location.symbol];
    this._manager._views.delete(view.viewId());
    this._views.delete(view.viewId());
    this._tabbedPane.closeTab(view.viewId());
  }

  /**
   * @param {!WebInspector.Event} event
   */
  _tabSelected(event) {
    var tabId = /** @type {string} */ (event.data.tabId);
    if (this._lastSelectedTabSetting && event.data['isUserGesture'])
      this._lastSelectedTabSetting.set(tabId);
    var view = this._views.get(tabId);
    if (!view)
      return;

    this._materializeWidget(view);

    if (view.isCloseable()) {
      var tabs = this._closeableTabSetting.get();
      if (!tabs[tabId]) {
        tabs[tabId] = true;
        this._closeableTabSetting.set(tabs);
      }
    }
  }

  /**
   * @param {!WebInspector.Event} event
   */
  _tabClosed(event) {
    var id = /** @type {string} */ (event.data['tabId']);
    var tabs = this._closeableTabSetting.get();
    if (tabs[id]) {
      delete tabs[id];
      this._closeableTabSetting.set(tabs);
    }
  }

  /**
   * @param {!WebInspector.View} view
   * @return {!Promise}
   */
  _materializeWidget(view) {
    var widget = /** @type {!WebInspector.ViewManager._ContainerWidget} */ (this._tabbedPane.tabView(view.viewId()));
    return widget._materialize();
  }

  /**
   * @param {!WebInspector.Event} event
   */
  _persistTabOrder(event) {
    var tabIds = this._tabbedPane.tabIds();
    var tabOrders = {};
    for (var i = 0; i < tabIds.length; i++)
      tabOrders[tabIds[i]] = (i + 1) * WebInspector.ViewManager._TabbedLocation.orderStep;
    this._tabOrderSetting.set(tabOrders);
  }
};

WebInspector.ViewManager._TabbedLocation.orderStep = 10;  // Keep in sync with descriptors.

/**
 * @implements {WebInspector.ViewLocation}
 * @unrestricted
 */
WebInspector.ViewManager._StackLocation = class extends WebInspector.ViewManager._Location {
  /**
   * @param {!WebInspector.ViewManager} manager
   * @param {function()=} revealCallback
   * @param {string=} location
   */
  constructor(manager, revealCallback, location) {
    var vbox = new WebInspector.VBox();
    super(manager, vbox, revealCallback);
    this._vbox = vbox;

    /** @type {!Map<string, !WebInspector.ViewManager._ExpandableContainerWidget>} */
    this._expandableContainers = new Map();

    if (location)
      this.appendApplicableItems(location);
  }

  /**
   * @override
   * @param {!WebInspector.View} view
   * @param {?WebInspector.View=} insertBefore
   */
  appendView(view, insertBefore) {
    var container = this._expandableContainers.get(view.viewId());
    if (!container) {
      view[WebInspector.ViewManager._Location.symbol] = this;
      this._manager._views.set(view.viewId(), view);
      container = new WebInspector.ViewManager._ExpandableContainerWidget(view);
      var beforeElement = null;
      if (insertBefore) {
        var beforeContainer = insertBefore[WebInspector.ViewManager._ExpandableContainerWidget._symbol];
        beforeElement = beforeContainer ? beforeContainer.element : null;
      }
      container.show(this._vbox.contentElement, beforeElement);
      this._expandableContainers.set(view.viewId(), container);
    }
  }

  /**
   * @override
   * @param {!WebInspector.View} view
   * @param {?WebInspector.View=} insertBefore
   * @return {!Promise}
   */
  showView(view, insertBefore) {
    this.appendView(view, insertBefore);
    var container = this._expandableContainers.get(view.viewId());
    return container._expand();
  }

  /**
   * @param {!WebInspector.View} view
   * @override
   */
  removeView(view) {
    var container = this._expandableContainers.get(view.viewId());
    if (!container)
      return;

    container.detach();
    this._expandableContainers.delete(view.viewId());
    delete view[WebInspector.ViewManager._Location.symbol];
    this._manager._views.delete(view.viewId());
  }

  /**
   * @override
   * @param {string} locationName
   */
  appendApplicableItems(locationName) {
    for (var view of this._manager._viewsForLocation(locationName))
      this.appendView(view);
  }
};

/**
 * @type {!WebInspector.ViewManager}
 */
WebInspector.viewManager;
