#include "common_core.hh"
#include "common_math.hh"

using Color = Color_RGBA24;

#include <ft2build.h>
#include FT_FREETYPE_H

#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

// only render visible ascii
const usize FR_FIRST_CHAR = (usize)' ';
const usize FR_LAST_CHAR  = (usize)'~';

struct FontGlyphInfo {
  Vec2 advance;
  Vec2 bearing;
  Vec2 tex_tl;
  Vec2 tex_br;
};

struct FontAtlas {
  FontAtlas* next;
  u16 height;
  SDL_Texture* texture;
  FontGlyphInfo glyphs[FR_LAST_CHAR - FR_FIRST_CHAR + 1];
};

struct FontRenderer {
  FT_Face face;
  FontAtlas* atlas;
};

enum {
  GAME_STATE_SELECT = 0,
  GAME_STATE_PLAYING,
  GAME_STATE_SCORE,
};

static struct {
  SDL_Window* wnd;
  SDL_Renderer* r;
  Vec2 vp_dim;
  Vec2 vp_center;
  FT_Library ft;
  FontRenderer f_mono;
  char** text_list;
  usize text_list_len;
  usize text_list_selected;
  u8 game_state;
  u16 font_height;
  char** lines;
  usize num_lines;
  usize cur_line;
  char inp[1024];
  usize inp_len = 0;
  bool stats_started = false;
  f32 stat_time_t0 = 0.0f;
  f32 stat_time = 0;
  u32 stat_chars = 0;
  u32 stat_words = 0;
} g = { };

//
// Font renderer
//

void FR_Init(FontRenderer* fr, const char* ttf) {
  int err = FT_New_Face(g.ft, ttf, 0, &fr->face);
  if (err != 0) {
    SDL_Log("Failed to create FreeType face");
  }
}

FontAtlas* FT_GetAtlas(FontRenderer* fr, u16 height) {
  for (FontAtlas* i = fr->atlas; i; i = i->next) {
    if (i->height == height) {
      return i;
    }
  }
  FontAtlas* atlas = MemAllocZ<FontAtlas>();
  atlas->height = height;

  int err = FT_Set_Pixel_Sizes(fr->face, 0, height);
  if (err != 0) {
    SDL_Log("Failed to set FreeType face size");
  }

  const usize atlas_w = 1024;
  const usize atlas_h = 1024;
  const usize atlas_area = atlas_w * atlas_h;
  u32* atlas_pixels = MemAllocZ<u32>(atlas_area); // RGBA
  usize atlas_pitch = atlas_w * 4;
  usize cur_x = 1;
  usize cur_y = 1;

  // white texture, blit glyphs to alpha channel
  for (usize i = 0; i < atlas_area; ++i) {
    atlas_pixels[i] = 0x00FFFFFF;
  }

  for (usize i = FR_FIRST_CHAR; i <= FR_LAST_CHAR; ++i) {
    const char c = (const char)i;
    err = FT_Load_Char(fr->face, c, FT_LOAD_RENDER);
    if (err != 0) {
      SDL_Log("Failed to render FreeType character %c", (char)i);
    }

    if (cur_x + fr->face->glyph->bitmap.width + 1 >= atlas_w) {
      cur_x = 1;
      cur_y += height + 1;
      if (cur_y + height + 1 >= atlas_h) {
        SDL_Log("Out of texture atlas space");
        exit(1);
      }
    }

    FontGlyphInfo* gi = &atlas->glyphs[i - FR_FIRST_CHAR];
    gi->advance.x = fr->face->glyph->advance.x >> 6;
    gi->advance.y = 0;
    gi->bearing.x = fr->face->glyph->bitmap_left;
    gi->bearing.y = fr->face->glyph->bitmap_top;
    gi->tex_tl = Vec2(cur_x, cur_y);
    gi->tex_br = gi->tex_tl + Vec2(fr->face->glyph->bitmap.width, fr->face->glyph->bitmap.rows);

    for (usize y = 0; y < fr->face->glyph->bitmap.rows; ++y) {
      for (usize x = 0; x < fr->face->glyph->bitmap.width; ++x) {
        // blit to alpha channel only
        usize src_pitch = fr->face->glyph->bitmap.pitch;
        u32 pixel = 0x00FFFFFF | ((u32)(fr->face->glyph->bitmap.buffer[y * src_pitch + x]) << 24);
        atlas_pixels[(cur_y + y) * atlas_w + (cur_x + x)] = pixel;
      }
    }
    cur_x += fr->face->glyph->bitmap.width + 1;
  }

  atlas->texture = SDL_CreateTexture(g.r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, atlas_w, atlas_h);
  if (!atlas->texture) {
    SDL_Log("Failed to create texture: %s", SDL_GetError());
  }
  SDL_UpdateTexture(atlas->texture, 0, atlas_pixels, atlas_pitch);
  atlas->next = fr->atlas;
  fr->atlas = atlas;
  return atlas;
}

