#pragma once
#include <functional>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <vector>

#include "../backend/Buffer.h"
#include "../debug/Annotation.h"
#include "../util/PerFrame.h"


namespace blob {

    struct Metaball {
        glm::vec3 center = glm::vec3(0, 0, 0);
        glm::vec3 scale = glm::vec3(1, 1, 1);
        float baseRadius = 1.0;
        float maxRadius = 2.0;
    };

    struct alignas(16) MetaballBlock {
        glm::vec4 center;
        glm::vec4 scale;
        float baseRadius;
        float maxRadius;
        float pad0 = 0;
        float pad1 = 0;
    };

    struct AABB {
        glm::vec3 min;
        glm::vec3 max;

        [[nodiscard]] bool overlaps(const AABB& o) const {
            return (min.x < o.max.x && max.x > o.min.x) &&
                   (min.y < o.max.y && max.y > o.min.y) &&
                   (min.z < o.max.z && max.z > o.min.z);
        }

        [[nodiscard]] bool isValid() const {
            return min.x < max.x && min.y < max.y && min.z < max.z;
        }
    };

    struct Domain {
        AABB bounds;
        std::vector<int> members;
    };

    class System {
    public:
        float cellSize;
        float groundLevel = 0.0;
        glm::vec3 origin = glm::vec3(0, 0, 0);

        System(const vma::Allocator &allocator, const vk::Device &device, int count, float cell_size);

        void update(const vma::Allocator &allocator, const vk::Device &device, const vk::CommandBuffer &cmd_buf);

        [[nodiscard]] const BufferBase &vertexBuffer() const { return mVertexBuffer; }
        [[nodiscard]] const BufferBase &drawIndirectBuffer() const { return mDrawIndirectBuffer; }
        [[nodiscard]] const BufferBase &metaballBuffer() const { return mMetaballBuffer; }
        [[nodiscard]] const BufferBase &domainMemberBuffer() const { return mDomainMemberBuffer; }

        [[nodiscard]] std::span<Metaball> balls() { return mBalls; }
        [[nodiscard]] std::span<const Metaball> balls() const { return mBalls; }

        [[nodiscard]] std::span<Domain> domains() { return mDomains; }
        [[nodiscard]] std::span<const Domain> domains() const { return mDomains; }

        [[nodiscard]] size_t count() const { return mBalls.size(); }

        [[nodiscard]] size_t estimateVertexCount(const Domain &domain) const;

    private:
        void partition();

        void resizeVertexBuffer(const vma::Allocator &allocator, const vk::Device &device, size_t required_count);

        std::vector<Metaball> mBalls;
        std::vector<Domain> mDomains;

        Buffer mDrawIndirectBuffer;
        Buffer mMetaballBuffer;
        Buffer mVertexBuffer;
        Buffer mDomainMemberBuffer;

        util::PerFrame<std::vector<std::function<void()>>> mTrash;
    };
} // namespace blob
