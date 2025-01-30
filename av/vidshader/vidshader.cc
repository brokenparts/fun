#include "common_core.hh"
#include "common_math.hh"

extern "C" {
  #include <libavcodec/avcodec.h>
  #include <libavformat/avformat.h>
  #include <libswscale/swscale.h>
}

#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <glad/glad.h>

struct Vertex {
  Vec2 position;
  Vec2 texcoord;
};

const char* VERTEX_SHADER = R"""(#version 330 core

layout (location = 0) in vec2 v_position;
layout (location = 1) in vec2 v_texcoord;

out vec2 texcoord;

void main() {
  texcoord = vec2(v_texcoord.x, 1.0f - v_texcoord.y);
  gl_Position = vec4(v_position.x, v_position.y, -1.0f, 1.0f);
}
)""";

const char* DEFAULT_FRAGMENT_SHADER = R"""(#version 330 core

in vec2 texcoord;

out vec4 color;

uniform sampler2D u_sampler;

void main() {
  color = vec4(texture(u_sampler, texcoord).rgb, 1.0f);
}

)""";

GLuint LoadAndCompileShader(GLenum type, const char* source) {
  GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &source, 0);
  glCompileShader(shader);
  GLint status = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
  if (!status) {
    char log[1024] = { };
    glGetShaderInfoLog(shader, sizeof(log), 0, log);
    SDL_Log("Failed to compile shader: %s", log);
    exit(1);
    return -1;
  }
  return shader;
}

GLuint LoadAndCompileProgram(const char* vert_src, const char* frag_src) {
  GLuint prog = glCreateProgram();
  glAttachShader(prog, LoadAndCompileShader(GL_VERTEX_SHADER, vert_src));
  glAttachShader(prog, LoadAndCompileShader(GL_FRAGMENT_SHADER, frag_src));
  glLinkProgram(prog);
  GLint status = 0;
  glGetProgramiv(prog, GL_LINK_STATUS, &status);
  if (!status) {
    SDL_Log("Failed to link program");
    exit(1);
    return -1;
  }
  return prog;
};

static struct {
  SDL_Window* wnd;
  SDL_GLContext gl;
  GLuint quad_vao;
  GLuint quad_vbo;
  GLuint quad_ibo;
  GLuint program;
  GLuint program_default;
  GLuint texture;
  Vec2 resolution;
  AVFormatContext* avfc;
  int avfc_video_stream;
  const AVCodec* avc;
  AVCodecContext* avcc;
  SwsContext* sws_ctx;
  bool paused;
  bool show_original;
} g = { };

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
  if (argc != 3) {
    SDL_Log("Usage: vidshader <video path> <shader path>");
    return SDL_APP_FAILURE;
  }

  const char* fragment_shader = (const char*)SDL_LoadFile(argv[2], 0);
  if (!fragment_shader) {
    SDL_Log("Failed to load file %s", argv[1]);
    return SDL_APP_FAILURE;
  }

  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
    SDL_Log("Failed to initialize SDL: %s", SDL_GetError());
  }
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
  if (!(g.wnd = SDL_CreateWindow(__FILE__, 800, 600, 0))) {
    SDL_Log("Failed to create SDL window: %s", SDL_GetError());
  }
  if (!(g.gl = SDL_GL_CreateContext(g.wnd))) {
    SDL_Log("Failed to create OpenGL context for window: %s", SDL_GetError());
  }

  if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
    SDL_Log("Failed to load OpenGL functinos");
  }

  const Vertex quad_verts[] = {
    { Vec2(-1.0f,  1.0f), Vec2(0.0f, 1.0f) }, // top-left
    { Vec2( 1.0f,  1.0f), Vec2(1.0f, 1.0f) }, // top-right
    { Vec2( 1.0f, -1.0f), Vec2(1.0f, 0.0f) }, // bottom-right
    { Vec2(-1.0f, -1.0f), Vec2(0.0f, 0.0f) }, // bottom-left
  };

  const u16 quad_indices[] = {
    0, 1, 2,
    0, 2, 3,
  };

  glGenVertexArrays(1, &g.quad_vao);
  glBindVertexArray(g.quad_vao);

  glGenBuffers(1, &g.quad_vbo);
  glBindBuffer(GL_ARRAY_BUFFER, g.quad_vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(quad_verts), quad_verts, GL_STATIC_DRAW);

  glGenBuffers(1, &g.quad_ibo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g.quad_ibo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(quad_indices), quad_indices, GL_STATIC_DRAW);

  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texcoord));
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);

  g.program = LoadAndCompileProgram(VERTEX_SHADER, fragment_shader);
  glUseProgram(g.program);

  g.program_default = LoadAndCompileProgram(VERTEX_SHADER, DEFAULT_FRAGMENT_SHADER);

  int ret = avformat_open_input(&g.avfc, argv[1], 0, 0);
  assert(ret == 0);
  avformat_find_stream_info(g.avfc, 0);

  av_dump_format(g.avfc, 0, argv[1], 0);

  g.avfc_video_stream = -1;
  for (unsigned int i = 0; i < g.avfc->nb_streams; ++i) {
    const AVStream* stream = g.avfc->streams[i];
    if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
      g.avc = avcodec_find_decoder(stream->codecpar->codec_id);
      assert(g.avc);
      SDL_Log("Found decoder: %s", g.avc->long_name);
      g.avfc_video_stream = i;
      break;
    }
  }
  assert(g.avfc_video_stream >= 0);
  const AVStream* avs = g.avfc->streams[g.avfc_video_stream];

  g.avcc = avcodec_alloc_context3(g.avc);
  assert(g.avcc);

  ret = avcodec_parameters_to_context(g.avcc, g.avfc->streams[g.avfc_video_stream]->codecpar);
  assert(ret == 0);

  ret = avcodec_open2(g.avcc, g.avc, 0);
  assert(ret == 0);

  AVCodecParameters* cpar = avs->codecpar;
  g.sws_ctx = sws_getContext(cpar->width, cpar->height, (AVPixelFormat)cpar->format,
                             cpar->width, cpar->height, AV_PIX_FMT_RGB24,
                             SWS_BICUBIC, 0, 0, 0);
  assert(g.sws_ctx);

  g.resolution = Vec2(cpar->width, cpar->height);
  SDL_SetWindowSize(g.wnd, cpar->width, cpar->height);
  SDL_SetWindowPosition(g.wnd, 100, 100);

  glGenTextures(1, &g.texture);
  glBindTexture(GL_TEXTURE_2D, g.texture);

  return SDL_APP_CONTINUE;
}

