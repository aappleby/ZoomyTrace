#include "GLBase.h"
#include "symlinks/glad/glad.h"
#include <SDL2/SDL.h>
#include <assert.h>
#include <map>

static bool is_nvidia = false;

void log(const char* format, ...);

//------------------------------------------------------------------------------

void APIENTRY debugOutput(GLenum source, GLenum type, GLuint id, GLenum severity,
                          GLsizei length, const GLchar* message, const GLvoid* userParam) {
  (void)source;
  (void)type;
  (void)id;
  (void)severity;
  (void)length;
  (void)message;
  (void)userParam;

  static std::map<uint32_t, const char*> messageMap = {
    {GL_DEBUG_SOURCE_API,               "API" },
    {GL_DEBUG_SOURCE_APPLICATION,       "APPLICATION" },
    {GL_DEBUG_SOURCE_OTHER,             "OTHER" },
    {GL_DEBUG_SOURCE_SHADER_COMPILER,   "SHADER_COMPILER" },
    {GL_DEBUG_SOURCE_THIRD_PARTY,       "THIRD_PARTY" },
    {GL_DEBUG_SOURCE_WINDOW_SYSTEM,     "WINDOW_SYSTEM" },

    {GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR, "DEPRECATED" },
    {GL_DEBUG_TYPE_ERROR,               "ERROR" },
    {GL_DEBUG_TYPE_MARKER,              "MARKER" },
    {GL_DEBUG_TYPE_OTHER,               "OTHER" },
    {GL_DEBUG_TYPE_PERFORMANCE,         "PERFORMANCE" },
    {GL_DEBUG_TYPE_POP_GROUP,           "POP_GROUP" },
    {GL_DEBUG_TYPE_PORTABILITY,         "PORTABILITY" },
    {GL_DEBUG_TYPE_PUSH_GROUP,          "PUSH_GROUP" },
    {GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR,  "UNDEFINED" },


    {GL_DEBUG_SEVERITY_HIGH,            "HIGH" },
    {GL_DEBUG_SEVERITY_MEDIUM,          "MEDIUM" },
    {GL_DEBUG_SEVERITY_LOW,             "LOW" },
    {GL_DEBUG_SEVERITY_NOTIFICATION,    "NOTIFICATION" },
  };

  // "will use VIDEO memory as the source..."
  if (id == 0x20071) return;

  log("GLDEBUG %.20s:%.20s:%.20s (0x%08x) %s",
    messageMap[source],
    messageMap[type],
    messageMap[severity],
    id,
    message);

  if (type == GL_DEBUG_TYPE_ERROR) {
    assert(false);
  }
}

//------------------------------------------------------------------------------

SDL_Window* create_gl_window(const char* name, int initial_screen_w, int initial_screen_h) {
  log("SDL_Init()");
  //SDL_Init(SDL_INIT_EVERYTHING);
  SDL_Init(SDL_INIT_VIDEO);
  log("SDL_Init() done");
  fflush(stdout);
  //SDL_Init(SDL_INIT_VIDEO);

  log("SDL_CreateWindow()");
  SDL_Window* window = SDL_CreateWindow(
    name,
    SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
    1920, 1080,
    SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI
  );
  log("SDL_CreateWindow() done");

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS,
                      SDL_GL_CONTEXT_DEBUG_FLAG
                      //| SDL_GL_CONTEXT_ROBUST_ACCESS_FLAG
                      //| SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG
                      );

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);

  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

  log("SDL_GL_CreateContext()");
  SDL_GLContext gl_context = SDL_GL_CreateContext((SDL_Window*)window);
  log("SDL_GL_CreateContext() done");

  SDL_GL_SetSwapInterval(1);  // Enable vsync
  //SDL_GL_SetSwapInterval(0); // Disable vsync

  log("gladLoadGLLoader()");
  gladLoadGLLoader(SDL_GL_GetProcAddress);
  log("gladLoadGLLoader() done");

  glEnable(GL_DEBUG_OUTPUT);
  glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
  glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
  glDebugMessageCallback(debugOutput, nullptr);

  glDisable(GL_CULL_FACE);

  int ext_count = 0;
  glGetIntegerv(GL_NUM_EXTENSIONS, &ext_count);

  auto vendor = (const char*)glGetString(GL_VENDOR);

  if (strncmp(vendor, "NVIDIA", 6) == 0) is_nvidia = true;

  log("Vendor:   %s", glGetString(GL_VENDOR));
  log("Renderer: %s", glGetString(GL_RENDERER));
  log("Version:  %s", glGetString(GL_VERSION));
  log("GLSL:     %s", glGetString(GL_SHADING_LANGUAGE_VERSION));
  log("Ext count %d", ext_count);

