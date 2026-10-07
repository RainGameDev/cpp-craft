#include "voxels/chunk_mesher.h"
#include "voxels/voxel_registry.h"
#include "voxels/world.h"

static constexpr MesherFace FACES[6] = {
    // +X
    {.dir = {1, 0, 0},
     .corners = {{1, 0, 1}, {1, 0, 0}, {1, 1, 0}, {1, 1, 1}},
     .shade = 0.8f},
    // minus X
    {.dir = {-1, 0, 0},
     .corners = {{0, 0, 0}, {0, 0, 1}, {0, 1, 1}, {0, 1, 0}},
     .shade = 0.8f},
    // +Y (top)
    {.dir = {0, 1, 0},
     .corners = {{0, 1, 1}, {1, 1, 1}, {1, 1, 0}, {0, 1, 0}},
     .shade = 1.0f},
    // minus Y, bottom face
    {.dir = {0, -1, 0},
     .corners = {{1, 0, 1}, {0, 0, 1}, {0, 0, 0}, {1, 0, 0}},
     .shade = 0.5f},
    // +Z (front)
    {.dir = {0, 0, 1},
     .corners = {{0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}},
     .shade = 0.7f},
    // minus Z, back face
    {.dir = {0, 0, -1},
     .corners = {{1, 0, 0}, {0, 0, 0}, {0, 1, 0}, {1, 1, 0}},
     .shade = 0.7f},
};

// corner UV for a fulltexture face.
static constexpr float FACE_UV[4][2] = {{0, 1}, {1, 1}, {1, 0}, {0, 0}};

bool shouldShowFace(const VoxelDef &currentDef, const VoxelDef &neighborDef) {
  if (neighborDef.render == VoxelDef::None)
    return true;
  // Same transparent type culls the shared face.
  if (!neighborDef.opaque)
    return neighborDef.id != currentDef.id;
  return false;
}

VoxelId snapshotBlockAt(const ChunkMeshSnapshot &snapshot, int worldX,
                        int worldY, int worldZ) {
  ChunkPos wanted = World::toChunkPos(worldX, worldY, worldZ);
  const Chunk *target = nullptr;
  if (wanted == snapshot.central.pos) {
    target = &snapshot.central;
  } else {
    // Neighbor order matches face order.
    for (size_t i = 0; i < 6; ++i) {
      if (snapshot.neighbors[i] && snapshot.neighbors[i]->pos == wanted) {
        target = &(*snapshot.neighbors[i]);
        break;
      }
    }
    if (!target)
      return AIR; // missing neighbor chunk treat as empty
  }
  return target->getLocal(floorMod(worldX, CW), floorMod(worldY, CH),
                          floorMod(worldZ, CD));
}

MeshData meshChunkWithGetter(
    const Chunk &chunk, const std::function<VoxelId(int, int, int)> &getBlock,
    const VoxelRegistry &registry, const AtlasLayout &atlasLayout) {
  MeshData mesh;
  mesh.reserve(1024, 1536);

  const int baseX = chunk.pos.x * CW;
  const int baseY = chunk.pos.y * CH;
  const int baseZ = chunk.pos.z * CD;

  for (int localY = 0; localY < CH; ++localY) {
    for (int localZ = 0; localZ < CD; ++localZ) {
      for (int localX = 0; localX < CW; ++localX) {
        VoxelId voxelId = chunk.getLocal(localX, localY, localZ);
        if (voxelId == AIR)
          continue;
        const VoxelDef &voxelDef = registry.get(voxelId);
        if (voxelDef.render == VoxelDef::None)
          continue;

        const int worldX = baseX + localX, worldY = baseY + localY,
                  worldZ = baseZ + localZ;

        for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
          const MesherFace &face = FACES[faceIndex];
          VoxelId neighborId = getBlock(
              worldX + face.dir[0], worldY + face.dir[1], worldZ + face.dir[2]);
          const VoxelDef &neighborDef = registry.get(neighborId);
          if (!shouldShowFace(voxelDef, neighborDef))
            continue;

          const uint32_t baseVertex =
              static_cast<uint32_t>(mesh.vertices.size());
          const auto &atlasCell = voxelDef.textureCoords[faceIndex];
          const float inset = 0.0;
          const float span = 1.0f - 2.0f * inset;
          for (int cornerIndex = 0; cornerIndex < 4; ++cornerIndex) {
            Vertex vertex;
            vertex.pos = glm::vec3(worldX + face.corners[cornerIndex][0],
                                   worldY + face.corners[cornerIndex][1],
                                   worldZ + face.corners[cornerIndex][2]);
            vertex.color = glm::vec3(face.shade);
            vertex.uv = glm::vec2(
                (atlasCell[0] + inset + FACE_UV[cornerIndex][0] * span) /
                    atlasLayout.columns,
                (atlasCell[1] + inset + FACE_UV[cornerIndex][1] * span) /
                    atlasLayout.rows);
            mesh.vertices.push_back(vertex);
          }
          mesh.indices.insert(mesh.indices.end(),
                              {baseVertex, baseVertex + 1, baseVertex + 2,
                               baseVertex, baseVertex + 2, baseVertex + 3});
        }
      }
    }
  }
  return mesh;
}

MeshData meshChunkSnapshot(const ChunkMeshSnapshot &snapshot,
                           const VoxelRegistry &registry,
                           const AtlasLayout &atlasLayout) {
  auto getter = [&snapshot](int x, int y, int z) {
    return snapshotBlockAt(snapshot, x, y, z);
  };
  std::function<VoxelId(int, int, int)> fn = getter;
  return meshChunkWithGetter(snapshot.central, fn, registry, atlasLayout);
}

MeshData meshChunk(const Chunk &chunk, const World &world,
                   const VoxelRegistry &registry,
                   const AtlasLayout &atlasLayout) {
  auto getter = [&world](int x, int y, int z) {
    return world.getBlock(x, y, z);
  };
  std::function<VoxelId(int, int, int)> fn = getter;
  return meshChunkWithGetter(chunk, fn, registry, atlasLayout);
}