void GetFrameTexture() {
  int ret = 0;

  // av_seek_frame(g.avfc, g.avfc_video_stream, 10, 0);

  // TEST: read 1 frame
  AVPacket* pkt = av_packet_alloc(); assert(pkt);
  AVFrame* frame = av_frame_alloc(); assert(frame);
  bool got_frame = false;
  while (!got_frame) {
    ret = av_read_frame(g.avfc, pkt);
    if (ret == AVERROR_EOF) {
      av_seek_frame(g.avfc, g.avfc_video_stream, 0, 0);
      return;
    }
    assert(ret == 0);
    // SDL_Log("+packet");

    if (pkt->stream_index == g.avfc_video_stream) {
      ret = avcodec_send_packet(g.avcc, pkt);
      assert(ret >= 0);
      ret = avcodec_receive_frame(g.avcc, frame);
      if (ret != AVERROR(EAGAIN)) {
        assert(ret == 0);
        // SDL_Log("+frame");
        got_frame = true;
      }
    }

    av_packet_unref(pkt);
  }

  AVFrame* frame2 = av_frame_alloc();
  frame2->width = frame->width;
  frame2->height = frame->height;
  frame2->format = AV_PIX_FMT_RGB24;
  av_frame_get_buffer(frame2, 0);

  u8* buffer = MemAllocZ<u8>(frame->width * frame->height * 3);
  u8* buffers[] = { buffer, 0, 0 };
  int rgb_strides[] = { frame->width * 3, 0, 0 };

  for (i32 i = 0; i < frame->width * frame->height * 3; ++i) {
    buffer[i] = 0xFF;
  }

  sws_scale(g.sws_ctx, frame->data, frame->linesize, 0, frame->height, buffers, rgb_strides);

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, frame->width, frame->height, 0, GL_RGB, GL_UNSIGNED_BYTE, buffer);
  glGenerateMipmap(GL_TEXTURE_2D);
}

SDL_AppResult SDL_AppIterate(void* appstate) {
  if (!g.paused) {
    GetFrameTexture();
  }

  GLuint program = g.show_original ? g.program_default : g.program;

  glUseProgram(program);
  glUniform2fv(glGetUniformLocation(program, "u_resolution"), 1, &g.resolution.x);

  glViewport(0, 0, (GLsizei)g.resolution.x, (GLsizei)g.resolution.y);
  glClearColor(0.0f, 0.05, 0.06f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);

  SDL_GL_SwapWindow(g.wnd);
  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
  switch (event->type) {
  case SDL_EVENT_QUIT: {
    return SDL_APP_SUCCESS;
  } break;
  case SDL_EVENT_KEY_DOWN: {
    if (event->key.key == SDLK_SPACE) {
      g.paused = !g.paused;
    }
    else if (event->key.key == SDLK_TAB) {
      g.show_original = true;
    }
  } break;
  case SDL_EVENT_KEY_UP: {
    if (event->key.key == SDLK_TAB) {
      g.show_original = false;
    }
  } break;
  };

  return SDL_APP_CONTINUE;
}

void SDLCALL SDL_AppQuit(void* appstate, SDL_AppResult result) {
  SDL_Quit();
}
