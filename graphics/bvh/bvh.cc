#include "common_core.hh"
#include "common_dsa.hh"
#include "common_math.hh"

#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

struct Entity {
  Vec2 pos;
  Vec2 vel;
  f32  next_steer;
  f32  radius;

  Entity* next;
};

struct AABB {
  Vec2 mins;
  Vec2 maxs;

  Vec2 Center() {
    return (mins + maxs) / 2.0f;
  }

  bool Test(Vec2 point) {
    return InRange(point.x, mins.x, maxs.x) && InRange(point.y, mins.y, maxs.y);
  }
};

AABB AABB_Combine(AABB box1, AABB box2) {
  return {
    .mins = Vec2(
      Min(box1.mins.x, box2.mins.x),
      Min(box1.mins.y, box2.mins.y)
    ),
    .maxs = Vec2(
      Max(box1.maxs.x, box2.maxs.x),
      Max(box1.maxs.y, box2.maxs.y)
    ),
  };
}

enum {
  DEBUG_ENTITY_STATE = 1 << 0,
  DEBUG_BVH_VOLUME = 1 << 1,
  DEBUG_FREEZE = 1 << 2,
};

static struct {
  SDL_Window* wnd;
  SDL_Renderer* r;
  Vec2 viewport;
  Vec2 cursor;
  u8 debug_flags;
  f32 time;
  f32 dtime;
  Xorshift rng;
  Entity* ents;
} g = { };

void E_Steer(Entity* ent) {
  ent->vel.x = g.rng.RandomFloat(-1.0f, 1.0f);
  ent->vel.y = g.rng.RandomFloat(-1.0f, 1.0f);
  ent->vel = ent->vel.Normalize() * 100.0f;
  ent->next_steer = g.time + g.rng.RandomFloat(3.0f);
}

void E_Think(Entity* ent) {
  if (ent->next_steer <= g.time) {
    E_Steer(ent);
  }
  ent->pos += ent->vel * g.dtime;
  if (ent->pos.x - ent->radius < 0.0f) {
    ent->pos.x = ent->radius;
    ent->vel.x *= -1.0f;
  }
  if (ent->pos.x + ent->radius > g.viewport.x) {
    ent->pos.x = g.viewport.x - ent->radius;
    ent->vel.x *= -1.0f;
  }
  if (ent->pos.y - ent->radius < 0.0f) {
    ent->pos.y = ent->radius;
    ent->vel.y *= -1.0f;
  }
  if (ent->pos.y + ent->radius > g.viewport.y) {
    ent->pos.y = g.viewport.y - ent->radius;
    ent->vel.y *= -1.0f;
  }
}

void E_Spawn() {
  Entity* ent = MemAllocZ<Entity>();
  ent->pos = g.viewport / 2.0f;
  ent->radius = g.rng.RandomFloat(3.0f, 10.0f);
  E_Steer(ent);
  ent->next = g.ents;
  g.ents = ent;
}

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
    SDL_Log("Failed to initialize SDL: %s", SDL_GetError());
  }
  if (!(g.wnd = SDL_CreateWindow(__FILE__, 800, 600, 0))) {
    SDL_Log("Failed to create SDL window: %s", SDL_GetError());
  }
  if (!(g.r = SDL_CreateRenderer(g.wnd, 0))) {
    SDL_Log("Failed to create SDL renderer: %s", SDL_GetError());
  }
  SDL_SetRenderVSync(g.r, 1);
  return SDL_APP_CONTINUE;
}

struct BVH_Node {
  AABB aabb;
  Entity* ent;
  bool debug_hit;
  BVH_Node* left;
  BVH_Node* right;
  BVH_Node* prev;
  BVH_Node* next;
};

BVH_Node* BVH_Prepend(BVH_Node* root, BVH_Node* node) {
  node->next = root;
  if (root) {
    root->prev = node;
  }
  return node;
}

