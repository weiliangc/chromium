# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import legacy_page_test
from telemetry.timeline import model
from telemetry.timeline import tracing_config
from telemetry.value import scalar


class RendererDrawTime(legacy_page_test.LegacyPageTest):

  def __init__(self):
    super(RendererDrawTime, self).__init__()

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs([
        '--enable-prefer-compositing-to-lcd-text',
    ])

  def WillNavigateToPage(self, page, tab):
    del page  # unused
    config = tracing_config.TracingConfig()
    config.chrome_trace_config.category_filter.AddIncludedCategory(
        'cc')
    config.enable_chrome_trace = True
    tab.browser.platform.tracing_controller.StartTracing(config)

  def ComputeAverageOfDurations(self, timeline_model, name):
    events = timeline_model.GetAllEventsOfName(name)
    event_durations = [d.duration for d in events]
    assert event_durations, 'Failed to find durations'

    duration_sum = sum(event_durations) * 1000
    duration_count = len(event_durations)
    duration_avg = duration_sum / duration_count
    return duration_avg

  def ValidateAndMeasurePage(self, page, tab, results):
    del page  # unused
    timeline_data = tab.browser.platform.tracing_controller.StopTracing()
    timeline_model = model.TimelineModel(timeline_data)

    draw_frame_avg = self.ComputeAverageOfDurations(
        timeline_model,
        'DirectRenderer::DrawFrame')

    results.AddValue(scalar.ScalarValue(
        results.current_page, 'DF_avg_cost', 'us', draw_frame_avg,
        description='Average time spent drawing frame'))

    draw_render_pass_avg = self.ComputeAverageOfDurations(
        timeline_model,
        'DirectRenderer::DrawRenderPass')

    results.AddValue(scalar.ScalarValue(
        results.current_page, 'DRP_avg_cost', 'us', draw_render_pass_avg,
        description='Average time spent drawing render pass'))

    draw_quad_avg = self.ComputeAverageOfDurations(
        timeline_model,
        'SkiaRenderer::DoDrawQuad')

    results.AddValue(scalar.ScalarValue(
        results.current_page, 'DQ_avg_cost', 'us', draw_quad_avg,
        description='Average time spent drawing quad'))


  def DidRunPage(self, platform):
    tracing_controller = platform.tracing_controller
    if tracing_controller.is_tracing_running:
      tracing_controller.StopTracing()
