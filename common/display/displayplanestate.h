﻿/*
// Copyright (c) 2016 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/

#ifndef COMMON_DISPLAY_DISPLAYPLANESTATE_H_
#define COMMON_DISPLAY_DISPLAYPLANESTATE_H_

#include <stdint.h>
#include <vector>

#include "overlaylayer.h"
#include "compositionregion.h"

namespace hwcomposer {

class DisplayPlane;
class DisplayPlaneState;
class NativeSurface;
struct OverlayLayer;

typedef std::vector<DisplayPlaneState> DisplayPlaneStateList;

class DisplayPlaneState {
 public:
  enum class State : int32_t { kScanout, kRender };

  DisplayPlaneState() = default;
  DisplayPlaneState(DisplayPlaneState &&rhs) = default;
  DisplayPlaneState &operator=(DisplayPlaneState &&other) = default;
  DisplayPlaneState(DisplayPlane *plane, OverlayLayer *layer, uint32_t index)
      : plane_(plane), layer_(layer) {
    source_layers_.emplace_back(index);
    display_frame_ = layer->GetDisplayFrame();
    if (layer->IsCursorLayer()) {
      cursor_layers_.emplace_back(index);
      type_ = PlaneType::kCursor;
    }
  }

  explicit DisplayPlaneState(DisplayPlane *plane) : plane_(plane) {
  }

  State GetCompositionState() const {
    return state_;
  }

  const HwcRect<int> &GetDisplayFrame() const {
    return display_frame_;
  }

  void AddLayer(size_t index, const HwcRect<int> &display_frame,
                bool cursor_layer) {
    display_frame_.left = std::min(display_frame_.left, display_frame.left);
    display_frame_.top = std::min(display_frame_.top, display_frame.top);
    display_frame_.right = std::max(display_frame_.right, display_frame.right);
    display_frame_.bottom =
        std::max(display_frame_.bottom, display_frame.bottom);

    source_layers_.emplace_back(index);
    state_ = State::kRender;

    if (cursor_layer) {
      cursor_layers_.emplace_back(index);
    }

    if (source_layers_.size() == cursor_layers_.size()) {
      type_ = PlaneType::kCursor;
    } else {
      type_ = PlaneType::kNormal;
    }
  }

  void AddLayers(const std::vector<size_t> &source_layers,
                 const HwcRect<int> &display_frame, State state) {
    for (const int &index : source_layers) {
      source_layers_.emplace_back(index);
    }

    display_frame_.left = display_frame.left;
    display_frame_.top = display_frame.top;
    display_frame_.right = display_frame.right;
    display_frame_.bottom = display_frame.bottom;
    state_ = state;
    type_ = PlaneType::kNormal;
    std::vector<size_t>().swap(cursor_layers_);
  }

  // This API should be called only when Cursor layer is being
  // added, is part of layers displayed by plane or is being
  // removed in this frame. AddLayers should be used in all
  // other cases.
  void AddLayersForCursor(const std::vector<size_t> &source_layers,
                          const std::vector<size_t> &cursor_layers,
                          const HwcRect<int> &display_frame, State state,
                          bool ignore_cursor_layer) {
    if (ignore_cursor_layer) {
      for (const int &index : source_layers) {
        bool ignore_layer = false;
        for (const int &cindex : cursor_layers) {
          if (cindex == index) {
            ignore_layer = true;
            break;
          }
        }

        if (ignore_layer) {
          continue;
        }

        source_layers_.emplace_back(index);
      }

      type_ = PlaneType::kNormal;
    } else {
      for (const int &index : source_layers) {
        source_layers_.emplace_back(index);
      }

      for (const int &cindex : cursor_layers) {
        cursor_layers_.emplace_back(cindex);
      }

      if (source_layers_.size() == cursor_layers_.size()) {
        type_ = PlaneType::kCursor;
      } else {
        type_ = PlaneType::kNormal;
      }
    }

    display_frame_.left = display_frame.left;
    display_frame_.top = display_frame.top;
    display_frame_.right = display_frame.right;
    display_frame_.bottom = display_frame.bottom;
    state_ = state;
  }

  void UpdateDisplayFrame(const HwcRect<int> &display_frame) {
    display_frame_.left = std::min(display_frame_.left, display_frame.left);
    display_frame_.top = std::min(display_frame_.top, display_frame.top);
    display_frame_.right = std::max(display_frame_.right, display_frame.right);
    display_frame_.bottom =
        std::max(display_frame_.bottom, display_frame.bottom);
  }

  void ForceGPURendering() {
    state_ = State::kRender;
  }

  void SetOverlayLayer(const OverlayLayer *layer) {
    layer_ = layer;
    display_frame_ = layer->GetDisplayFrame();
  }

  void ReUseOffScreenTarget() {
    state_ = State::kScanout;
    recycled_surface_ = true;
  }

  bool SurfaceRecycled() const {
    return recycled_surface_;
  }

  const OverlayLayer *GetOverlayLayer() const {
    return layer_;
  }

  void SetOffScreenTarget(NativeSurface *target) {
    surfaces_.emplace(surfaces_.begin(), target);
  }

  NativeSurface *GetOffScreenTarget() const {
    if (surfaces_.size() == 0) {
      return NULL;
    }

    return surfaces_.at(0);
  }

  void TransferSurfaces(DisplayPlaneState &plane_state,
                        bool swap_front_buffer) {
    size_t size = surfaces_.size();
    if (size < 3 || !swap_front_buffer) {
      // Lets make sure front buffer is now back in the list.
      for (uint32_t i = 0; i < size; i++) {
        plane_state.surfaces_.emplace_back(surfaces_.at(i));
      }
    } else {
      plane_state.surfaces_.emplace_back(surfaces_.at(1));
      plane_state.surfaces_.emplace_back(surfaces_.at(2));
      plane_state.surfaces_.emplace_back(surfaces_.at(0));
    }
  }

  const std::vector<NativeSurface *> &GetSurfaces() const {
    return surfaces_;
  }

  std::vector<NativeSurface *> &GetSurfaces() {
    return surfaces_;
  }

  DisplayPlane *plane() const {
    return plane_;
  }

  const std::vector<size_t> &source_layers() const {
    return source_layers_;
  }

  std::vector<CompositionRegion> &GetCompositionRegion() {
    return composition_region_;
  }

  const std::vector<CompositionRegion> &GetCompositionRegion() const {
    return composition_region_;
  }

  void DisableClearSurface() {
    clear_surface_ = false;
  }

  bool ClearSurface() const {
    return clear_surface_;
  }

  bool IsCursorPlane() const {
    return type_ == PlaneType::kCursor;
  }

  const std::vector<size_t> &GetCursorLayers() const {
    return cursor_layers_;
  }

  bool IsVideoPlane() const {
    return type_ == PlaneType::kVideo;
  }

  void SetVideoPlane() {
#ifdef USE_LIBVA
    type_ = PlaneType::kVideo;
#endif
  }

 private:
  enum class PlaneType : int32_t {
    kCursor,  // Plane is compositing only Cursor.
    kVideo,   // Plane is compositing only Media content.
    kNormal   // Plane is compositing different types of content.
  };
  State state_ = State::kScanout;
  DisplayPlane *plane_ = NULL;
  const OverlayLayer *layer_ = NULL;
  HwcRect<int> display_frame_;
  std::vector<size_t> source_layers_;
  std::vector<size_t> cursor_layers_;
  std::vector<CompositionRegion> composition_region_;
  bool recycled_surface_ = false;
  bool clear_surface_ = true;
  std::vector<NativeSurface *> surfaces_;
  PlaneType type_ = PlaneType::kNormal;
};

}  // namespace hwcomposer
#endif  // COMMON_DISPLAY_DISPLAYPLANESTATE_H_
