#pragma once
#include "rendering/vertex.h"
#include "voxels/chunk.h"
#include "voxels/world.h"
#include <cstdint>
#include <vector>

class VoxelRegistry;

struct MeshData {
  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;
  bool empty() const { return indices.empty(); }
  void clear() {
    vertices.clear();
    indices.clear();
  }
  void reserve(size_t vertexCount, size_t indexCount) {
    vertices.reserve(vertexCount);
    indices.reserve(indexCount);
  }
};

// Face order must match VoxelDef::tiles layout: +X,-X,+Y,-Y,+Z,-Z.
struct MesherFace {
  int dir[3];
  int corners[4][3]; // matches eCounterClockwise cull
  float shade;
};

MeshData meshChunk(const Chunk &chunk, const World &world,
                   const VoxelRegistry &registry);
bool shouldShowFace(const VoxelDef &currentDef, const VoxelDef &neighborDef);
