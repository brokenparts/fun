#define SDL_ASSERT_LEVEL 3
#include <SDL3/SDL.h>

#define ArrLen(arr) (sizeof(arr) / sizeof((arr)[0]))

typedef struct Point Point;
struct Point {
  float x;
  float y;
};

typedef Point Vec2;

// Point POINTS_SQUARE[] = {
//   { .x = 100, .y = 100 },
//   { .x = 200, .y = 100 },
//   { .x = 200, .y = 200 },
//   { .x = 100, .y = 200 },
// };

float RadToDeg(float rad) {
  return rad * 180.0f / 3.14159265f;
}

float Vec2_DotProduct(Vec2 vec1, Vec2 vec2) {
  return vec1.x * vec2.x + vec1.y * vec2.y;
}

Vec2 Vec2_Normalize(Vec2 vec) {
  float mag = SDL_sqrtf(vec.x * vec.x + vec.y * vec.y);
  if (mag > 0) {
    vec.x /= mag;
    vec.y /= mag;
  }
  return vec;
}

float CalcAngle(Point p1, Point p2, Point p3) {
  Vec2 v1 = Vec2_Normalize((Vec2){ p2.x - p1.x, p2.y - p1.y });
  Vec2 v2 = Vec2_Normalize((Vec2){ p2.x - p3.x, p2.y - p3.y });
  return RadToDeg(SDL_acos(Vec2_DotProduct(v1, v2)));
}

float CalcInteriorAngle(Point p1, Point p2, Point p3) {
  float a1 = CalcAngle(p1, p2, p3);
  float a2 = CalcAngle(p3, p2, p1);
  if (a1 < a2) {
    return a1;
  } else {
    return a2;
  }
}

float CalcExteriorAngle(Point p1, Point p2, Point p3) {
  float a1 = CalcAngle(p1, p2, p3);
  float a2 = CalcAngle(p3, p2, p1);
  if (a1 > a2) {
    return a1;
  } else {
    return a2;
  }
}

// 2D vector cross product as an edge function
bool IntersectLines_Orient(Point p1, Point p2, Point test) {
  return (test.y - p1.y) * (p2.x - p1.x) > (p2.y - p1.y) * (test.x - p1.x);
}

bool IntersectLines(Point l1p1, Point l1p2, Point l2p1, Point l2p2) {
  // Returns false when lines are collinear, that's ok
  return
    IntersectLines_Orient(l1p1, l2p1, l2p2) != IntersectLines_Orient(l1p2, l2p1, l2p2) &&
    IntersectLines_Orient(l1p1, l1p2, l2p1) != IntersectLines_Orient(l1p1, l1p2, l2p2);
}

bool IsPentagram(Point* points, int num_points) {
  if (num_points == 5) {
    // Each line segment needs to intersect other line segments twice
    for (int i = 0; i < 5; ++i) {
      int intersection_count = 0;
      Point l1p1 = points[i];
      Point l1p2 = points[(i + 1) % 5];
      for (int j = 0; j < 5; ++j) {
        if (j == i) {
          continue;
        }
        Point l2p1 = points[j];
        Point l2p2 = points[(j + 1) % 5];
        if (IntersectLines(l1p1, l1p2, l2p1, l2p2)) {
          ++intersection_count;
        }
      }
      if (intersection_count != 2) {
        return false;
      }
    }
    return true;
  }
  return false;
}

int main(int argc, char *argv[]) {
  SDL_Window* wnd = SDL_CreateWindow("shapetest", 800, 600, 0);
  SDL_assert(wnd);

  SDL_Renderer* r = SDL_CreateRenderer(wnd, 0);
  SDL_assert(r);

  Point points[100] = { 0 };
  int num_points = 0;

  // SDL_memcpy(points, POINTS_SQUARE, sizeof(POINTS_SQUARE));
  // num_points = ArrLen(POINTS_SQUARE);

  bool running = true;
  while (running) {
    SDL_Event evt = { 0 };
    while (SDL_PollEvent(&evt)) {
      if (evt.type == SDL_EVENT_QUIT) {
        running = false;
      }
      else if (evt.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        if (evt.button.button == 1) {
          Point* p = &points[num_points];
          p->x = evt.button.x;
          p->y = evt.button.y;
          if (num_points < (int)ArrLen(points) - 1) {
            ++num_points;
          }
        } else {
          if (num_points > 0) {
            --num_points;
          }
        }
      }
    }

    SDL_Log("IsPentagram: %d", IsPentagram(points, num_points));

    SDL_SetRenderDrawColor(r, 0x00, 0x00, 0x00, 0xFF);
    SDL_RenderClear(r);

    SDL_SetRenderDrawColor(r, 0x00, 0xFF, 0x00, 0xFF);
    for (int i = 0; i < num_points; ++i) {
      Point* p2 = &points[i];
      Point* p1 = 0;
      if (i > 0) {
        p1 = &points[i - 1];
      } else {
        p1 = &points[num_points - 1];
      }
      SDL_RenderLine(r, p1->x, p1->y, p2->x, p2->y);
    }

    SDL_SetRenderDrawColor(r, 0xFF, 0x00, 0x00, 0xFF);
    for (int i = 0; i < num_points; ++i) {
      const float point_size = 5.0f;
      SDL_FRect rect = {
        .x = points[i].x - point_size / 2.0f,
        .y = points[i].y - point_size / 2.0f,
        .w = point_size,
        .h = point_size
      };
      SDL_RenderFillRect(r, &rect);
    }

    SDL_RenderPresent(r);

  }

  SDL_DestroyWindow(wnd);

  return 0;
}