void FT_Draw(FontRenderer* fr, u16 height, Color color, Vec2 pos, const char* text) {
  FontAtlas* atlas = FT_GetAtlas(fr, height);

#if 0
  SDL_FRect dst_rect = {
    .x = 0, .y = 0,
    .w = 512, .h = 512,
  };
  SDL_RenderTexture(g.r, atlas->texture, 0, &dst_rect);
#endif

  SDL_SetTextureColorMod(atlas->texture, color.r, color.g, color.b);

  for (; *text; ++text) {
    const FontGlyphInfo* gi = &atlas->glyphs[(usize)*text - FR_FIRST_CHAR];
    SDL_FRect src_rect = {
      .x = gi->tex_tl.x,
      .y = gi->tex_tl.y,
      .w = gi->tex_br.x - gi->tex_tl.x,
      .h = gi->tex_br.y - gi->tex_tl.y,
    };
    SDL_FRect dst_rect = {
      .x = pos.x + gi->bearing.x,
      .y = pos.y - gi->bearing.y,
      .w = src_rect.w,
      .h = src_rect.h,
    };
    SDL_RenderTexture(g.r, atlas->texture, &src_rect, &dst_rect);
    pos += gi->advance;
  }
}

struct TextMetrics {
  Vec2 tl_rel;
  Vec2 dim;
};

TextMetrics FT_MeasureText(FontRenderer* fr, u16 height, const char* text) {
  TextMetrics metrics = { };
  FontAtlas* atlas = FT_GetAtlas(fr, height);
  for (; *text; ++text) {
    const FontGlyphInfo* gi = &atlas->glyphs[(usize)*text - FR_FIRST_CHAR];
    metrics.dim.x += gi->advance.x;
    metrics.dim.y = Max(metrics.dim.y, gi->tex_br.y - gi->tex_tl.y);
  }
  metrics.tl_rel.x = 0;
  metrics.tl_rel.y = -metrics.dim.y;
  return metrics;
}

//
// App
//

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
  RESET_CWD_TO_SOURCE_DIR();
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
    SDL_Log("Failed to initialize SDL: %s", SDL_GetError());
  }
  if (!SDL_CreateWindowAndRenderer(__FILE__, 800, 600, SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED, &g.wnd, &g.r)) {
    SDL_Log("Failed to create window: %s", SDL_GetError());
  }
  SDL_SetRenderVSync(g.r, 1);

  int ret = FT_Init_FreeType(&g.ft);
  if (ret != 0) {
    SDL_Log("Failed to intialize FreeType");
  }
  FR_Init(&g.f_mono, "../../common/data/ttf/JetBrainsMono-Regular.ttf");

  g.text_list = SDL_GlobDirectory("./text", "*.txt", 0, (int*)&g.text_list_len);
  g.font_height = 32;

  return SDL_APP_CONTINUE;
}