#if 0
  for (int i = 0; i < ext_count; i++) {
    log("Ext %2d: %s", i, glGetStringi(GL_EXTENSIONS, i));
  }
#endif

  //----------------------------------------
  // Set initial GL state

  glDisable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glClearColor(0.1, 0.1, 0.2, 0.0);
  glClearDepth(1.0);

  return window;
}

//-----------------------------------------------------------------------------

void* init_gl(void* window) {

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS,
                      SDL_GL_CONTEXT_DEBUG_FLAG
                      //| SDL_GL_CONTEXT_ROBUST_ACCESS_FLAG
                      //| SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG
                      );

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

  SDL_GLContext gl_context = SDL_GL_CreateContext((SDL_Window*)window);

  SDL_GL_SetSwapInterval(1);  // Enable vsync
  //SDL_GL_SetSwapInterval(0); // Disable vsync

  gladLoadGLLoader(SDL_GL_GetProcAddress);

  /*
  glEnable(GL_DEBUG_OUTPUT);
  glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
  glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
  glDebugMessageCallback(debugOutput, nullptr);
  */

  int ext_count = 0;
  glGetIntegerv(GL_NUM_EXTENSIONS, &ext_count);

  log("Vendor:   %s", glGetString(GL_VENDOR));
  log("Renderer: %s", glGetString(GL_RENDERER));
  log("Version:  %s", glGetString(GL_VERSION));
  log("GLSL:     %s", glGetString(GL_SHADING_LANGUAGE_VERSION));
  log("Ext count %d", ext_count);

#if 0
  for (int i = 0; i < ext_count; i++) {
    log("Ext %2d: %s", i, glGetStringi(GL_EXTENSIONS, i));
  }
#endif

  //----------------------------------------
  // Set initial GL state

  glDisable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glClearColor(0.1f, 0.1f, 0.2f, 0.f);
  //glClearDepthf(1.0);
  glClearDepth(1.0);

  return (void*)gl_context;
}

//-----------------------------------------------------------------------------

void check_gl_error() {
  int err = glGetError();
  if (err) {
    log("glGetError %d", err);
    assert(false);
  }
}

//-----------------------------------------------------------------------------

int create_vao() {
  int vao = 0;
  glGenVertexArrays(1, (GLuint*)&vao);
  glBindVertexArray(vao);
  return vao;
}

void bind_vao(int vao) {
  glBindVertexArray(vao);
}

//-----------------------------------------------------------------------------

int create_vbo() {
  int vbo = 0;
  glGenBuffers(1, (GLuint*)&vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  return vbo;
}

int create_vbo(int size_bytes, const void* data) {
  int vbo = create_vbo();
  glBufferData(GL_ARRAY_BUFFER, size_bytes, data, GL_DYNAMIC_DRAW);
  return vbo;
}

void update_vbo(int vbo, int size_bytes, const void* data) {
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, size_bytes, data, GL_DYNAMIC_DRAW);
}


//-----------------------------------------------------------------------------

int create_ibo() {
  int ibo = 0;
  glGenBuffers(1, (GLuint*)&ibo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
  return ibo;
}

void update_ibo(int ibo, int size_bytes, const uint16_t* data) {
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, size_bytes, data, GL_DYNAMIC_DRAW);
}

//-----------------------------------------------------------------------------

int create_ubo() {
  int ubo = 0;
  glGenBuffers(1, (GLuint*)&ubo);
  glBindBuffer(GL_UNIFORM_BUFFER, ubo);
  return ubo;
}

