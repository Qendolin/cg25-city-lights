#pragma once

#include <source_location>
#include <vulkan/vulkan.hpp>

// namespace util?? idk...
namespace util {

    class ScopedCommandLabel {
        vk::CommandBuffer mCmd;
        mutable int32_t mCount = 0;

    public:
        ScopedCommandLabel(const vk::CommandBuffer &cmd, const std::source_location &loc = std::source_location::current())
            : mCmd(cmd) {
            start(loc.function_name());
        }

        ScopedCommandLabel(const vk::CommandBuffer &cmd, const std::string &label) : mCmd(cmd) { start(label.c_str()); }

        ~ScopedCommandLabel();

        void start(const char *label) const {
#ifndef NDEBUG
            mCount++;
            vk::DebugUtilsLabelEXT info;
            info.pLabelName = label;
            mCmd.beginDebugUtilsLabelEXT(info);
#endif
        }

        void end() const {
#ifndef NDEBUG
            mCount--;
            mCmd.endDebugUtilsLabelEXT();
#endif
        }

        void swap(std::string_view new_label) const {
            end();
            start(new_label.data());
        }
    };

    template <typename T>
    void setDebugName(const vk::Device& device, T object, const std::string& name) {
#ifndef NDEBUG
        // Extract the C-API handle type (e.g., VkBuffer or VkCommandBuffer)
        using CType = typename T::CType;
        uint64_t handle = reinterpret_cast<uint64_t>(static_cast<CType>(object));

        // vulkan.hpp classes have a static 'objectType' member
        vk::DebugUtilsObjectNameInfoEXT nameInfo;
        nameInfo.objectType   = T::objectType;
        nameInfo.objectHandle = handle;
        nameInfo.pObjectName  = name.c_str();

        device.setDebugUtilsObjectNameEXT(nameInfo);
#endif
    }
} // namespace util
