#include <algorithm>

static float smoothstep(float e0, float e1, float x) {
  float t = std::clamp((x - e0) / (e1 - e0), 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}