void TestRender(const char* text) {
  Vec2 pos = Vec2(15.0f, 15.0f + 32.0f);
  FT_Draw(&g.f_mono, 32, Color::White(), pos, text);
  TextMetrics m = FT_MeasureText(&g.f_mono, 32, text);

  Uint8 old_r, old_g, old_b, old_a;
  SDL_GetRenderDrawColor(g.r, &old_r, &old_g, &old_b, &old_a);
  SDL_SetRenderDrawColor(g.r, 0x00, 0xFF, 0x00, 0xFF);
  const f32 r = 10.0f;
  SDL_RenderLine(g.r, pos.x - r, pos.y, pos.x + r, pos.y);
  SDL_RenderLine(g.r, pos.x, pos.y - r, pos.x, pos.y + r);
  SDL_SetRenderDrawColor(g.r, 0x00, 0xFF, 0xFF, 0xFF);
  SDL_FRect rect = {
    .x = pos.x + m.tl_rel.x,
    .y = pos.y + m.tl_rel.y,
    .w = m.dim.x,
    .h = m.dim.y,
  };
  SDL_RenderRect(g.r, &rect);
  SDL_SetRenderDrawColor(g.r, old_r, old_g, old_b, old_a);
}

enum {
  RTEXT_FLAG_NONE = 0,
  RTEXT_FLAG_CENTER_OF_MASS = 1 << 0,
  RTEXT_FLAG_LEFT_ALIGNED   = 1 << 1,
  RTEXT_FLAG_RIGHT_ALIGNED  = 1 << 2,
};

void RenderText(Vec2 pos, u16 height, Color color, u32 flags, const char* fmt, ...) {
  char buf[1024];
  va_list va; va_start(va, fmt);
  SDL_vsnprintf(buf, sizeof(buf), fmt, va);
  va_end(va);

  const TextMetrics metrics = FT_MeasureText(&g.f_mono, height, buf);

  if (flags & RTEXT_FLAG_CENTER_OF_MASS) {  
    pos -= metrics.dim.Scale(0.5f);
  }
  if (flags & RTEXT_FLAG_LEFT_ALIGNED) {
    pos.x += metrics.dim.x * 0.5f;
  }
  if (flags & RTEXT_FLAG_RIGHT_ALIGNED) {
    pos.x -= metrics.dim.x * 0.5f;
  }

  // @@ looks better
#if 0
  pos.x = SDL_floorf(pos.x);
  pos.y = SDL_floorf(pos.y);
#else
  // round to nearest integer
  pos.x = (int)(pos.x + 0.5f);
  pos.y = (int)(pos.y + 0.5f);
#endif

  FT_Draw(&g.f_mono, height, color, pos, buf);
}

TextMetrics MeasureText(u16 height, const char* fmt, ...) {
  char buf[1024];
  va_list va; va_start(va, fmt);
  SDL_vsnprintf(buf, sizeof(buf), fmt, va);
  va_end(va);
  return FT_MeasureText(&g.f_mono, height, buf);
}

enum : u8 {
  KEYBIND_NONE = 0,
  KEYBIND_UP,
  KEYBIND_DOWN,
  KEYBIND_NEXT,
};

u8 GetKeyBind(SDL_Keycode key) {
  switch (key) {
  case SDLK_W:
  case SDLK_UP:     return KEYBIND_UP;
  case SDLK_S:
  case SDLK_DOWN:   return KEYBIND_DOWN;
  case SDLK_D:
  case SDLK_RETURN: return KEYBIND_NEXT;
  default:          return KEYBIND_NONE;
  }
}

