#include "System.h"

#include <unordered_set>
#include <vector>

#include "../util/math.h"
#include "VertexData.h"

namespace blob {

    float snap(float v, float cellSize) { return std::floor(v / cellSize + 0.5f) * cellSize; }

    AABB snapAABB(const AABB &b, float cellSize) {
        return {
            glm::vec3(
                    floor(b.min.x / cellSize) * cellSize, floor(b.min.y / cellSize) * cellSize,
                    floor(b.min.z / cellSize) * cellSize
            ),
            glm::vec3(ceil(b.max.x / cellSize) * cellSize, ceil(b.max.y / cellSize) * cellSize, ceil(b.max.z / cellSize) * cellSize)
        };
    }

    int nextPowerOfTwo(int n) {
        if (n <= 1)
            return 1;
        return 1 << (int) std::ceil(std::log2(n));
    }

    System::System(const vma::Allocator &allocator, const vk::Device &device, int count, float cell_size)
        : cellSize(cell_size), mBalls(count), mDomains(count * 2) {
        assert(count <= 16 && "A maxmimum of 16 metaballs are supported");

        mMetaballBuffer = Buffer::create(
                allocator,
                {
                    .size = sizeof(MetaballBlock) * count,
                    .usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
                }
        );
        util::setDebugName(device, *mMetaballBuffer.buffer, "blob_metaball_buffer");

        mTrash.create(globals::MaxFramesInFlight + 1, [] { return std::vector<std::function<void()>>(); });

        // Pre-allocate
        resizeDomainMemberBuffer(allocator, device, 1024*1024);
        resizeDrawIndirectBuffer(allocator, device, 512);
        resizeVertexBuffer(allocator, device, 1024);
    }

    void System::update(const vma::Allocator &allocator, const vk::Device &device, const vk::CommandBuffer &cmd_buf) {
        auto &trash = mTrash.next();
        for (const auto &t: trash) {
            t();
        }
        trash.clear();

        partition();

        resizeDrawIndirectBuffer(allocator, device, mDomains.size());

        size_t required_count = 0;
        for (const auto &d: mDomains) {
            required_count += estimateVertexCount(d);
        }
        resizeVertexBuffer(allocator, device, required_count);

        std::vector<MetaballBlock> metaball_data(mBalls.size());
        for (size_t i = 0; i < mBalls.size(); i++) {
            const auto &b = mBalls[i];
            metaball_data[i] = {
                .center = glm::vec4(b.center, 0.0),
                .scale = glm::vec4(b.scale, 1.0),
                .baseRadius = b.baseRadius,
                .maxRadius = b.maxRadius,
            };
        }

        mMetaballBuffer.barrier(cmd_buf, BufferResourceAccess::TransferWrite);
        assert(metaball_data.size() * sizeof(MetaballBlock) < 65536);
        cmd_buf.updateBuffer(mMetaballBuffer, 0, metaball_data.size() * sizeof(MetaballBlock), metaball_data.data());

        // Calculate total members across all domains
        size_t totalMembers = 0;
        for (const auto &d: mDomains) {
            totalMembers += d.members.size();
        }

        std::vector<uint32_t> domain_members(totalMembers);
        size_t metaball_index_offset = 0;
        for (const auto &d: mDomains) {
            for (int i: d.members) {
                domain_members[metaball_index_offset++] = i;
            }
        }

        resizeDomainMemberBuffer(allocator, device, domain_members.size());
        mDomainMemberBuffer.barrier(cmd_buf, BufferResourceAccess::TransferWrite);
        assert(domain_members.size() * sizeof(uint32_t) < 65536);
        cmd_buf.updateBuffer(mDomainMemberBuffer, 0, domain_members.size() * sizeof(uint32_t), domain_members.data());
    }

    size_t System::estimateVertexCount(const Domain &domain) const {
        constexpr size_t MAX_VERTS_PER_CELL = 12;
        constexpr float ESTIMATE = 0.5f;
        float ratio = 8.0f; // That's macroCellSize / cellSize;
        size_t totalCells = static_cast<size_t>(ratio * ratio * ratio);
        return totalCells * MAX_VERTS_PER_CELL * ESTIMATE;
    }

