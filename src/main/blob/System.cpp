#include "System.h"

#include <glm/ext/quaternion_geometric.hpp>
#include <glm/ext/scalar_constants.hpp>
#include <vector>

#include "../util/math.h"
#include "VertexData.h"

namespace blob {

    float snap(float v, float cellSize) {
        return std::floor(v / cellSize + 0.5f) * cellSize;
    }

    // Conservative Grid Snap
    AABB snapAABB(const AABB& b, float cellSize) {
        return {
            glm::vec3(floor(b.min.x/cellSize)*cellSize, floor(b.min.y/cellSize)*cellSize, floor(b.min.z/cellSize)*cellSize),
            glm::vec3(ceil(b.max.x/cellSize)*cellSize,  ceil(b.max.y/cellSize)*cellSize,  ceil(b.max.z/cellSize)*cellSize)
        };
    }

    int nextPowerOfTwo(int n) {
        if (n <= 1) return 1;
        return 1 << (int)std::ceil(std::log2(n));
    }

    // Get the tight bounding box of a specific set of balls
    AABB getBallsBounds(const std::vector<int>& indices, const std::vector<Metaball>& balls) {
        AABB bounds = {{1e10, 1e10, 1e10}, {-1e10, -1e10, -1e10}};
        if (indices.empty()) return {{0,0,0},{0,0,0}};

        for (int idx : indices) {
            const auto& b = balls[idx];
            float maxS = std::max({b.scale.x, b.scale.y, b.scale.z});
            glm::vec3 r = glm::vec3(b.maxRadius * maxS);
            bounds.min = glm::min(bounds.min, b.center - r);
            bounds.max = glm::max(bounds.max, b.center + r);
        }
        return bounds;
    }

    // --- Core Split Logic ---

    // Recursively split 'spaceBounds' containing 'candidates' into 'targetCount' subdivisions
    void splitToTarget(
        AABB spaceBounds,
        const std::vector<int>& candidates,
        const std::vector<Metaball>& allBalls,
        std::vector<Domain>& outDomains,
        int targetCount,
        float cellSize
    ) {
        // 1. FILTER: Which balls actually touch this space?
        //    (Fixes your "center point" bug)
        std::vector<int> activeBalls;
        AABB ballsUnion = {{1e10, 1e10, 1e10}, {-1e10, -1e10, -1e10}};

        for (int idx : candidates) {
            const auto& b = allBalls[idx];
            float maxS = std::max({b.scale.x, b.scale.y, b.scale.z});
            glm::vec3 r = glm::vec3(b.maxRadius * maxS);
            AABB ballBox = {b.center - r, b.center + r};

            if (spaceBounds.overlaps(ballBox)) {
                activeBalls.push_back(idx);
                ballsUnion.min = glm::min(ballsUnion.min, ballBox.min);
                ballsUnion.max = glm::max(ballsUnion.max, ballBox.max);
            }
        }

        if (activeBalls.empty()) return;

        // 2. SHRINK: The valid domain is Intersection(Space, UnionOfBalls)
        //    (Fixes your "shrinkToFit overlaps" bug by respecting the parent split plane via spaceBounds)
        AABB validBounds = {
            glm::max(spaceBounds.min, ballsUnion.min),
            glm::min(spaceBounds.max, ballsUnion.max)
        };
        // 3. SNAP: Align to grid
        //    (Fixes your "cracks" bug)
        validBounds = snapAABB(validBounds, cellSize);

        // Sanity check
        if (!validBounds.isValid()) return;

        // 4. Base Case: Target reached (or forced leaf)
        if (targetCount <= 1) {
            Domain d;
            d.bounds = validBounds;
            d.members = std::move(activeBalls);
            std::sort(d.members.begin(), d.members.end());
            outDomains.push_back(d);
            return;
        }

        // 5. Split: Longest Axis -> Sort by Center -> Find Gap
        glm::vec3 size = validBounds.max - validBounds.min;
        int axis = 0;
        if (size.y > size.x && size.y > size.z) axis = 1;
        else if (size.z > size.x && size.z > size.y) axis = 2;

        // Sort active balls along this axis
        // We copy to a temp vector for sorting to avoid messing up the indices
        std::vector<int> sortedBalls = activeBalls;
        std::sort(sortedBalls.begin(), sortedBalls.end(), [&](int a, int b) {
            return allBalls[a].center[axis] < allBalls[b].center[axis];
        });

        // Heuristic: Try to find a gap, otherwise use spatial midpoint
        float bestSplit = (validBounds.min[axis] + validBounds.max[axis]) * 0.5f;
        float maxGap = -1.0f;

        // Try to find a gap in the actual geometry
        for (size_t i = 0; i + 1 < sortedBalls.size(); ++i) {
            const auto& b1 = allBalls[sortedBalls[i]];
            const auto& b2 = allBalls[sortedBalls[i+1]];

            float maxS1 = std::max({b1.scale.x, b1.scale.y, b1.scale.z});
            float maxS2 = std::max({b2.scale.x, b2.scale.y, b2.scale.z});

            float end1 = b1.center[axis] + b1.maxRadius * maxS1;
            float start2 = b2.center[axis] - b2.maxRadius * maxS2;

            float gap = start2 - end1;
            // Gap must be positive to be a separator
            if (gap > maxGap && gap > 0.0f) {
                maxGap = gap;
                bestSplit = (end1 + start2) * 0.5f;
            }
        }

        // Snap the split plane
        bestSplit = snap(bestSplit, cellSize);

        // Fallback: If split is invalid (on edge) due to snapping or clustering,
        // force spatial center
        if (bestSplit <= validBounds.min[axis] || bestSplit >= validBounds.max[axis]) {
             bestSplit = snap((validBounds.min[axis] + validBounds.max[axis]) * 0.5f, cellSize);
        }

        // Final safety: If we still can't split (e.g. box is 1 cell wide), make leaf
        if (bestSplit <= validBounds.min[axis] || bestSplit >= validBounds.max[axis]) {
            Domain d;
            d.bounds = validBounds;
            d.members = std::move(activeBalls);
            std::sort(d.members.begin(), d.members.end());
            outDomains.push_back(d);
            return;
        }

        AABB left = validBounds;
        AABB right = validBounds;
        left.max[axis] = bestSplit;
        right.min[axis] = bestSplit;

        int targetLeft = targetCount / 2;
        int targetRight = targetCount - targetLeft;

        // Pass ALL active balls to both. They will filter themselves in Step 1.
        splitToTarget(left, activeBalls, allBalls, outDomains, targetLeft, cellSize);
        splitToTarget(right, activeBalls, allBalls, outDomains, targetRight, cellSize);
    }