void BVH_Unlink(BVH_Node* node) {
  if (!node) {
    return;
  }
  if (node->prev) {
    node->prev->next = node->next;
  }
  if (node->next) {
    node->next->prev = node->prev;
  }
  node->prev = 0;
  node->next = 0;
}

BVH_Node* BVH_BuildBottomUp_Refine(BVH_Node* level) {
  if (!level) {
    return level;
  }
  BVH_Node* root = 0;
  while (level) {
    BVH_Node* left = 0;
    BVH_Node* right = 0;
    BVH_Node* parent = 0;

    if (level->next) {
      // At least two nodes exist - group the closest two together
      left = level;
      f32 closest_dist = 0.0f;
      for (BVH_Node* other = level->next; other; other = other->next) {
        f32 dist = (other->aabb.Center() - left->aabb.Center()).Length();
        if (!right || dist < closest_dist) {
          right = other;
          closest_dist = dist;
        }
      }
      assert(left && right);

      parent = MemAllocZ<BVH_Node>();
      parent->aabb = AABB_Combine(left->aabb, right->aabb);
    } else {
      // No children - child becomes parent
      parent = level;
      level = level->next;
      BVH_Unlink(parent);
    }

    // Remove children from working set
    while (level && (level == left || level == right)) {
      level = level->next;
    }
    BVH_Unlink(left);
    BVH_Unlink(right);

    // Prepend parent node
    parent->left = left;
    parent->right = right;
    root = BVH_Prepend(root, parent);
  }

  if (root->next) {
    return BVH_BuildBottomUp_Refine(root);
  }

  return root;
}

// Bottom-up
BVH_Node* BVH_BuildBottomUp() {
  BVH_Node* nodes = 0;
  for (Entity* ent = g.ents; ent; ent = ent->next) {
    BVH_Node* node = MemAllocZ<BVH_Node>();
    node->aabb.mins = ent->pos - Vec2(ent->radius, ent->radius);
    node->aabb.maxs = ent->pos + Vec2(ent->radius, ent->radius);
    node->ent = ent;
    nodes = BVH_Prepend(nodes, node);
  }

  return BVH_BuildBottomUp_Refine(nodes);
};

void BVH_Free(BVH_Node* bvh) {
  if (!bvh) {
    return;
  }
  if (bvh->left) {
    BVH_Free(bvh->left);
  }
  if (bvh->right) {
    BVH_Free(bvh->right);
  }
  MemFree(bvh);
}

void BVH_DrawRecursive(BVH_Node* node) {
  if (node && node->left && node->right) {
    BVH_DrawRecursive(node->left);
    BVH_DrawRecursive(node->right);
    SDL_FRect rect = {
      .x = node->aabb.mins.x,
      .y = node->aabb.mins.y,
      .w = node->aabb.maxs.x - node->aabb.mins.x,
      .h = node->aabb.maxs.y - node->aabb.mins.y,
    };
    if (node->debug_hit) {
      SDL_SetRenderDrawColor(g.r, 0xFF, 0xFF, 0x00, 0xFF);
    } else {
      SDL_SetRenderDrawColor(g.r, 0x00, 0x00, 0xFF, 0xFF);
    }
    SDL_RenderRect(g.r, &rect);
    if (g.debug_flags & DEBUG_BVH_VOLUME) {
      SDL_RenderDebugTextFormat(g.r, node->aabb.mins.x, node->aabb.mins.y, "<%.0f, %.0f>",
        node->aabb.maxs.x - node->aabb.mins.x, node->aabb.maxs.y - node->aabb.mins.y);
    }
  }
}

void BVH_HitTest(BVH_Node* node) {
  if (node) {
    node->debug_hit = node->aabb.Test(g.cursor);
    BVH_HitTest(node->left);
    BVH_HitTest(node->right);
  }
}

