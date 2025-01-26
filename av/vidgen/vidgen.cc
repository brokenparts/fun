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

constexpr u32 CANVAS_W = 800;
constexpr u32 CANVAS_H = 600;

constexpr u32 FPS = 60;

struct Pixel_RGB888 {
  u8 r;
  u8 g;
  u8 b;
};
static_assert(sizeof(Pixel_RGB888) == 3);

constexpr Pixel_RGB888 PIXEL_BLACK = { .r = 0x00, .g = 0x00, .b = 0x00 };
constexpr Pixel_RGB888 PIXEL_WHITE = { .r = 0xFF, .g = 0xFF, .b = 0xFF };

static struct {
  SDL_Window* wnd;
  SDL_Renderer* r;
  Vec2 cur_pos;
  f32 cur_radius;
  Pixel_RGB888* canvas;
  SDL_Texture* canvas_tex;
  bool recording;
  char filename[64];
  AVFormatContext* avfc;
  AVCodecContext* avcc;
  AVStream* avst;
  AVFrame* avframe_inp;
  AVFrame* avframe_out;
  AVPacket* avpkt;
  SwsContext* sws;
  u32 frame_num;
  u32 next_pts;
} g = { };

void REC_Begin() {
  g.recording = true;

  SDL_snprintf(g.filename, sizeof(g.filename), "vidgen-%u.mkv", (u32)time(0));

  // Alloc container
  int avret = avformat_alloc_output_context2(&g.avfc, 0, 0, g.filename);
  assert(avret >= 0 && g.avfc);

  // Container format
  const AVOutputFormat* avof = g.avfc->oformat;

  // Use h264 for video encoding
  const AVCodec* avc = avcodec_find_encoder(AV_CODEC_ID_H264);
  assert(avc);

  // Set up encoder
  g.avcc = avcodec_alloc_context3(avc);
  assert(g.avcc);
  g.avcc->codec_id = avc->id;
  g.avcc->bit_rate = 400000;
  g.avcc->width = CANVAS_W;
  g.avcc->height = CANVAS_H;
  g.avcc->time_base = { 1, FPS };
  g.avcc->gop_size = 12;
  g.avcc->pix_fmt = AV_PIX_FMT_YUV420P;
  if (avof->flags & AVFMT_GLOBALHEADER) {
    g.avcc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
  }

  // Open encoder
  avret = avcodec_open2(g.avcc, avc, 0);
  assert(avret >= 0);

  // Create video stream
  g.avst = avformat_new_stream(g.avfc, avc);
  assert(g.avst);
  g.avst->time_base = g.avcc->time_base;
  g.avst->id = g.avfc->nb_streams - 1;

  // Copy stream parameters
  avret = avcodec_parameters_from_context(g.avst->codecpar, g.avcc);
  assert(avret >= 0);

  // Input frame
  g.avframe_inp = av_frame_alloc();
  assert(g.avframe_inp);
  g.avframe_inp->format = AV_PIX_FMT_RGB24;
  g.avframe_inp->width = CANVAS_W;
  g.avframe_inp->height = CANVAS_H;
  avret = av_frame_get_buffer(g.avframe_inp, 0);
  assert(avret >= 0);

  // Output frame
  g.avframe_out = av_frame_alloc();
  assert(g.avframe_out);
  g.avframe_out->format = g.avcc->pix_fmt;
  g.avframe_out->width = CANVAS_W;
  g.avframe_out->height = CANVAS_H;
  avret = av_frame_get_buffer(g.avframe_out, 0);
  assert(avret >= 0);

  // Transient packet
  g.avpkt = av_packet_alloc();
  assert(g.avpkt);

  // Set up scaling
  g.sws = sws_getContext(g.avframe_inp->width, g.avframe_inp->height, (AVPixelFormat)g.avframe_inp->format,
                         g.avframe_out->width, g.avframe_out->height, (AVPixelFormat)g.avframe_out->format,
                         SWS_BICUBIC, 0, 0, 0);
  assert(g.sws);

  // Dump debug
  av_dump_format(g.avfc, 0, g.filename, 1);

  // Open file
  avret = avio_open(&g.avfc->pb, g.filename, AVIO_FLAG_WRITE);
  assert(avret >= 0);
  avret = avformat_write_header(g.avfc, 0);
  assert(avret >= 0);
}

void REC_End() {
  g.recording = false;

  // Write trailer
  int avret = av_write_trailer(g.avfc);
  assert(avret >= 0);

  avio_closep(&g.avfc->pb);

  av_packet_free(&g.avpkt);
  av_frame_free(&g.avframe_out);
  av_frame_free(&g.avframe_inp);
  avcodec_free_context(&g.avcc);
  avformat_free_context(g.avfc);
}

