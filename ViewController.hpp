#pragma once

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------

struct dvec2 {
  double x;
  double y;

  void  operator += (dvec2 b) { x += b.x; y += b.y; }
};

inline dvec2 operator +  (dvec2 a,  dvec2 b)  { return { a.x + b.x, a.y + b.y }; }
inline dvec2 operator +  (dvec2 a, double b)  { return { a.x + b,   a.y + b }; }
inline dvec2 operator +  (double a, dvec2 b)  { return { a + b.x, a + b.y }; }

inline dvec2 operator -  (dvec2 a,  dvec2 b)  { return { a.x - b.x, a.y - b.y }; }
inline dvec2 operator -  (dvec2 a,  double b) { return { a.x - b,   a.y - b }; }

inline dvec2 operator *  (dvec2 a,  double s) { return { a.x * s,   a.y * s }; }
inline dvec2 operator *  (double a, dvec2 b)  { return { a * b.x, a * b.y }; }

inline dvec2 operator /  (dvec2 a,  double s) { return { a.x / s,   a.y / s }; }

inline bool  operator == (dvec2 a, dvec2 b)   { return a.x == b.x && a.y == b.y; }

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------

struct Viewport {

  static Viewport from_center_zoom(dvec2 center, double zoom);
  double view_zoom() const;
  double scale_world_to_screen() const;
  double scale_screen_to_world() const;
  dvec2 screen_min(dvec2 screen_size) const;
  dvec2 screen_max(dvec2 screen_size) const;
  dvec2 world_center() const;
  bool operator ==(const Viewport& b) const;
  void translate_x(double d);
  void translate_y(double d);
  dvec2 world_to_screen(dvec2 v, dvec2 screen_size) const;
  dvec2 screen_to_world(dvec2 v, dvec2 screen_size) const;
  Viewport center_on(dvec2 c);
  Viewport zoom(dvec2 screen_pos, dvec2 screen_size, double zoom);
  Viewport pan(dvec2 delta);
  Viewport snap();
  Viewport ease(Viewport target, double delta);

  static Viewport screenspace(dvec2 screen_size) {
    return {
      screen_size * 0.5,
      0.0
    };
  }

  dvec2 _world_center = {};
  double _view_zoom = 0;
};

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------

struct ViewController {
  void init(dvec2 screen_size);
  void update(double delta);
  void on_mouse_wheel(dvec2 mouse_pos, dvec2 screen_size, double wheel);
  void pan(dvec2 delta_pos);
  void center_on(dvec2 c);

  Viewport view_target;
  Viewport view_target_snap;

  Viewport view_smooth;
  Viewport view_smooth_snap;
};

//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------