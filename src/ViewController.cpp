#include "ViewController.hpp"

#include <stdint.h>
#include <math.h>
#include <stdio.h>

//-----------------------------------------------------------------------------

double ease(double a, double b, double delta) {
  if (a == b) return b;
  double t = 1.0 - pow(0.1, delta / 0.08);
  double c = a + (b - a) * t;

  // We want to snap to the endpoint if our easing doesn't get us closer to it.
  // We were checking float(c) == float(a), but that causes precision issues
  // when we're easing between viewports far from the origin.
  return (c == a) ? b : c;
}

dvec2 ease(dvec2 a, dvec2 b, double delta) {
  return {
    ease(a.x, b.x, delta),
    ease(a.y, b.y, delta),
  };
}

Viewport ease(Viewport a, Viewport b, double delta) {
  // Easing the viewport directly doesn't work well, but you can uncomment it if you want. :D
  /*
  return Viewport{
    ease(a._world_center, b._world_center, delta),
    ease(a._view_zoom, b._view_zoom, delta)
  };
  */

  dvec2 world_min_a = a.world_center() - a.scale_screen_to_world() * 0.5;
  dvec2 world_min_b = b.world_center() - b.scale_screen_to_world() * 0.5;

  dvec2 world_max_a = a.world_center() + a.scale_screen_to_world() * 0.5;
  dvec2 world_max_b = b.world_center() + b.scale_screen_to_world() * 0.5;

  dvec2 world_min = ease(world_min_a, world_min_b, delta);
  dvec2 world_max = ease(world_max_a, world_max_b, delta);

  double zoom_x = -log2(world_max.x - world_min.x);
  double zoom_y = -log2(world_max.y - world_min.y);

  return Viewport{
    (world_min + world_max) * 0.5,
    {zoom_x, zoom_y}
  };
}

//#pragma warning(disable : 4201)
union double_funtimes {
  double value;
  struct {
    uint64_t mantissa : 52;
    uint64_t exponent : 11;
    uint64_t sign : 1;
  };
};

//-----------------------------------------------------------------------------

Viewport Viewport::from_center_zoom(dvec2 center, dvec2 zoom) {
  return Viewport{
    center,
    zoom
  };
}

dvec2 Viewport::view_zoom() const {
  return _view_zoom;
}

dvec2 Viewport::scale_world_to_screen() const {
  return {
    exp2(_view_zoom.x),
    exp2(_view_zoom.y),
  };
}

dvec2 Viewport::scale_screen_to_world() const {
  return {
    1.0 / exp2(_view_zoom.x),
    1.0 / exp2(_view_zoom.y),
  };
}

dvec2 Viewport::screen_min(dvec2 screen_size) const {
  return {
    _world_center.x - screen_size.x * scale_screen_to_world().x * 0.5,
    _world_center.y - screen_size.y * scale_screen_to_world().y * 0.5
  };
}

dvec2 Viewport::screen_max(dvec2 screen_size) const {
  return {
    _world_center.x + screen_size.x * scale_screen_to_world().x * 0.5,
    _world_center.y + screen_size.y * scale_screen_to_world().y * 0.5
  };
};

dvec2 Viewport::world_center() const {
  return _world_center;
}

bool Viewport::operator ==(const Viewport& b) const {
  return (_world_center == b._world_center) && (_view_zoom == b._view_zoom);
}

void Viewport::translate_x(double d) {
  _world_center.x += d;
}

void Viewport::translate_y(double d) {
  _world_center.y += d;
}

//-----------------------------------------------------------------------------

dvec2 Viewport::world_to_screen(dvec2 v, dvec2 screen_size) const {
  dvec2 screen_center = screen_size * 0.5;
  return (v - world_center()) * scale_world_to_screen() + screen_center;
}

dvec2 Viewport::screen_to_world(dvec2 v, dvec2 screen_size) const {
  dvec2 screen_center = screen_size * 0.5;
  return (v - screen_center) * scale_screen_to_world() + world_center();
}

//-----------------------------------------------------------------------------

Viewport Viewport::center_on(dvec2 c) {
  return Viewport::from_center_zoom(c, {3.0, 3.0});
}

//-----------------------------------------------------------------------------

Viewport Viewport::zoom(dvec2 screen_pos, dvec2 screen_size, dvec2 zoom) {
  dvec2 old_zoom = view_zoom();
  dvec2 new_zoom = view_zoom() + zoom;

  dvec2 screen_center = 0.5 * screen_size;
  auto screen_delta = screen_pos - screen_center;

  dvec2 new_center = {
    world_center().x + screen_delta.x / (exp2(old_zoom.x)) - screen_delta.x / (exp2(new_zoom.x)),
    world_center().y + screen_delta.y / (exp2(old_zoom.y)) - screen_delta.y / (exp2(new_zoom.y)),
  };

  return Viewport::from_center_zoom(new_center, new_zoom);
}

//-----------------------------------------------------------------------------

Viewport Viewport::pan(dvec2 screen_delta) {
  //printf("screen delta %f %f\n", screen_delta.x, screen_delta.y);
  dvec2 world_delta = screen_delta * scale_screen_to_world();
  //printf("world_delta %f %f\n", world_delta.x, world_delta.y);

  return Viewport::from_center_zoom(world_center() - world_delta, view_zoom());
}

//-----------------------------------------------------------------------------

Viewport Viewport::snap() {
  dvec2 zoom1 = view_zoom();
  dvec2 zoom2 = {
    round(zoom1.x * 4.0) / 4.0,
    round(zoom1.y * 4.0) / 4.0,
  };
  dvec2 ppw2 = {
    exp2(zoom2.x),
    exp2(zoom2.y)
  };

  dvec2 old_center = world_center();
  dvec2 new_center = {
    round(old_center.x * ppw2.x) / ppw2.x,
    round(old_center.y * ppw2.y) / ppw2.y
  };

  return Viewport::from_center_zoom(new_center, zoom2);
}

//-----------------------------------------------------------------------------

Viewport Viewport::ease(Viewport target, double delta) {
  return ::ease(*this, target, delta);
}

//-----------------------------------------------------------------------------




//-----------------------------------------------------------------------------

void ViewController::init(dvec2 screen_size) {
  auto view_screen = Viewport::screenspace(screen_size);
  view_target = view_screen;
  view_target_snap = view_screen;
  view_smooth = view_screen;
  view_smooth_snap = view_screen;
}

void ViewController::update(double delta) {
  view_smooth = view_smooth.ease(view_target, delta);
  view_smooth_snap = view_smooth_snap.ease(view_target_snap, delta);
}

void ViewController::on_mouse_wheel(dvec2 mouse_pos, dvec2 screen_size, double wheel) {
  view_target = view_target.zoom(mouse_pos, screen_size, {wheel, wheel});
  view_target_snap = view_target.snap();
  //printf("new view zoom %f %f\n", view_target_snap._view_zoom.x, view_target_snap._view_zoom.y);
}

void ViewController::pan(dvec2 delta_pos) {
  view_target = view_target.pan(delta_pos);
  view_target_snap = view_target.snap();
}

void ViewController::center_on(dvec2 c) {
  view_target = view_target.center_on(c);
  view_target_snap = view_target.snap();
}