int create_ubo(int size_bytes) {
  int ubo = create_ubo();
  glBufferData(GL_UNIFORM_BUFFER, size_bytes, nullptr, GL_DYNAMIC_DRAW);
  return ubo;
}

void update_ubo(int ubo, int size_bytes, const void* data) {
  glBindBuffer(GL_UNIFORM_BUFFER, ubo);
  glBufferData(GL_UNIFORM_BUFFER, size_bytes, data, GL_DYNAMIC_DRAW);
}

void bind_ubo(int prog, const char* name, int index, int ubo) {
  glUniformBlockBinding(prog, glGetUniformBlockIndex(prog, name), index);
  glBindBufferBase(GL_UNIFORM_BUFFER, index, ubo);
}

//-----------------------------------------------------------------------------
// hasn't been tested yet beware

int create_ssbo(size_t size_bytes) {
  GLuint ssbo;
  glGenBuffers(1, &ssbo);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
  glBufferStorage(GL_SHADER_STORAGE_BUFFER, size_bytes, nullptr,
    GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT /*| GL_MAP_COHERENT_BIT*/ | GL_DYNAMIC_STORAGE_BIT);
  return (int)ssbo;
}

void update_ssbo(int ssbo, const void* data, size_t size_bytes) {
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
  glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, size_bytes, data);
}

void* map_ssbo(int ssbo, size_t size_bytes) {
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
  void* result = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, size_bytes,
    GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);
  return result;
}

void unmap_ssbo(int ssbo) {
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
  glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
}

void bind_ssbo(int ssbo, int binding, size_t offset, size_t size) {
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
  glBindBufferRange(GL_SHADER_STORAGE_BUFFER, binding, ssbo, offset, size);
}

void unbind_ssbo() {
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

//-----------------------------------------------------------------------------

int create_texture_u32(int width, int height, const void* data, bool filter) {
  int tex = 0;
  glGenTextures(1, (GLuint*)&tex);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter ? GL_LINEAR : GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter ? GL_LINEAR : GL_NEAREST);

  return tex;
}

//----------------------------------------

void update_texture_u32(int tex, int width, int height, const void* pix) {
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexSubImage2D(GL_TEXTURE_2D, 0,
                  0, 0, width, height,
                  GL_RGBA, GL_UNSIGNED_BYTE, pix);
}

//-----------------------------------------------------------------------------

int create_texture_u8(int width, int height, const void* data, bool filter) {
  int tex = 0;
  glGenTextures(1, (GLuint*)&tex);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, tex);

  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, data);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter ? GL_LINEAR : GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter ? GL_LINEAR : GL_NEAREST);

  return tex;
}

//----------------------------------------

void update_texture_u8(int tex, int width, int height, const void* pix) {
  update_texture_u8(tex, 0, 0, width, height, pix);
}

void update_texture_u8(int tex, int dx, int dy, int dw, int dh, const void* pix) {
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, tex);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexSubImage2D(GL_TEXTURE_2D, 0,
                  dx, dy, dw, dh,
                  GL_RED, GL_UNSIGNED_BYTE, pix);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
}

//-----------------------------------------------------------------------------

int create_table_u8(int width, int height, const void* data) {
  int tex = 0;
  glGenTextures(1, (GLuint*)&tex);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, tex);

  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_R8UI, width, height, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, data);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  return tex;
}

//----------------------------------------

void update_table_u8(int tex, int width, int height, const void* pix) {
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, tex);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexSubImage2D(GL_TEXTURE_2D, 0,
                  0, 0, width, height,
                  GL_RED_INTEGER, GL_UNSIGNED_BYTE, pix);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
}

//-----------------------------------------------------------------------------

int create_table_u32(int width, int height, const void* data) {
  int tex = 0;
  glGenTextures(1, (GLuint*)&tex);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, tex);

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8UI, width, height, 0, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, data);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  return tex;
}

//----------------------------------------

void update_table_u32(int tex, int width, int height, const void* pix) {
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexSubImage2D(GL_TEXTURE_2D, 0,
                  0, 0, width, height,
                  GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, pix);
}

