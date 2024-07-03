#include "TracePainter.h"
#include "GLBase.h"
#include "glad/glad.h"

const char* trace_glsl = R"(

#ifdef _VERTEX_

layout(location = 0) in vec2 vpos;

void main() {
  gl_Position = vec4(vpos, 0.0, 1.0);
}

#else

out vec4 frag;

void main() {
  frag = vec4(1.0, 0.0, 1.0, 1.0);
}

#endif

)";


void TracePainter::init() {
  vao = create_vao();
  vbo = create_vbo();
  ibo = create_ibo();

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8, (void*)0);

  float verts[] = {
    0.0, 0.0,
    1.0, 0.0,
    1.0, 1.0,
    0.0, 0.0,
    1.0, 1.0,
    0.0, 1.0,
  };

  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, 8 * 6, verts, GL_STATIC_DRAW);
}

void TracePainter::update() {
}

void TracePainter::render() {
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glDrawArrays(GL_TRIANGLES, 0, 6);
}
