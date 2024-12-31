#include "common_core.hh"
#include "common_dsa.hh"
#include "common_math.hh"

#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

// https://jacco.ompf2.com/2022/04/13/how-to-build-a-bvh-part-1-basics/

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

  Vec2 Size() {
    return maxs - mins;
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

  Vec2 ent_mins = ent->pos - Vec2(ent->radius, ent->radius);
  Vec2 ent_maxs = ent->pos + Vec2(ent->radius, ent->radius);

  // Collide with walls
  if (ent_mins.x <= 0.0f || ent_maxs.x >= g.viewport.x) {
    ent->vel.x *= -1.0f;
  }
  if (ent_mins.y <= 0.0f || ent_maxs.y >= g.viewport.y) {
    ent->vel.y *= -1.0f;
  }
  ent->pos.x = Clamp(ent->pos.x, ent->radius, g.viewport.x - ent->radius);
  ent->pos.y = Clamp(ent->pos.y, ent->radius, g.viewport.y - ent->radius);
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
  Entity**    ents;
  usize       ents_count;
  BVH_Node*   left;
  BVH_Node*   right;
  bool        debug_hit;
};

struct BVH {
  Entity**  ents;
  usize     ents_count;
  BVH_Node*   root;
};

void BVH_BuildTopDown_Subdivide(BVH_Node* node) {
  // Update AABB for this node
  constexpr f32 huge_number = 9999.0f;
  node->aabb.mins = Vec2(huge_number, huge_number);
  node->aabb.maxs = Vec2(-huge_number, -huge_number);
  // BVH_Entity* ent = node->e_first;
  for (usize i = 0; i < node->ents_count; ++i) {
    Entity* ent = node->ents[i];

    node->aabb.mins.x = Min(node->aabb.mins.x, ent->pos.x - ent->radius);
    node->aabb.mins.y = Min(node->aabb.mins.y, ent->pos.y - ent->radius);
    node->aabb.maxs.x = Max(node->aabb.maxs.x, ent->pos.x + ent->radius);
    node->aabb.maxs.y = Max(node->aabb.maxs.y, ent->pos.y + ent->radius);
  }

  constexpr usize max_children = 2;
  if (node->ents_count <= max_children) {
    return;
  }

  // Select split axis and position
  Vec2 aabb_dims = node->aabb.Size();
  const u8 split_axis = (aabb_dims.x >= aabb_dims.y) ? VEC_AXIS_X : VEC_AXIS_Y;
  const f32 split_pos = node->aabb.Center().GetAxis(split_axis);

  // Split
  i32 i_left = 0;
  i32 i_right = node->ents_count - 1; // 2
  while (i_left <= i_right) {
    if (node->ents[i_left]->pos.GetAxis(split_axis) <= split_pos) {
      // left
      ++i_left;
    } else {
      // right
      Swap(node->ents[i_left], node->ents[i_right]);
      --i_right;
    }
  }

  // Handle edge case where all entities are in the same position
  if ((usize)i_left == node->ents_count) {
    i_left = node->ents_count / 2;
  }

  node->left = MemAllocZ<BVH_Node>();
  node->left->ents = node->ents;
  node->left->ents_count = i_left;
  node->right = MemAllocZ<BVH_Node>();
  node->right->ents = node->ents + i_left;
  node->right->ents_count = node->ents_count - i_left;
  BVH_BuildTopDown_Subdivide(node->left);
  BVH_BuildTopDown_Subdivide(node->right);
}

// Bottom-up
BVH* BVH_BuildTopDown() {
  BVH* bvh = MemAllocZ<BVH>();
  // Copy entity list into array
  for (Entity* ent = g.ents; ent; ent = ent->next) {
    ++bvh->ents_count;
  }
  bvh->ents = MemAllocZ<Entity*>(bvh->ents_count);
  Entity* ent = g.ents;
  for (usize i = 0; i < bvh->ents_count; ++i) {
    bvh->ents[i] = ent;
    ent = ent->next;
  }
  // Create root node with all entities
  bvh->root = MemAllocZ<BVH_Node>();
  bvh->root->ents = bvh->ents;
  bvh->root->ents_count = bvh->ents_count;
  // Recursively subdivide
  if (bvh->root->ents_count > 0) {
    BVH_BuildTopDown_Subdivide(bvh->root);
  }
  return bvh;
};

void BVH_FreeNode(BVH_Node* node) {
  if (node->left) {
    BVH_FreeNode(node->left);
  }
  if (node->right) {
    BVH_FreeNode(node->right);
  }
  MemFree(node);
}

void BVH_Free(BVH* bvh) {
  MemFree(bvh->ents);
  BVH_FreeNode(bvh->root);
}

void BVH_DrawRecursive(BVH_Node* node, u32 depth=0) {
  if (!node || !node->ents_count) {
    return;
  }
  BVH_DrawRecursive(node->left, depth + 1);
  BVH_DrawRecursive(node->right, depth + 1);
  SDL_FRect rect = {
    .x = node->aabb.mins.x,
    .y = node->aabb.mins.y,
    .w = node->aabb.maxs.x - node->aabb.mins.x,
    .h = node->aabb.maxs.y - node->aabb.mins.y,
  };
  if (node->debug_hit) {
    SDL_SetRenderDrawColor(g.r, 0xFF, 0xFF, 0x00, 0xFF);
  } else {
    SDL_SetRenderDrawColor(g.r, 0xFF, 0x00, 0x00, 0xFF);
  }
  SDL_RenderRect(g.r, &rect);
  if (g.debug_flags & DEBUG_BVH_VOLUME) {
    SDL_RenderDebugTextFormat(g.r, node->aabb.mins.x, node->aabb.mins.y, "<%.0f, %.0f>, d=%u",
    node->aabb.maxs.x - node->aabb.mins.x, node->aabb.maxs.y - node->aabb.mins.y, depth);
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

  BVH* bvh = BVH_BuildTopDown();

  BVH_HitTest(bvh->root);

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

  BVH_DrawRecursive(bvh->root);

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
  PushDebugString("  Time:   %.2f", g.time);
  PushDebugString("  Delta:  %.2fms (%u FPS)", g.dtime * 1000.0f, (u32)(1.0f / g.dtime));
  PushDebugString("  Cursor: <%.0f, %.0f>", g.cursor.x, g.cursor.y);
  // PushDebugString("  Root:   <%.0f, %.0f> <%.0f, %.0f>",
  //   bvh->root->aabb.mins.x, bvh->root->aabb.mins.y,
  //   bvh->root->aabb.maxs.x, bvh->root->aabb.maxs.y);
  PushDebugString("[Controls]");
  PushDebugString("  Space: Spawn entity");
  PushDebugString("  1:     Toggle entity state debug");
  PushDebugString("  2:     Toggle BVH volume debug");
  PushDebugString("  3:     Pause entity simulation");
  PushDebugString("  4:     Remove all entities");

  BVH_Free(bvh);
  
  SDL_RenderPresent(g.r);
  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
  switch (event->type) {
  case SDL_EVENT_KEY_DOWN: {
    switch (event->key.key) {
    case SDLK_SPACE: {
      for (int i = 0; i < 1; ++i) {
        E_Spawn();
      }  
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
    case SDLK_4: {
      Entity* next = 0;
      for (Entity* ent = g.ents; ent; ent = next) {
        next = ent->next;
        MemFree(ent);
      }
      g.ents = 0;
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