//-----------------------------------------------------------------------------

void bind_texture(int prog, const char* name, int index, int tex) {
  glActiveTexture(GL_TEXTURE0 + index);
  glBindTexture(GL_TEXTURE_2D, tex);
  glUniform1i(glGetUniformLocation(prog, name), index);
}

void bind_table(int prog, const char* name, int index, int tex) {
  glActiveTexture(GL_TEXTURE0 + index);
  glBindTexture(GL_TEXTURE_2D, tex);
  glUniform1i(glGetUniformLocation(prog, name), index);
}

//-----------------------------------------------------------------------------

void dump_shader_info(int program) {
  int count = 0;

  glGetProgramiv(program, GL_ACTIVE_ATTRIBUTES, &count);
  log("  Active Attributes: %d", count);

  for (int i = 0; i < count; i++) {
    const int bufSize = 16;
    GLenum type;
    GLchar var_name[bufSize];
    GLsizei length;
    GLint size;
    glGetActiveAttrib(program, (GLuint)i, bufSize, &length, &size, &type, var_name);
    log("    Attribute #%d Type: %u Name: %s", i, type, var_name);
  }

  glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &count);
  log("  Active Uniforms: %d", count);

  for (int i = 0; i < count; i++) {
    const int bufSize = 16;
    GLenum type;
    GLchar var_name[bufSize];
    GLsizei length;
    GLint size;
    glGetActiveUniform(program, (GLuint)i, bufSize, &length, &size, &type, var_name);
    int loc = glGetUniformLocation(program, var_name);
    log("    Uniform '%16s' @ %2d Type: 0x%04x", var_name, loc, type);
  }

  glGetProgramiv(program, GL_ATTACHED_SHADERS, &count);
  log("  Attached shaders: %d", count);

  glGetProgramiv(program, GL_ACTIVE_UNIFORM_BLOCKS, &count);
  log("  Uniform blocks: %d", count);

  for (int i = 0; i < count; i++) {
    const int bufSize = 16;
    GLchar var_name[bufSize];
    GLsizei length;

    glGetActiveUniformBlockName(program, i, bufSize, &length, var_name);
    log("  Uniform block #%d Name: %s", i, var_name);

    int temp[16];

    glGetActiveUniformBlockiv(program, i, GL_UNIFORM_BLOCK_BINDING, temp);
    log("    GL_UNIFORM_BLOCK_BINDING %d", temp[0]);

    glGetActiveUniformBlockiv(program, i, GL_UNIFORM_BLOCK_DATA_SIZE, temp);
    log("    GL_UNIFORM_BLOCK_DATA_SIZE %d", temp[0]);

    glGetActiveUniformBlockiv(program, i, GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS, temp);
    log("    GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS %d", temp[0]);

    glGetActiveUniformBlockiv(program, i, GL_UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER, temp);
    log("    GL_UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER %d", temp[0]);

    glGetActiveUniformBlockiv(program, i, GL_UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER, temp);
    log("    GL_UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER %d", temp[0]);

    glGetActiveUniformBlockiv(program, i, GL_UNIFORM_BLOCK_REFERENCED_BY_COMPUTE_SHADER, temp);
    log("    GL_UNIFORM_BLOCK_REFERENCED_BY_COMPUTE_SHADER %d", temp[0]);
  }
}

//-----------------------------------------------------------------------------

