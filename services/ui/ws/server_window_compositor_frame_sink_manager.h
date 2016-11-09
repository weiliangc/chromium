// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_SERVER_WINDOW_SURFACE_MANAGER_H_
#define SERVICES_UI_WS_SERVER_WINDOW_SURFACE_MANAGER_H_

#include <map>

#include "base/macros.h"
#include "cc/ipc/compositor_frame.mojom.h"
#include "cc/surfaces/surface_factory.h"
#include "cc/surfaces/surface_id.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/ui/public/interfaces/window_tree.mojom.h"

namespace ui {
namespace ws {

class ServerWindow;
class ServerWindowCompositorFrameSink;
class ServerWindowCompositorFrameSinkManagerTestApi;

// ServerWindowCompositorFrameSinkManager tracks the surfaces associated with a
// ServerWindow.
class ServerWindowCompositorFrameSinkManager {
 public:
  explicit ServerWindowCompositorFrameSinkManager(ServerWindow* window);
  ~ServerWindowCompositorFrameSinkManager();

  // Returns true if the surfaces from this manager should be drawn.
  bool ShouldDraw();

  // Creates a new surface of the specified type, replacing the existing one of
  // the specified type.
  void CreateCompositorFrameSink(
      mojom::CompositorFrameSinkType surface_type,
      mojo::InterfaceRequest<cc::mojom::MojoCompositorFrameSink> request,
      cc::mojom::MojoCompositorFrameSinkClientPtr client);

  ServerWindow* window() { return window_; }

  ServerWindowCompositorFrameSink* GetDefaultCompositorFrameSink() const;
  ServerWindowCompositorFrameSink* GetUnderlayCompositorFrameSink() const;
  ServerWindowCompositorFrameSink* GetCompositorFrameSinkByType(
      mojom::CompositorFrameSinkType type) const;
  bool HasCompositorFrameSinkOfType(mojom::CompositorFrameSinkType type) const;
  bool HasAnyCompositorFrameSink() const;

  cc::SurfaceManager* GetCompositorFrameSinkManager();

 private:
  friend class ServerWindowCompositorFrameSinkManagerTestApi;
  friend class ServerWindowCompositorFrameSink;

  // Returns true if a CompositorFrameSink of |type| has been set and has
  // received a frame that is greater than the size of the window.
  bool IsCompositorFrameSinkReadyAndNonEmpty(
      mojom::CompositorFrameSinkType type) const;

  ServerWindow* window_;

  using TypeToCompositorFrameSinkMap =
      std::map<mojom::CompositorFrameSinkType,
               std::unique_ptr<ServerWindowCompositorFrameSink>>;

  TypeToCompositorFrameSinkMap type_to_compositor_frame_sink_map_;

  // While true the window is not drawn. This is initially true if the window
  // has the property |kWaitForUnderlay_Property|. This is set to false once
  // the underlay and default surface have been set *and* their size is at
  // least that of the window. Ideally we would wait for sizes to match, but
  // the underlay is not necessarily as big as the window.
  bool waiting_for_initial_frames_;

  DISALLOW_COPY_AND_ASSIGN(ServerWindowCompositorFrameSinkManager);
};

}  // namespace ws
}  // namespace ui

#endif  // SERVICES_UI_WS_SERVER_WINDOW_SURFACE_MANAGER_H_
