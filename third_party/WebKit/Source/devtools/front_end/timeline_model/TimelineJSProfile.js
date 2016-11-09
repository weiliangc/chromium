// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

WebInspector.TimelineJSProfileProcessor = class {
  /**
   * @param {!WebInspector.CPUProfileDataModel} jsProfileModel
   * @param {!WebInspector.TracingModel.Thread} thread
   * @return {!Array<!WebInspector.TracingModel.Event>}
   */
  static generateTracingEventsFromCpuProfile(jsProfileModel, thread) {
    var idleNode = jsProfileModel.idleNode;
    var programNode = jsProfileModel.programNode;
    var gcNode = jsProfileModel.gcNode;
    var samples = jsProfileModel.samples;
    var timestamps = jsProfileModel.timestamps;
    var jsEvents = [];
    /** @type {!Map<!Object, !Array<!Protocol.Runtime.CallFrame>>} */
    var nodeToStackMap = new Map();
    nodeToStackMap.set(programNode, []);
    for (var i = 0; i < samples.length; ++i) {
      var node = jsProfileModel.nodeByIndex(i);
      if (!node) {
        console.error(`Node with unknown id ${samples[i]} at index ${i}`);
        continue;
      }
      if (node === gcNode || node === idleNode)
        continue;
      var callFrames = nodeToStackMap.get(node);
      if (!callFrames) {
        callFrames = /** @type {!Array<!Protocol.Runtime.CallFrame>} */ (new Array(node.depth + 1));
        nodeToStackMap.set(node, callFrames);
        for (var j = 0; node.parent; node = node.parent)
          callFrames[j++] = /** @type {!Protocol.Runtime.CallFrame} */ (node);
      }
      var jsSampleEvent = new WebInspector.TracingModel.Event(
          WebInspector.TracingModel.DevToolsTimelineEventCategory, WebInspector.TimelineModel.RecordType.JSSample,
          WebInspector.TracingModel.Phase.Instant, timestamps[i], thread);
      jsSampleEvent.args['data'] = {stackTrace: callFrames};
      jsEvents.push(jsSampleEvent);
    }
    return jsEvents;
  }

  /**
   * @param {!Array<!WebInspector.TracingModel.Event>} events
   * @return {!Array<!WebInspector.TracingModel.Event>}
   */
  static generateJSFrameEvents(events) {
    /**
     * @param {!Protocol.Runtime.CallFrame} frame1
     * @param {!Protocol.Runtime.CallFrame} frame2
     * @return {boolean}
     */
    function equalFrames(frame1, frame2) {
      return frame1.scriptId === frame2.scriptId && frame1.functionName === frame2.functionName;
    }

    /**
     * @param {!WebInspector.TracingModel.Event} e
     * @return {number}
     */
    function eventEndTime(e) {
      return e.endTime || e.startTime;
    }

    /**
     * @param {!WebInspector.TracingModel.Event} e
     * @return {boolean}
     */
    function isJSInvocationEvent(e) {
      switch (e.name) {
        case WebInspector.TimelineModel.RecordType.RunMicrotasks:
        case WebInspector.TimelineModel.RecordType.FunctionCall:
        case WebInspector.TimelineModel.RecordType.EvaluateScript:
          return true;
      }
      return false;
    }

    var jsFrameEvents = [];
    var jsFramesStack = [];
    var lockedJsStackDepth = [];
    var ordinal = 0;
    const showAllEvents = Runtime.experiments.isEnabled('timelineShowAllEvents');
    const showRuntimeCallStats = Runtime.experiments.isEnabled('timelineV8RuntimeCallStats');
    const showNativeFunctions = WebInspector.moduleSetting('showNativeFunctionsInJSProfile').get();

    const visibleV8RuntimeStatsItems = new Set([
      'Compile',
      'CompileCode',
      'CompileCodeLazy',
      'CompileDeserialize',
      'CompileEval',
      'CompileFullCode',
      'CompileIgnition',
      'CompilerDispatcher',
      'CompileSerialize',
      'DeoptimizeCode',
      'OptimizeCode',
      'ParseProgram',
      'ParseFunction',
      'RecompileConcurrent',
      'RecompileSynchronous',
      'ParseLazy',
      'FunctionCallback'
    ]);

    /**
     * @param {!WebInspector.TracingModel.Event} e
     */
    function onStartEvent(e) {
      e.ordinal = ++ordinal;
      extractStackTrace(e);
      // For the duration of the event we cannot go beyond the stack associated with it.
      lockedJsStackDepth.push(jsFramesStack.length);
    }

    /**
     * @param {!WebInspector.TracingModel.Event} e
     * @param {?WebInspector.TracingModel.Event} parent
     */
    function onInstantEvent(e, parent) {
      e.ordinal = ++ordinal;
      if (parent && isJSInvocationEvent(parent))
        extractStackTrace(e);
    }

    /**
     * @param {!WebInspector.TracingModel.Event} e
     */
    function onEndEvent(e) {
      truncateJSStack(lockedJsStackDepth.pop(), e.endTime);
    }

    /**
     * @param {number} depth
     * @param {number} time
     */
    function truncateJSStack(depth, time) {
      if (lockedJsStackDepth.length) {
        var lockedDepth = lockedJsStackDepth.peekLast();
        if (depth < lockedDepth) {
          console.error(
              'Child stack is shallower (' + depth + ') than the parent stack (' + lockedDepth + ') at ' + time);
          depth = lockedDepth;
        }
      }
      if (jsFramesStack.length < depth) {
        console.error('Trying to truncate higher than the current stack size at ' + time);
        depth = jsFramesStack.length;
      }
      for (var k = 0; k < jsFramesStack.length; ++k)
        jsFramesStack[k].setEndTime(time);
      jsFramesStack.length = depth;
    }

    /**
     * @param {!Array<!Protocol.Runtime.CallFrame>} stack
     */
    function filterStackFrames(stack) {
      if (showAllEvents)
        return;
      for (var i = 0, j = 0; i < stack.length; ++i) {
        const frame = stack[i];
        const url = frame.url;
        if (!showNativeFunctions && url && url.startsWith('native '))
          continue;
        if (url === 'native V8Runtime' && (!visibleV8RuntimeStatsItems.has(frame.functionName) || !showRuntimeCallStats))
          continue;
        stack[j++] = frame;
      }
      stack.length = j;
    }

    /**
     * @param {!WebInspector.TracingModel.Event} e
     */
    function extractStackTrace(e) {
      var recordTypes = WebInspector.TimelineModel.RecordType;
      var callFrames;
      if (e.name === recordTypes.JSSample) {
        var eventData = e.args['data'] || e.args['beginData'];
        callFrames = /** @type {!Array<!Protocol.Runtime.CallFrame>} */ (eventData && eventData['stackTrace']);
      } else {
        callFrames = /** @type {!Array<!Protocol.Runtime.CallFrame>} */ (
            jsFramesStack.map(frameEvent => frameEvent.args['data']).reverse());
      }
      filterStackFrames(callFrames);
      var endTime = eventEndTime(e);
      var numFrames = callFrames.length;
      var minFrames = Math.min(numFrames, jsFramesStack.length);
      var i;
      for (i = lockedJsStackDepth.peekLast() || 0; i < minFrames; ++i) {
        var newFrame = callFrames[numFrames - 1 - i];
        var oldFrame = jsFramesStack[i].args['data'];
        if (!equalFrames(newFrame, oldFrame))
          break;
        jsFramesStack[i].setEndTime(Math.max(jsFramesStack[i].endTime, endTime));
      }
      truncateJSStack(i, e.startTime);
      for (; i < numFrames; ++i) {
        var frame = callFrames[numFrames - 1 - i];
        var jsFrameEvent = new WebInspector.TracingModel.Event(
            WebInspector.TracingModel.DevToolsTimelineEventCategory, recordTypes.JSFrame,
            WebInspector.TracingModel.Phase.Complete, e.startTime, e.thread);
        jsFrameEvent.ordinal = e.ordinal;
        jsFrameEvent.addArgs({data: frame});
        jsFrameEvent.setEndTime(endTime);
        jsFramesStack.push(jsFrameEvent);
        jsFrameEvents.push(jsFrameEvent);
      }
    }

    const firstTopLevelEvent = events.find(WebInspector.TracingModel.isTopLevelEvent);
    if (firstTopLevelEvent)
      WebInspector.TimelineModel.forEachEvent(
          events, onStartEvent, onEndEvent, onInstantEvent, firstTopLevelEvent.startTime);
    return jsFrameEvents;
  }
};
