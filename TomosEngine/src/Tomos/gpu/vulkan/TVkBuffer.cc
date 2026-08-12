#include "Tomos/gpu/vulkan/TVkBuffer.hh"

#include <cstring>
#include <stdexcept>

#include "Tomos/gpu/vulkan/TVkUtil.hh"

namespace Tomos
{
    static VkBufferUsageFlags toVkBufferUsage( TBufUsage p_usage )
    {
        VkBufferUsageFlags flags = 0;
        if ( p_usage & TBufUsage::CopySrc ) flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        if ( p_usage & TBufUsage::CopyDst ) flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        if ( p_usage & TBufUsage::Vertex ) flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        if ( p_usage & TBufUsage::Index ) flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        if ( p_usage & TBufUsage::Uniform ) flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        if ( p_usage & TBufUsage::Storage ) flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        if ( p_usage & TBufUsage::Indirect ) flags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
        return flags;
    }

    TVkBuffer::TVkBuffer( VkDevice p_device, VkPhysicalDevice p_physDevice, size_t p_size, TBufUsage p_usage ) : m_device( p_device ), m_size( p_size )
    {
        VkBufferCreateInfo bufInfo{};
        bufInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufInfo.size        = p_size;
        bufInfo.usage       = toVkBufferUsage( p_usage );
        bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if ( vkCreateBuffer( m_device, &bufInfo, nullptr, &m_buffer ) != VK_SUCCESS ) throw std::runtime_error( "[TVkBuffer] Failed to create VkBuffer" );

        VkMemoryRequirements memReq{};
        vkGetBufferMemoryRequirements( m_device, m_buffer, &memReq );

        // Host-visible buffers (uniforms, storage) are kept persistently mapped.
        // Vertex / index buffers use device-local memory.
        const bool            hostVisible = ( p_usage & TBufUsage::Uniform ) || ( p_usage & TBufUsage::Storage );
        VkMemoryPropertyFlags props =
                hostVisible ? ( VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT ) : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize  = memReq.size;
        allocInfo.memoryTypeIndex = VkUtil::findMemoryType( p_physDevice, memReq.memoryTypeBits, props );

        if ( vkAllocateMemory( m_device, &allocInfo, nullptr, &m_memory ) != VK_SUCCESS )
            throw std::runtime_error( "[TVkBuffer] Failed to allocate buffer memory" );

        vkBindBufferMemory( m_device, m_buffer, m_memory, 0 );

        if ( hostVisible ) vkMapMemory( m_device, m_memory, 0, p_size, 0, &m_mapped );
    }

    TVkBuffer::~TVkBuffer()
    {
        if ( m_device == VK_NULL_HANDLE ) return;
        if ( m_mapped != nullptr ) vkUnmapMemory( m_device, m_memory );
        if ( m_buffer != VK_NULL_HANDLE ) vkDestroyBuffer( m_device, m_buffer, nullptr );
        if ( m_memory != VK_NULL_HANDLE ) vkFreeMemory( m_device, m_memory, nullptr );
    }

    TVkBuffer::TVkBuffer( TVkBuffer&& p_other ) noexcept :
        m_device( p_other.m_device ), m_buffer( p_other.m_buffer ), m_memory( p_other.m_memory ), m_mapped( p_other.m_mapped ), m_size( p_other.m_size )
    {
        p_other.m_device = VK_NULL_HANDLE;
        p_other.m_buffer = VK_NULL_HANDLE;
        p_other.m_memory = VK_NULL_HANDLE;
        p_other.m_mapped = nullptr;
    }

    TVkBuffer& TVkBuffer::operator=( TVkBuffer&& p_other ) noexcept
    {
        if ( this == &p_other ) return *this;
        this->~TVkBuffer();
        new ( this ) TVkBuffer( std::move( p_other ) );
        return *this;
    }

    void TVkBuffer::upload( const void* p_data, size_t p_offset, size_t p_size ) const
    {
        if ( m_mapped == nullptr ) throw std::runtime_error( "[TVkBuffer] Cannot upload — buffer is not host-visible" );
        std::memcpy( static_cast<char*>( m_mapped ) + p_offset, p_data, p_size );
    }
}  // namespace Tomos