SDL_AppResult SDL_AppIterate(void* appstate) {
  //
  // Update app state
  //

  static f32 last_t = 0.0f;
  f32 next_t = (f32)SDL_GetTicks() / 1000.0f;
  g.time = next_t;
  g.dtime = g.time - last_t;
  last_t = next_t;

  int wnd_x = 0;
  int wnd_y = 0;
  SDL_GetWindowSizeInPixels(g.wnd, &wnd_x, &wnd_y);
  g.viewport = Vec2(wnd_x, wnd_y);

  SDL_GetMouseState(&g.cursor.x, &g.cursor.y);

  //
  // Simulate entities
  //

  if (!(g.debug_flags & DEBUG_FREEZE)) {
    for (Entity* ent = g.ents; ent; ent = ent->next) {
      E_Think(ent);
    }
  }

  //
  // Compute BVH
  //

  BVH_Node* bvh = BVH_BuildBottomUp();

  BVH_HitTest(bvh);

  //
  // Draw
  //

  SDL_SetRenderDrawColor(g.r, 0x00, 0x00, 0x00, 0xFF);
  SDL_RenderClear(g.r);

  SDL_SetRenderDrawColor(g.r, 0x00, 0xFF, 0x00, 0xFF);
  for (Entity* ent = g.ents; ent; ent = ent->next) {
    SDL_FRect rect = {
      .x = ent->pos.x - ent->radius,
      .y = ent->pos.y - ent->radius,
      .w = ent->radius * 2.0f,
      .h = ent->radius * 2.0f,
    };
    SDL_RenderRect(g.r, &rect);
  }

  SDL_SetRenderDrawColor(g.r, 0xFF, 0x00, 0x00, 0xFF);
  BVH_DrawRecursive(bvh);

  if (g.debug_flags & DEBUG_ENTITY_STATE) {
    for (Entity* ent = g.ents; ent; ent = ent->next) {
      SDL_RenderDebugTextFormat(g.r, ent->pos.x, ent->pos.y,
        ".p=<%.2f, %.2f> .v=<%.2f, %.2f>, .n=%.2f",
        ent->pos.x, ent->pos.y, ent->vel.x, ent->vel.y, ent->next_steer);
    }
  }

  SDL_SetRenderDrawColor(g.r, 0xFF, 0xFF, 0x00, 0xFF);
  SDL_RenderLine(g.r, g.cursor.x - 10, g.cursor.y, g.cursor.x + 10, g.cursor.y);
  SDL_RenderLine(g.r, g.cursor.x, g.cursor.y - 10, g.cursor.x, g.cursor.y + 10);

  SDL_SetRenderDrawColor(g.r, 0xFF, 0xFF, 0xFF, 0xFF);
  int ndebug = 0;
  auto PushDebugString = [&](const char* fmt, ...) {
    char strbuf[1024];
    va_list va; va_start(va, fmt);
    SDL_vsnprintf(strbuf, sizeof(strbuf), fmt, va);
    va_end(va);
    SDL_RenderDebugText(g.r, 5.0f, 5.0f + 10.0f * ndebug, strbuf);
    ++ndebug;
  };

  PushDebugString("[State]");
  PushDebugString("  Time:  %.2f", g.time);
  PushDebugString("  Delta: %.2fms (%u FPS)", g.dtime, (u32)(1000.0f / g.dtime));
  PushDebugString("[Controls]");
  PushDebugString("  Space: Spawn entity");
  PushDebugString("  1:     Toggle entity state debug");
  PushDebugString("  2:     Toggle BVH volume debug");
  PushDebugString("  3:     Pause entity simulation");

  BVH_Free(bvh);
  
  SDL_RenderPresent(g.r);
  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
  switch (event->type) {
  case SDL_EVENT_KEY_DOWN: {
    switch (event->key.key) {
    case SDLK_SPACE: {
      E_Spawn();
    } break;
    case SDLK_1: {
      g.debug_flags ^= DEBUG_ENTITY_STATE;
    } break;;
    case SDLK_2: {
      g.debug_flags ^= DEBUG_BVH_VOLUME;
    } break;
    case SDLK_3: {
      g.debug_flags ^= DEBUG_FREEZE;
    } break;
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
