#pragma once

#include "symlinks/glm/glm/glm.hpp"
using namespace glm;

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------

struct Viewport {

  Viewport() {
    _center = {0,0};
    _zoom = {0,0};
  }

  Viewport(dvec2 center, dvec2 zoom) {
    _center = center;
    _zoom = zoom;
  }

  bool operator == (const Viewport& b) const;

  dvec2 world_min(dvec2 screen_size) const;
  dvec2 world_max(dvec2 screen_size) const;
  dvec2 world_to_screen(dvec2 v, dvec2 screen_size) const;
  dvec2 screen_to_world(dvec2 v, dvec2 screen_size) const;

  Viewport zoom(dvec2 screen_pos, dvec2 screen_size, dvec2 delta_zoom);
  Viewport pan(dvec2 delta_pos);
  Viewport snap();
  Viewport ease(Viewport target, double dt);

  dvec2 _center;
  dvec2 _zoom;
};

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------

struct ViewController {
  void init(dvec2 screen_size);
  void update(double dt);

  void zoom(dvec2 mouse_pos, dvec2 screen_size, double delta_zoom);
  void pan (dvec2 delta_pos);

  Viewport view_target;
  Viewport view_target_snap;

  Viewport view_smooth;
  Viewport view_smooth_snap;
};

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------