    // Helper for sorting
    struct GridKey {
        int x, y, z;
        bool operator<(const GridKey &o) const {
            if (x != o.x)
                return x < o.x;
            if (y != o.y)
                return y < o.y;
            return z < o.z;
        }
        bool operator==(const GridKey &o) const { return x == o.x && y == o.y && z == o.z; }
    };

    void System::partition() {
        mDomains.clear();
        if (mBalls.empty())
            return;

        float macroCellSize = cellSize * 8.0f;
        float padding = macroCellSize * 0.5f;

        // Calculate half-diagonal for conservative voxel culling
        // Distance from center of voxel to its corner
        float voxelRadius = (macroCellSize * 1.73205f) * 0.5f;

        // 1. Identify Active Voxels (Geometric Shell)
        std::vector<GridKey> activeKeys;
        activeKeys.reserve(mBalls.size() * 64);

        for (const auto &ball: mBalls) {
            float maxS = std::max({ball.scale.x, ball.scale.y, ball.scale.z});
            float minS = std::min({ball.scale.x, ball.scale.y, ball.scale.z});

            float rOuter = ball.maxRadius * maxS;

            // Inner radius for Core Culling.
            // Use minS to be safe (if flattened, the core is thin).
            float rInner = ball.baseRadius * minS;

            glm::vec3 minCorner = ball.center - glm::vec3(rOuter);
            glm::vec3 maxCorner = ball.center + glm::vec3(rOuter);

            glm::ivec3 minVoxel = glm::ivec3(glm::floor((minCorner - origin) / macroCellSize));
            glm::ivec3 maxVoxel = glm::ivec3(glm::floor((maxCorner - origin) / macroCellSize));

            for (int z = minVoxel.z; z <= maxVoxel.z; ++z) {
                for (int y = minVoxel.y; y <= maxVoxel.y; ++y) {
                    for (int x = minVoxel.x; x <= maxVoxel.x; ++x) {

                        // Calculate center of this potential domain
                        glm::vec3 voxelCenter = origin + (glm::vec3(x, y, z) + 0.5f) * macroCellSize;
                        float dist = glm::distance(ball.center, voxelCenter);

                        // OPTIMIZATION A: Cull corners of the bounding box.
                        // If the voxel is completely outside the outer radius, skip it.
                        if (dist > rOuter + voxelRadius)
                            continue;

                        // OPTIMIZATION B: Cull solid core.
                        // If the voxel is completely inside the inner radius, skip it.
                        // (We subtract voxelRadius to ensure the WHOLE voxel is inside).
                        if (dist < rInner - voxelRadius)
                            continue;

                        activeKeys.push_back({x, y, z});
                    }
                }
            }
        }

        // 2. Deduplicate
        if (activeKeys.empty())
            return;
        std::sort(activeKeys.begin(), activeKeys.end());
        activeKeys.erase(std::unique(activeKeys.begin(), activeKeys.end()), activeKeys.end());

        // 3. Build Domains (Keep exactly as Phase 1.6)
        mDomains.reserve(activeKeys.size());

        for (const auto &key: activeKeys) {
            Domain d;

            glm::vec3 voxelMin = origin + glm::vec3(key.x, key.y, key.z) * macroCellSize;
            glm::vec3 voxelMax = voxelMin + glm::vec3(macroCellSize);

            d.bounds.min = voxelMin;
            d.bounds.max = voxelMax;

            glm::vec3 checkMin = voxelMin - glm::vec3(padding);
            glm::vec3 checkMax = voxelMax + glm::vec3(padding);

            for (int i = 0; i < mBalls.size(); ++i) {
                const auto &ball = mBalls[i];
                float maxS = std::max({ball.scale.x, ball.scale.y, ball.scale.z});
                float rOuter = ball.maxRadius * maxS;

                glm::vec3 ballMin = ball.center - glm::vec3(rOuter);
                glm::vec3 ballMax = ball.center + glm::vec3(rOuter);

                bool overlapX = (ballMin.x <= checkMax.x && ballMax.x >= checkMin.x);
                bool overlapY = (ballMin.y <= checkMax.y && ballMax.y >= checkMin.y);
                bool overlapZ = (ballMin.z <= checkMax.z && ballMax.z >= checkMin.z);

                if (overlapX && overlapY && overlapZ) {
                    d.members.push_back(i);
                }
            }

            if (!d.members.empty()) {
                std::sort(d.members.begin(), d.members.end());
                mDomains.push_back(d);
            }
        }
    }