void SetState(u8 state) {
  switch (state) {
  case GAME_STATE_PLAYING: {
    SDL_StartTextInput(g.wnd);

    g.stats_started = 0;
    g.stat_time = 0;
    g.stat_chars = 0;
    g.stat_words = 0;

    g.inp_len = 0;
    g.inp[0] = '\0';

    char text_path[128] = { };
    SDL_snprintf(text_path, sizeof(text_path), "./text/%s", g.text_list[g.text_list_selected]);
    char* text = (char*)SDL_LoadFile(text_path, 0);
    assert(text);
  
    g.num_lines = 0;

    while (true) {
      char* line = text;
      usize line_len = 0;
      for (; line[line_len]; ++line_len) {
        if (line[line_len] == '\n') {
          break;
        }
      }
      if (line_len == 0) {
        break;
      }
      text += line_len;
      if (line[line_len] != '\0') {
        text += 1;
      }
      line[line_len] = '\0';
      g.lines = (char**)realloc(g.lines, sizeof(char*) * (g.num_lines + 1));
      g.lines[g.num_lines] = line;
      ++g.num_lines;
    }
    
  } break;
  }
  g.game_state = state;
}

void OnKeyBind(u8 key) {
  switch (g.game_state) {
  case GAME_STATE_SELECT: {
    switch (key) {
    case KEYBIND_UP: {
      if (g.text_list_selected > 0) {
        --g.text_list_selected;
      }
    } break;
    case KEYBIND_DOWN: {
      if (g.text_list_selected + 1 < g.text_list_len) {
        ++g.text_list_selected;
      }
    } break;
    case KEYBIND_NEXT: {
      SetState(GAME_STATE_PLAYING);
    } break;
  }
  } break;
  case GAME_STATE_SCORE: {
    if (key == KEYBIND_NEXT) {
      SetState(GAME_STATE_SELECT);
    }
  } break;
}
}

void RenderGame() {
  switch (g.game_state) {
  case GAME_STATE_SELECT: {
    const f32 gap = g.font_height * 1.25f;
    const f32 y = g.vp_center.y - gap * g.text_list_selected;
    RenderText(Vec2(g.vp_center.x, g.vp_center.y), g.font_height, Color::White(), RTEXT_FLAG_CENTER_OF_MASS | RTEXT_FLAG_RIGHT_ALIGNED, "Open file:");
    for (usize i = 0; i < g.text_list_len; ++i) {
      Color color = (i == g.text_list_selected) ? Color::White() : Color::Gray();
      RenderText(Vec2(g.vp_center.x, y + gap * i), g.font_height, color, RTEXT_FLAG_CENTER_OF_MASS | RTEXT_FLAG_LEFT_ALIGNED, g.text_list[i]);
    }
  } break;
  case GAME_STATE_PLAYING: {
    if (g.inp_len >= SDL_strlen(g.lines[g.cur_line])) {
      g.stat_chars += g.inp_len;
      g.inp_len = 0;
      g.inp[0] = '\0';
      if (++g.cur_line >= g.num_lines) {
        g.cur_line = 0;
        SetState(GAME_STATE_SCORE);
        return;
      }
    }

    TextMetrics m1 = MeasureText(g.font_height, "%s", g.lines[g.cur_line]);
    TextMetrics m2 = MeasureText(g.font_height, "%s", g.inp);
    TextMetrics m3 = MeasureText(g.font_height, " ");
    Vec2 pos = g.vp_center + Vec2(-m1.dim.x / 2.0f, -m1.dim.y / 2.0f);
    RenderText(pos + Vec2(m2.dim.x, 0.0f), g.font_height, Color::Gray(), 0, "%s", g.lines[g.cur_line] + g.inp_len);
    for (usize i = 0; i < g.inp_len; ++i) {
      TextMetrics m4 = MeasureText(g.font_height, "%c", g.inp[i]);
      char c = g.inp[i];
      if (c == ' ') {
        c = g.lines[g.cur_line][i];
      }
      RenderText(pos, g.font_height, (g.inp[i] == g.lines[g.cur_line][i]) ? Color::White() : Color::Red(), 0, "%c", c);
      pos.x += m4.dim.x;
    }
    SDL_FRect cursor = {
      pos.x,
      pos.y + 5.0f,
      m3.dim.x,
      2,
    };
    SDL_SetRenderDrawColor(g.r, 0x7F, 0x7F, 0x7F, 0xFF);
    SDL_RenderFillRect(g.r, &cursor);

    const usize peek = 5;
    for (usize i = 1; i < Min(peek, g.num_lines - g.cur_line); ++i) {
      f32 gray = (f32)0x7F * (1.0f - (f32)i / (f32)peek);
      RenderText(g.vp_center + Vec2(0.0f, (f32)i * g.font_height * 1.5f), g.font_height, Color((u8)gray), RTEXT_FLAG_CENTER_OF_MASS, "%s", g.lines[g.cur_line + i]);
    }

    const f32 now = (f32)SDL_GetTicks() / 1e3f;

    if(!g.stats_started && g.inp_len > 0) {
      g.stats_started = true;
      g.stat_time_t0 = now;
    }

    f32 calc_time = g.stats_started ? (now - g.stat_time_t0) : 0.0f;
    u32 calc_chars = g.stat_chars + g.inp_len;
    u32 calc_cpm  = (f32)calc_chars / calc_time * 60.0f;

    RenderText(Vec2(7.0f, 5.0f + g.font_height), g.font_height, Color::Gray(), 0,
      "T=%.2fs N=%d CPM=%d", calc_time, calc_chars, calc_cpm);
  } break;
  case GAME_STATE_SCORE: {
    RenderText(g.vp_center, g.font_height, Color::White(), RTEXT_FLAG_CENTER_OF_MASS, "File completed. Press Enter to continue.");
  } break;
}
}