int create_shader(const char* name, const char* src) {
  static bool verbose = false;
  assert(!glGetError());

  log("Compiling %s", name);

  auto vert_srcs = {
    "#version 460 core\n",
    "#extension GL_ARB_gpu_shader_int64 : enable\n",
    "precision highp float;\n",
    "precision highp int;\n",
    "precision highp usampler2D;\n",
    "#define _VERTEX_\n",
    src
  };

  int vertexShader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertexShader, (int)vert_srcs.size(), vert_srcs.begin(), NULL);
  glCompileShader(vertexShader);

  int vshader_result = 0;
  glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &vshader_result);

  if ((vshader_result == GL_FALSE) || verbose) {
    char buf[1024];
    int len = 0;
    glGetShaderInfoLog(vertexShader, 1024, &len, buf);
    log("  Vert shader log %s", buf);
  }

  auto frag_srcs = {
    "#version 460 core\n",
    "#extension GL_ARB_gpu_shader_int64 : enable\n",
    "precision highp float;\n",
    "precision highp int;\n",
    "precision highp usampler2D;\n",
    "#define _FRAGMENT_\n",
    src
  };
  int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragmentShader, (int)frag_srcs.size(), frag_srcs.begin(), NULL);
  glCompileShader(fragmentShader);

  int fshader_result = 0;
  glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &fshader_result);

  if ((fshader_result == GL_FALSE) || verbose) {
    char buf[1024];
    int len = 0;
    glGetShaderInfoLog(fragmentShader, 1024, &len, buf);
    log("  Frag shader log %s", buf);
  }

  int shaderProgram = glCreateProgram();
  glAttachShader(shaderProgram, vertexShader);
  glAttachShader(shaderProgram, fragmentShader);
  glLinkProgram(shaderProgram);
  glUseProgram(shaderProgram);

  int prog_result = 0;
  glGetProgramiv(shaderProgram, GL_LINK_STATUS, &prog_result);

  if((prog_result == GL_FALSE) || verbose) {
    char buf[1024];
    int len = 0;
    glGetProgramInfoLog(shaderProgram, 1024, &len, buf);
    log("  Shader program log %s", buf);

  }

  glDetachShader(shaderProgram, vertexShader);
  glDetachShader(shaderProgram, fragmentShader);
  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);

  if (verbose) {
    dump_shader_info(shaderProgram);
  }

  log("Compiling %s done", name);

  return shaderProgram;
}

//----------------------------------------

void bind_shader(int shader) {
  glUseProgram(shader);
}

//-----------------------------------------------------------------------------

int create_compute_shader(const char* name, const char* src) {
  static bool verbose = false;
  assert(!glGetError());

  log("Compiling %s", name);

  const char* layout = nullptr;

  if (is_nvidia) {
    layout = "layout(local_size_x = 32, local_size_y = 1, local_size_z = 1) in;\n";
  }
  else {
    layout = "layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;\n";
  }

  auto comp_srcs = {
    "#version 460 core\n",
    "#extension GL_ARB_gpu_shader_int64 : enable\n",
    //"#extension GL_EXT_shader_explicit_arithmetic_types : enable\n",
    "precision highp float;\n",
    "precision highp int;\n",
    "precision highp usampler2D;\n",
    "#define _COMPUTE_\n",
    layout,
    src
  };

  int compShader = glCreateShader(GL_COMPUTE_SHADER);
  glShaderSource(compShader, (int)comp_srcs.size(), comp_srcs.begin(), NULL);
  glCompileShader(compShader);

  int vshader_result = 0;
  glGetShaderiv(compShader, GL_COMPILE_STATUS, &vshader_result);

  if ((vshader_result == GL_FALSE) || verbose) {
    char buf[1024];
    int len = 0;
    glGetShaderInfoLog(compShader, 1024, &len, buf);
    log("  Comp shader log %s", buf);
  }

  int shaderProgram = glCreateProgram();
  glAttachShader(shaderProgram, compShader);
  glLinkProgram(shaderProgram);
  glUseProgram(shaderProgram);

  int prog_result = 0;
  glGetProgramiv(shaderProgram, GL_LINK_STATUS, &prog_result);

  if((prog_result == GL_FALSE) || verbose) {
    char buf[1024];
    int len = 0;
    glGetProgramInfoLog(shaderProgram, 1024, &len, buf);
    log("  Shader program log %s", buf);

  }

  glDetachShader(shaderProgram, compShader);
  glDeleteShader(compShader);

  //if (verbose) {
  if (1) {
    dump_shader_info(shaderProgram);
  }

  log("Compiling %s done", name);

  return shaderProgram;
}

//----------------------------------------

void bind_compute_shader(int shader) {
  glUseProgram(shader);
}

//-----------------------------------------------------------------------------
