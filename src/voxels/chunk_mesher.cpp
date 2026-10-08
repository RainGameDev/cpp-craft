#include "voxels/chunk_mesher.h"
#include "voxels/voxel_registry.h"
#include "voxels/world.h"

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

MeshData meshChunkSnapshot(const ChunkMeshSnapshot &snapshot,
                           const VoxelRegistry &registry,
                           const AtlasLayout &atlasLayout) {
  const Chunk *n[6] = {0};
  for (int i = 0; i < 6; i++)
    if (snapshot.neighbors[i])
      n[i] = &*snapshot.neighbors[i];

  MeshData mesh;
  mesh.reserve(8192, 12288);

  const int baseX = snapshot.central.pos.x * CW;
  const int baseY = snapshot.central.pos.y * CH;
  const int baseZ = snapshot.central.pos.z * CD;

  for (int localY = 0; localY < CH; ++localY)
    for (int localZ = 0; localZ < CD; ++localZ)
      for (int localX = 0; localX < CW; ++localX) {
        VoxelId id = snapshot.central.getLocal(localX, localY, localZ);
        if (id == AIR)
          continue;
        const VoxelDef &def = registry.get(id);
        if (def.render == VoxelDef::None)
          continue;
        for (int f = 0; f < 6; ++f) {
          const MesherFace &face = FACES[f];
          int lx = localX + face.dir[0], ly = localY + face.dir[1],
              lz = localZ + face.dir[2];
          VoxelId nid;
          if ((unsigned)lx < CW && (unsigned)ly < CH && (unsigned)lz < CD)
            nid = snapshot.central.voxels[Chunk::idx(lx, ly, lz)];
          else if (!n[f])
            nid = AIR;
          else
            nid = n[f]->voxels[Chunk::idx((lx + CW) % CW, (ly + CH) % CH,
                                          (lz + CD) % CD)];
          const VoxelDef &nd = registry.get(nid);
          if (!shouldShowFace(def, nd))
            continue;
          const uint32_t baseVertex =
              static_cast<uint32_t>(mesh.vertices.size());
          const auto &cell = def.textureCoords[f];
          for (int c = 0; c < 4; ++c) {
            Vertex v;
            v.pos = glm::vec3(baseX + localX + face.corners[c][0],
                              baseY + localY + face.corners[c][1],
                              baseZ + localZ + face.corners[c][2]);
            v.shade = face.shade;
            v.uv = glm::vec2((cell[0] + FACE_UV[c][0]) / atlasLayout.columns,
                             (cell[1] + FACE_UV[c][1]) / atlasLayout.rows);
            mesh.vertices.push_back(v);
          }
          mesh.indices.insert(mesh.indices.end(),
                              {baseVertex, baseVertex + 1, baseVertex + 2,
                               baseVertex, baseVertex + 2, baseVertex + 3});
        }
      }
  return mesh;
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
