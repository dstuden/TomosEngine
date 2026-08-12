#pragma once

#include <vulkan/vulkan.h>

#include "Tomos/gpu/TGpuEnums.hh"

namespace Tomos
{
    class TVkBuffer
    {
    public:
        TVkBuffer() = default;
        TVkBuffer( VkDevice p_device, VkPhysicalDevice p_physDevice, size_t p_size, TBufUsage p_usage );
        ~TVkBuffer();

        TVkBuffer( const TVkBuffer& )            = delete;
        TVkBuffer& operator=( const TVkBuffer& ) = delete;
        TVkBuffer( TVkBuffer&& p_other ) noexcept;
        TVkBuffer& operator=( TVkBuffer&& p_other ) noexcept;

        // Host-visible buffers only (Uniform / Storage).
        void upload( const void* p_data, size_t p_offset, size_t p_size ) const;

        [[nodiscard]] VkBuffer handle() const { return m_buffer; }
        [[nodiscard]] size_t   size() const { return m_size; }
        [[nodiscard]] bool     valid() const { return m_buffer != VK_NULL_HANDLE; }

    private:
        VkDevice       m_device = VK_NULL_HANDLE;
        VkBuffer       m_buffer = VK_NULL_HANDLE;
        VkDeviceMemory m_memory = VK_NULL_HANDLE;
        void*          m_mapped = nullptr;
        size_t         m_size   = 0;
    };
}  // namespace Tomos
