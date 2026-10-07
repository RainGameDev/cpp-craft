#pragma once
#include "rendering/vertex.h"
#include "voxels/chunk.h"
#include "voxels/texture_atlas.h"
#include "voxels/world.h"
#include <array>
#include <cstdint>
#include <functional>
#include <optional>
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

// Face order must match VoxelDef tiles layout, one entry per cube face.
struct MesherFace {
  int dir[3];
  int corners[4][3]; // matches eCounterClockwise cull
  float shade;
};

// copy of a chunk + its 6 neighbors for threads.
struct ChunkMeshSnapshot {
  Chunk central;
  std::array<std::optional<Chunk>, 6> neighbors{};
};

MeshData meshChunk(const Chunk &chunk, const World &world,
                   const VoxelRegistry &registry,
                   const AtlasLayout &atlasLayout);
// Core mesher takes any block lookup, so workers can pass a snapshot.
MeshData meshChunkWithGetter(
    const Chunk &chunk, const std::function<VoxelId(int, int, int)> &getBlock,
    const VoxelRegistry &registry, const AtlasLayout &atlasLayout);
// Snapshot overload used by the mesh thread pool.
MeshData meshChunkSnapshot(const ChunkMeshSnapshot &snapshot,
                           const VoxelRegistry &registry,
                           const AtlasLayout &atlasLayout);
VoxelId snapshotBlockAt(const ChunkMeshSnapshot &snapshot, int worldX,
                        int worldY, int worldZ);
bool shouldShowFace(const VoxelDef &currentDef, const VoxelDef &neighborDef);