    System::System(const vma::Allocator &allocator, const vk::Device &device, int count, float cell_size)
        : cellSize(cell_size), mBalls(count), mDomains(count * 2) {
        assert(count <= 16 && "A maxmimum of 16 metaballs are supported");
        mDrawIndirectBuffer = Buffer::create(
                allocator,
                {
                    .size = sizeof(vk::DrawIndirectCommand) * mDomains.size(),
                    .usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer |
                             vk::BufferUsageFlagBits::eTransferDst,
                }
        );
        util::setDebugName(device, *mDrawIndirectBuffer.buffer, "blob_indirect_buffer");

        mMetaballBuffer = Buffer::create(
                allocator,
                {
                    .size = sizeof(MetaballBlock) * count,
                    .usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
                }
        );
        util::setDebugName(device, *mMetaballBuffer.buffer, "blob_metaball_buffer");

        mDomainMemberBuffer = Buffer::create(
                allocator,
                {
                    .size = sizeof(uint32_t) * count * count,
                    .usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
                }
        );
        util::setDebugName(device, *mDomainMemberBuffer.buffer, "blob_domain_member_buffer");

        mTrash.create(globals::MaxFramesInFlight + 1, []{ return std::vector<std::function<void()>>(); });
        // Vertex buffer is allocated during update
    }

    void System::update(const vma::Allocator &allocator, const vk::Device &device, const vk::CommandBuffer &cmd_buf) {
        auto& trash = mTrash.next();
        for (const auto& t : trash) {
            t();
        }
        trash.clear();

        partition();

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
        cmd_buf.updateBuffer(mMetaballBuffer, 0, metaball_data.size() * sizeof(MetaballBlock), metaball_data.data());

        std::vector<uint32_t> domain_members(mBalls.size() * mBalls.size());
        size_t metaball_index_offset = 0;
        for (const auto &d: mDomains) {
            for (int i: d.members) {
                domain_members[metaball_index_offset++] = i;
            }
        }

        mDomainMemberBuffer.barrier(cmd_buf, BufferResourceAccess::TransferWrite);
        cmd_buf.updateBuffer(mDomainMemberBuffer, 0, domain_members.size() * sizeof(uint32_t), domain_members.data());
    }