bool REC_EncodeFrame() {
  ++g.frame_num;

  // Convert to YUV frame
  int avret = av_frame_make_writable(g.avframe_out);
  assert(avret >= 0);
  sws_scale(g.sws, (const u8* const*)&g.canvas, g.avframe_inp->linesize, 0,
    g.avframe_out->height, g.avframe_out->data, g.avframe_out->linesize);
  g.avframe_out->pts = g.next_pts++;

  // Encode into packets
  avret = avcodec_send_frame(g.avcc, g.avframe_out);
  assert(avret >= 0);

  while (avret >= 0) {
    avret = avcodec_receive_packet(g.avcc, g.avpkt);
    if (avret == AVERROR(EAGAIN) || avret == AVERROR_EOF) {
      break;
    } else {
      assert(avret >= 0);
    }

    av_packet_rescale_ts(g.avpkt, g.avcc->time_base, g.avst->time_base);
    g.avpkt->stream_index = g.avst->index;

    avret = av_interleaved_write_frame(g.avfc, g.avpkt);
    assert(avret >= 0);
    SDL_Log("Encoded frame");
  }

  return avret == AVERROR_EOF ? true : false;
}

void REC_Frame() {
  // bool enc = true;
  // while (enc) {
  //   enc = !REC_EncodeFrame();
  // }
  REC_EncodeFrame();
}

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
    SDL_Log("Failed to initialize SDL: %s", SDL_GetError());
  }
  if (!(g.wnd = SDL_CreateWindow(__FILE__, CANVAS_W, CANVAS_H, 0))) {
    SDL_Log("Failed to create SDL window: %s", SDL_GetError());
  }
  if (!(g.r = SDL_CreateRenderer(g.wnd, 0))) {
    SDL_Log("Failed to create SDL renderer: %s", SDL_GetError());
  }
  SDL_SetRenderVSync(g.r, 1);

  g.cur_radius = 10.0f;

  g.canvas = MemAlloc<Pixel_RGB888>(CANVAS_W * CANVAS_H);
  for (u32 i = 0; i < CANVAS_W * CANVAS_H; ++i) {
    g.canvas[i] = PIXEL_BLACK;
  }

  g.canvas_tex = SDL_CreateTexture(g.r, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, CANVAS_W, CANVAS_H);
  if (!g.canvas_tex) {
    SDL_Log("Failed to create SDL texture: %s", SDL_GetError());
  }

  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
  // Update canvas
  if (SDL_GetRelativeMouseState(0, 0) & SDL_BUTTON_MASK(1)) {
    Vec2 tl = g.cur_pos - Vec2(g.cur_radius, g.cur_radius);
    tl.x = Max(tl.x, 0.0f);
    tl.y = Max(tl.y, 0.0f);
    Vec2 br = g.cur_pos + Vec2(g.cur_radius, g.cur_radius);
    br.x = Min(br.x, (f32)(CANVAS_W - 1));
    br.y = Min(br.y, (f32)(CANVAS_H - 1));
    for (u32 y = (u32)tl.y; y <= (u32)br.y; ++y) {
      Pixel_RGB888* row = &g.canvas[y * CANVAS_W];
      for (u32 x = (u32)tl.x; x <= (u32)br.x; ++x) {
        row[x] = PIXEL_WHITE;
      }
    }
  }

  // Render to file
  if (g.recording) {
    REC_Frame();
  }

  // Draw canvas
  void* texture = 0;
  int texture_pitch = 0; // why isnt this an optional parameter?
  if (!SDL_LockTexture(g.canvas_tex, 0, &texture, &texture_pitch)) {
    SDL_Log("Failed to lock texture: %s", SDL_GetError());
  }
  SDL_memcpy(texture, g.canvas, sizeof(Pixel_RGB888) * CANVAS_W * CANVAS_H);
  SDL_UnlockTexture(g.canvas_tex);
  SDL_RenderTexture(g.r, g.canvas_tex, 0, 0);

  // Draw cursor
  SDL_SetRenderDrawColor(g.r, 0x00, 0xFF, 0x00, 0xFF);
  SDL_FRect cur_rect = {
    .x = g.cur_pos.x - g.cur_radius,
    .y = g.cur_pos.y - g.cur_radius,
    .w = g.cur_radius * 2.0f,
    .h = g.cur_radius * 2.0f,
  };
  SDL_RenderRect(g.r, &cur_rect);

  if (!g.recording) {
    SDL_RenderDebugText(g.r, 2, 2, "Status: Not recording. Press SPACE to start.");
  } else {
    SDL_RenderDebugTextFormat(g.r, 2, 2, "Status: Recording to %s. Press SPACE to stop", g.filename);
  }

  SDL_RenderPresent(g.r);
  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
  switch (event->type) {
  case SDL_EVENT_QUIT: {
    return SDL_APP_SUCCESS;
  } break;
  case SDL_EVENT_MOUSE_MOTION: {
    g.cur_pos.x = event->motion.x;
    g.cur_pos.y = event->motion.y;
  } break;
  case SDL_EVENT_KEY_DOWN: {
    if (event->key.key == SDLK_SPACE) {
      if (g.recording) {
        REC_End();
      } else {
        REC_Begin();
      }
    }
  } break;
  };

  return SDL_APP_CONTINUE;
}

void SDLCALL SDL_AppQuit(void* appstate, SDL_AppResult result) {
  SDL_Quit();
}
