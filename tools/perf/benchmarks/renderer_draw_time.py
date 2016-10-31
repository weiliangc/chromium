# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry import benchmark

from measurements import renderer_draw_time
import page_sets

class RendererDrawTimeTop25(benchmark.Benchmark):
    """Measures the relative performance of Skia Renderer.
    http://www.chromium.org/developers/design-documents/rendering-benchmarks
    """
    test = renderer_draw_time.RendererDrawTime
    page_set = page_sets.Top25SmoothPageSet

    @classmethod
    def Name(cls):
        return 'renderer_draw_time.top_25'