SDL_AppResult SDL_AppIterate(void* appstate) {
  int wnd_w = 0;
  int wnd_h = 0;
  SDL_GetWindowSizeInPixels(g.wnd, &wnd_w, &wnd_h);

  g.vp_dim = Vec2(wnd_w, wnd_h);
  g.vp_center = g.vp_dim.Scale(0.5f);

  SDL_SetRenderDrawColor(g.r, 0x00, 0x00, 0x00, 0xFF);
  SDL_RenderClear(g.r);
  RenderGame();
  SDL_RenderPresent(g.r);
  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
  switch (event->type) {
  case SDL_EVENT_KEY_DOWN: {
    u8 bind = GetKeyBind(event->key.key);
    if (bind > KEYBIND_NONE) {
      OnKeyBind(bind);
    }

    switch (event->key.key) {
    case SDLK_EQUALS: {
      if (!SDL_TextInputActive(g.wnd)) {
        g.font_height = Min(128, g.font_height << 1);
      }
    } break;
    case SDLK_MINUS: {
      if (!SDL_TextInputActive(g.wnd)) {
        g.font_height = Max(8, g.font_height >> 1);
      }
    } break;
    case SDLK_BACKSPACE: {
      if (g.game_state == GAME_STATE_PLAYING) {
        if (g.inp_len > 0) {
          g.inp[--g.inp_len] = '\0';
          SDL_Log("inp: %s", g.inp);
        }
      }
    } break;
    case SDLK_ESCAPE: {
      if (g.game_state == GAME_STATE_PLAYING) {
        SetState(GAME_STATE_SELECT);
      }
    } break;
  }
  } break;
  case SDL_EVENT_MOUSE_WHEEL: {
    if (event->wheel.y > 0) {
      OnKeyBind(KEYBIND_UP);
    }
    if (event->wheel.y < 0) {
      OnKeyBind(KEYBIND_DOWN);
    }
  } break;
  case SDL_EVENT_TEXT_INPUT: {
    if (g.game_state == GAME_STATE_PLAYING) {
      const char c = event->text.text[0];
      if (!(c & 0b10000000) && g.inp_len + 1 < sizeof(g.inp)) {
        g.inp[g.inp_len++] = c;
        g.inp[g.inp_len  ] = '\0';
        SDL_Log("inp: %s", g.inp);
      }
    }
  } break;
  case SDL_EVENT_QUIT: {
    return SDL_APP_SUCCESS;
  } break;
  };
  return SDL_APP_CONTINUE;
}

void SDLCALL SDL_AppQuit(void* appstate, SDL_AppResult result) {
  SDL_Quit();
}
