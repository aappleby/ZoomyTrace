#include "ViewController.hpp"

#include <stdint.h>
#include <math.h>
#include <stdio.h>

//-----------------------------------------------------------------------------

double ease(double a, double b, double dt) {
  if (a == b) return b;
  double t = 1.0 - pow(0.1, dt / 0.08);
  double c = a + (b - a) * t;

  // Snap to the endpoint if our easing doesn't get us closer to it due to
  // numerical precision issues.

  return c == a ? b : c;
}

dvec2 ease(dvec2 a, dvec2 b, double dt) {
  return dvec2(ease(a.x, b.x, dt), ease(a.y, b.y, dt));
}

//-----------------------------------------------------------------------------

dvec2 Viewport::world_min(dvec2 screen_size) const {
  return _center - screen_size * exp2(-_zoom) * 0.5;
}

dvec2 Viewport::world_max(dvec2 screen_size) const {
  return _center + screen_size * exp2(-_zoom) * 0.5;
};

bool Viewport::operator ==(const Viewport& b) const {
  return (_center == b._center) && (_zoom == b._zoom);
}

//-----------------------------------------------------------------------------

dvec2 Viewport::world_to_screen(dvec2 v, dvec2 screen_size) const {
  dvec2 screen_center = screen_size * 0.5;
  return (v - _center) * exp2(_zoom) + screen_center;
}

dvec2 Viewport::screen_to_world(dvec2 v, dvec2 screen_size) const {
  dvec2 screen_center = screen_size * 0.5;
  return (v - screen_center) * exp2(-_zoom) + _center;
}

//-----------------------------------------------------------------------------

Viewport Viewport::zoom(dvec2 screen_pos, dvec2 screen_size, dvec2 delta_zoom) {

  auto screen_delta = (screen_pos - 0.5 * screen_size);

  auto old_zoom = _zoom;
  auto new_zoom = _zoom + delta_zoom;

  auto old_world_delta = screen_delta * exp2(-old_zoom);
  auto new_world_delta = screen_delta * exp2(-new_zoom);

  dvec2 new_center = _center + old_world_delta - new_world_delta;

  return Viewport(new_center, new_zoom);
}

//-----------------------------------------------------------------------------

Viewport Viewport::pan(dvec2 screen_delta) {
  dvec2 world_delta = screen_delta * exp2(-_zoom);
  return Viewport(_center - world_delta, _zoom);
}

//-----------------------------------------------------------------------------

Viewport Viewport::snap() {
  dvec2 ppw = exp2(floor(_zoom));
  dvec2 new_center = round(_center * ppw) / ppw;
  return Viewport(new_center, _zoom);
}

//-----------------------------------------------------------------------------

Viewport Viewport::ease(Viewport target, double dt) {
  auto& a = *this;
  auto& b = target;

  dvec2 center_e = ::ease(a._center, b._center, dt);

  dvec2 scale_a = exp2(-a._zoom);
  dvec2 scale_b = exp2(-b._zoom);
  dvec2 scale_e = ::ease(scale_a, scale_b, dt);

  dvec2 zoom_e  = -log2(scale_e);

  return Viewport(center_e, zoom_e);
}

//-----------------------------------------------------------------------------

void ViewController::init(dvec2 screen_size) {
  auto view_screen = Viewport(screen_size * 0.5, {0, 0});
  view_target = view_screen;
  view_target_snap = view_screen;
  view_smooth = view_screen;
  view_smooth_snap = view_screen;
}

void ViewController::update(double dt) {
  view_smooth = view_smooth.ease(view_target, dt);
  view_smooth_snap = view_smooth_snap.ease(view_target_snap, dt);
}

void ViewController::zoom(dvec2 mouse_pos, dvec2 screen_size, double delta_zoom) {
  view_target = view_target.zoom(mouse_pos, screen_size, {delta_zoom, delta_zoom});
  view_target_snap = view_target.snap();
}

void ViewController::pan(dvec2 delta_pos) {
  view_target = view_target.pan(delta_pos);
  view_target_snap = view_target.snap();
}

//-----------------------------------------------------------------------------
