#pragma once
#include "voxels/world.h"
#include <cmath>
#include <glm/glm.hpp>
#include <limits>

struct RayHit {
  bool hit = false;
  int x, y, z;                   // hit voxel
  int prevX, prevY, prevZ;       // previous empty (for place)
  int normalX, normalY, normalZ; // face normal
  float dist = 0.0f;
};

inline RayHit raycastVoxel(const World &world, glm::vec3 origin, glm::vec3 dir,
                           float maxDist) {
  RayHit hit;
  int x = (int)std::floor(origin.x);
  int y = (int)std::floor(origin.y);
  int z = (int)std::floor(origin.z);

  int stepX = (dir.x > 0) ? 1 : -1;
  int stepY = (dir.y > 0) ? 1 : -1;
  int stepZ = (dir.z > 0) ? 1 : -1;

  const float INF = std::numeric_limits<float>::infinity();
  float tDeltaX = (dir.x != 0) ? std::abs(1.0f / dir.x) : INF;
  float tDeltaY = (dir.y != 0) ? std::abs(1.0f / dir.y) : INF;
  float tDeltaZ = (dir.z != 0) ? std::abs(1.0f / dir.z) : INF;

  // distance to boundary
  float fx = origin.x - x, fy = origin.y - y, fz = origin.z - z;
  float tMaxX = (dir.x != 0) ? tDeltaX * (stepX > 0 ? 1.0f - fx : fx) : INF;
  float tMaxY = (dir.y != 0) ? tDeltaY * (stepY > 0 ? 1.0f - fy : fy) : INF;
  float tMaxZ = (dir.z != 0) ? tDeltaZ * (stepZ > 0 ? 1.0f - fz : fz) : INF;

  float t = 0.0f;
  int prevX = x, prevY = y, prevZ = z;

  for (int i = 0; i < 256; ++i) {
    if (tMaxX < tMaxY && tMaxX < tMaxZ) {
      prevX = x;
      prevY = y;
      prevZ = z;
      x += stepX;
      t = tMaxX;
      tMaxX += tDeltaX;
      hit.normalX = -stepX;
      hit.normalY = 0;
      hit.normalZ = 0;
    } else if (tMaxY < tMaxZ) {
      prevX = x;
      prevY = y;
      prevZ = z;
      y += stepY;
      t = tMaxY;
      tMaxY += tDeltaY;
      hit.normalX = 0;
      hit.normalY = -stepY;
      hit.normalZ = 0;
    } else {
      prevX = x;
      prevY = y;
      prevZ = z;
      z += stepZ;
      t = tMaxZ;
      tMaxZ += tDeltaZ;
      hit.normalX = 0;
      hit.normalY = 0;
      hit.normalZ = -stepZ;
    }

    // over reached
    if (t > maxDist)
      return {};

    // block isnt air
    // we hit something!
    if (world.getBlock(x, y, z) != AIR) { // or: registry.get(id).solid
      hit.hit = true;
      hit.x = x;
      hit.y = y;
      hit.z = z;
      hit.prevX = prevX;
      hit.prevY = prevY;
      hit.prevZ = prevZ;
      hit.dist = t;
      return hit;
    }
  }
  return {};
}