    size_t System::estimateVertexCount(const Domain &domain) const {
        constexpr size_t MAX_VERTS_PER_CELL = 15; // Max for MC is 15 (5 triangles), though usually < 12
        constexpr float SAFETY_FACTOR = 4.0f;

        // 1. Estimate based on Surface Area of contents
        // (Good for large domains containing whole balls)
        float totalSurfaceArea = 0.0f;
        for (int idx : domain.members) {
            const auto& b = mBalls[idx];

            // Conservative radius using max scale
            float maxS = std::max({b.scale.x, b.scale.y, b.scale.z});
            float r = b.maxRadius * maxS;

            // Area of sphere = 4 * PI * r^2
            totalSurfaceArea += 4.0f * glm::pi<float>() * r * r;
        }

        // Estimate cells needed for this surface area
        // A cell face has area (cellSize^2).
        float cellFaceArea = cellSize * cellSize;
        auto areaBasedEstimate = static_cast<size_t>((totalSurfaceArea / cellFaceArea) * MAX_VERTS_PER_CELL);

        // 2. Estimate based on Domain Volume
        // (Good for small BSP slices where the ball is much larger than the domain)
        glm::vec3 domainSize = domain.bounds.max - domain.bounds.min;

        // Add 1.0 to dimensions to account for boundary cells
        float cellsX = std::ceil(domainSize.x / cellSize) + 1.0f;
        float cellsY = std::ceil(domainSize.y / cellSize) + 1.0f;
        float cellsZ = std::ceil(domainSize.z / cellSize) + 1.0f;

        auto totalCellsInDomain = static_cast<size_t>(cellsX * cellsY * cellsZ);
        size_t volumeBasedEstimate = totalCellsInDomain * MAX_VERTS_PER_CELL;

        // 3. The Result is the Minimum of the two
        // If the domain is tiny, Volume limits it.
        // If the domain is huge but empty, Surface Area limits it.
        size_t count = std::min(areaBasedEstimate, volumeBasedEstimate);

        return static_cast<size_t>(static_cast<float>(count) * SAFETY_FACTOR);
    }

    void System::partition() {
        mDomains.clear();
        if (mBalls.empty()) return;

        // 1. Get AABBs for initial grouping
        std::vector<AABB> ballAABBs;
        ballAABBs.reserve(mBalls.size());
        for (const auto& b : mBalls) {
            float maxS = std::max({b.scale.x, b.scale.y, b.scale.z});
            glm::vec3 r = glm::vec3(b.maxRadius * maxS);
            ballAABBs.push_back({b.center - r, b.center + r});
        }

        // 2. Find Groups (Connected Components)
        // (Using standard N^2 flood fill as per your reference)
        std::vector<std::vector<int>> groups;
        std::vector<bool> visited(mBalls.size(), false);

        for (size_t i = 0; i < mBalls.size(); ++i) {
            if (visited[i]) continue;

            std::vector<int> group;
            std::vector<int> stack = {static_cast<int>(i)};
            visited[i] = true;

            while (!stack.empty()) {
                int curr = stack.back();
                stack.pop_back();
                group.push_back(curr);

                for (size_t j = 0; j < mBalls.size(); ++j) {
                    if (!visited[j] && ballAABBs[curr].overlaps(ballAABBs[j])) {
                        visited[j] = true;
                        stack.push_back(static_cast<int>(j));
                    }
                }
            }
            groups.push_back(group);
        }

        // 3. Process each group
        for (const auto& group : groups) {
            // Merge AABBs to get start space
            AABB groupBounds = getBallsBounds(group, mBalls);
            groupBounds = snapAABB(groupBounds, cellSize);

            // Calculate target count
            int target = util::nextLowestPowerOfTwo(static_cast<int>(group.size()));

            // Recursive Split
            splitToTarget(groupBounds, group, mBalls, mDomains, target, cellSize);
        }
    }


    void System::resizeVertexBuffer(const vma::Allocator &allocator, const vk::Device &device, size_t required_count) {
        size_t current_count = mVertexBuffer.size / sizeof(VertexData);
        size_t reallocated_count = 0;
        // upsize if current maximum is exceeded or downsize if less than half
        if (required_count > current_count || required_count < current_count / 2) {
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


} // namespace blob
