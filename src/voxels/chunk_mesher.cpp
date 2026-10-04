#include "voxels/chunk_mesher.h"
#include "voxels/voxel_registry.h"

static constexpr MesherFace FACES[6] = {
    // +X
    {.dir = {1, 0, 0},
     .corners = {{1, 0, 1}, {1, 0, 0}, {1, 1, 0}, {1, 1, 1}},
     .shade = 0.8f},
    // -X
    {.dir = {-1, 0, 0},
     .corners = {{0, 0, 0}, {0, 0, 1}, {0, 1, 1}, {0, 1, 0}},
     .shade = 0.8f},
    // +Y (top)
    {.dir = {0, 1, 0},
     .corners = {{0, 1, 1}, {1, 1, 1}, {1, 1, 0}, {0, 1, 0}},
     .shade = 1.0f},
    // -Y (bottom)
    {.dir = {0, -1, 0},
     .corners = {{1, 0, 1}, {0, 0, 1}, {0, 0, 0}, {1, 0, 0}},
     .shade = 0.5f},
    // +Z (front)
    {.dir = {0, 0, 1},
     .corners = {{0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}},
     .shade = 0.7f},
    // -Z (back)
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

MeshData meshChunk(const Chunk &chunk, const World &world,
                   const VoxelRegistry &registry) {
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
          VoxelId neighborId = world.getBlock(
              worldX + face.dir[0], worldY + face.dir[1], worldZ + face.dir[2]);
          const VoxelDef &neighborDef = registry.get(neighborId);
          if (!shouldShowFace(voxelDef, neighborDef))
            continue;

          const uint32_t baseVertex =
              static_cast<uint32_t>(mesh.vertices.size());
          for (int cornerIndex = 0; cornerIndex < 4; ++cornerIndex) {
            Vertex vertex;
            vertex.pos = glm::vec3(worldX + face.corners[cornerIndex][0],
                                   worldY + face.corners[cornerIndex][1],
                                   worldZ + face.corners[cornerIndex][2]);
            vertex.color = glm::vec3(face.shade);
            vertex.uv =
                glm::vec2(FACE_UV[cornerIndex][0], FACE_UV[cornerIndex][1]);
            // TODO: offset uv into voxelDef.tiles[faceIndex] atlas cell when
            // atlas exists.
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