    void System::resizeVertexBuffer(const vma::Allocator &allocator, const vk::Device &device, size_t required_count) {
        size_t current_count = mVertexBuffer.size / sizeof(VertexData);
        size_t reallocated_count = 0;
        // if (required_count > current_count || required_count < current_count / 2) {
        if (required_count > current_count) {
            reallocated_count = static_cast<size_t>(1.5 * static_cast<double>(required_count));
        } else {
            return;
        }

        if (mVertexBuffer) {
            auto old_buffer = mVertexBuffer.buffer.release();
            auto old_alloc = mVertexBuffer.allocation.release();
            mTrash.get().emplace_back([allocator, old_buffer, old_alloc]() {
                allocator.destroyBuffer(old_buffer, old_alloc);
            });
        }

        mVertexBuffer = Buffer::create(
                allocator,
                {
                    .size = sizeof(VertexData) * reallocated_count,
                    .usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eVertexBuffer,
                }
        );
        util::setDebugName(device, *mVertexBuffer.buffer, "blob_vertex_buffer");
    }

    void System::resizeDrawIndirectBuffer(const vma::Allocator &allocator, const vk::Device &device, size_t required_count) {
        size_t current_count = mDrawIndirectBuffer.size / sizeof(vk::DrawIndirectCommand);
        size_t reallocated_count = 0;
        // if (required_count > current_count || required_count < current_count / 2) {
        if (required_count > current_count) {
            reallocated_count = static_cast<size_t>(1.5 * static_cast<double>(required_count));
        } else {
            return;
        }

        if (mDrawIndirectBuffer) {
            auto old_buffer = mDrawIndirectBuffer.buffer.release();
            auto old_alloc = mDrawIndirectBuffer.allocation.release();
            mTrash.get().emplace_back([allocator, old_buffer, old_alloc]() {
                allocator.destroyBuffer(old_buffer, old_alloc);
            });
        }

        mDrawIndirectBuffer = Buffer::create(
                allocator,
                {
                    .size = sizeof(vk::DrawIndirectCommand) * reallocated_count,
                    .usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer |
                             vk::BufferUsageFlagBits::eTransferDst,
                }
        );
        util::setDebugName(device, *mDrawIndirectBuffer.buffer, "blob_draw_indirect_buffer");
    }

    void System::resizeDomainMemberBuffer(const vma::Allocator &allocator, const vk::Device &device, size_t required_count) {
        size_t current_count = mDomainMemberBuffer.size / sizeof(uint32_t);
        size_t reallocated_count = 0;
        // if (required_count > current_count || required_count < current_count / 2) {
        if (required_count > current_count) {
            reallocated_count = static_cast<size_t>(1.5 * static_cast<double>(required_count));
        } else {
            return;
        }

        if (mDomainMemberBuffer) {
            auto old_buffer = mDomainMemberBuffer.buffer.release();
            auto old_alloc = mDomainMemberBuffer.allocation.release();
            mTrash.get().emplace_back([allocator, old_buffer, old_alloc]() {
                allocator.destroyBuffer(old_buffer, old_alloc);
            });
        }

        mDomainMemberBuffer = Buffer::create(
                allocator,
                {
                    .size = sizeof(uint32_t) * reallocated_count,
                    .usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
                }
        );
        util::setDebugName(device, *mDomainMemberBuffer.buffer, "blob_domain_member_buffer");
    }

} // namespace blob
