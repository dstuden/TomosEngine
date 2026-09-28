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

    static bool isHostVisible( TBufUsage p_usage )
    {
        return ( p_usage & TBufUsage::Uniform ) || ( p_usage & TBufUsage::Storage ) || ( p_usage & TBufUsage::Staging );
    }

    static bool isPersistentlyMapped( TBufUsage p_usage )
    {
        return ( p_usage & TBufUsage::Uniform ) || ( p_usage & TBufUsage::Storage );
    }

    TVkBuffer::TVkBuffer( VkDevice p_device, VkPhysicalDevice p_physDevice, size_t p_size, TBufUsage p_usage ) :
        m_device( p_device ), m_size( p_size ), m_usage( p_usage )
    {
        VkBufferCreateInfo bufInfo{};
        bufInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufInfo.size        = p_size;
        bufInfo.usage       = toVkBufferUsage( p_usage );
        bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if ( vkCreateBuffer( m_device, &bufInfo, nullptr, &m_buffer ) != VK_SUCCESS ) throw std::runtime_error( "[TVkBuffer] Failed to create VkBuffer" );

        VkMemoryRequirements memReq{};
        vkGetBufferMemoryRequirements( m_device, m_buffer, &memReq );

        // Host-visible buffers (uniforms, storage, upload staging) use coherent memory.
        // Vertex / index buffers use device-local memory.
        const bool            hostVisible = isHostVisible( p_usage );
        VkMemoryPropertyFlags props =
                hostVisible ? ( VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT ) : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize  = memReq.size;
        allocInfo.memoryTypeIndex = VkUtil::findMemoryType( p_physDevice, memReq.memoryTypeBits, props );

        if ( vkAllocateMemory( m_device, &allocInfo, nullptr, &m_memory ) != VK_SUCCESS )
            throw std::runtime_error( "[TVkBuffer] Failed to allocate buffer memory" );

        vkBindBufferMemory( m_device, m_buffer, m_memory, 0 );

        if ( hostVisible && isPersistentlyMapped( p_usage ) ) vkMapMemory( m_device, m_memory, 0, p_size, 0, &m_mapped );
    }

    TVkBuffer::~TVkBuffer()
    {
        if ( m_device == VK_NULL_HANDLE ) return;
        if ( m_mapped != nullptr ) vkUnmapMemory( m_device, m_memory );
        if ( m_buffer != VK_NULL_HANDLE ) vkDestroyBuffer( m_device, m_buffer, nullptr );
        if ( m_memory != VK_NULL_HANDLE ) vkFreeMemory( m_device, m_memory, nullptr );
    }

    TVkBuffer::TVkBuffer( TVkBuffer&& p_other ) noexcept :
        m_device( p_other.m_device ), m_buffer( p_other.m_buffer ), m_memory( p_other.m_memory ), m_mapped( p_other.m_mapped ), m_size( p_other.m_size ),
        m_usage( p_other.m_usage )
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
        if ( !isHostVisible( m_usage ) ) throw std::runtime_error( "[TVkBuffer] Cannot upload — buffer is not host-visible" );

        if ( m_mapped != nullptr )
        {
            std::memcpy( static_cast<char*>( m_mapped ) + p_offset, p_data, p_size );
            return;
        }

        void* mapped = nullptr;
        if ( vkMapMemory( m_device, m_memory, p_offset, p_size, 0, &mapped ) != VK_SUCCESS )
            throw std::runtime_error( "[TVkBuffer] Failed to map staging buffer for upload" );
        std::memcpy( mapped, p_data, p_size );
        vkUnmapMemory( m_device, m_memory );
    }
}  // namespace Tomos
