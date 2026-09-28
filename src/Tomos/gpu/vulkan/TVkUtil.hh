#pragma once

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

#include "Tomos/util/path/TPath.hh"

namespace Tomos::VkUtil
{
    inline uint32_t findMemoryType( VkPhysicalDevice p_physDevice, uint32_t p_typeBits, VkMemoryPropertyFlags p_props )
    {
        VkPhysicalDeviceMemoryProperties memProps{};
        vkGetPhysicalDeviceMemoryProperties( p_physDevice, &memProps );

        for ( uint32_t i = 0; i < memProps.memoryTypeCount; ++i )
        {
            const bool typeBitSet = ( p_typeBits & ( 1u << i ) ) != 0;
            const bool propsMatch = ( memProps.memoryTypes[ i ].propertyFlags & p_props ) == p_props;
            if ( typeBitSet && propsMatch ) return i;
        }

        throw std::runtime_error( "[TVkUtil] No suitable memory type found" );
    }

    inline void immediateSubmit( VkDevice p_device, VkCommandPool p_pool, VkQueue p_queue, const auto& p_fn )
    {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool        = p_pool;
        allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer cmd{};
        vkAllocateCommandBuffers( p_device, &allocInfo, &cmd );

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer( cmd, &beginInfo );

        p_fn( cmd );

        vkEndCommandBuffer( cmd );

        VkSubmitInfo submitInfo{};
        submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers    = &cmd;

        vkQueueSubmit( p_queue, 1, &submitInfo, VK_NULL_HANDLE );
        vkQueueWaitIdle( p_queue );
        vkFreeCommandBuffers( p_device, p_pool, 1, &cmd );
    }

    inline void imageBarrier( VkCommandBuffer p_cmd, VkImage p_image, VkImageLayout p_oldLayout, VkImageLayout p_newLayout, VkPipelineStageFlags2 p_srcStage,
                              VkAccessFlags2 p_srcAccess, VkPipelineStageFlags2 p_dstStage, VkAccessFlags2 p_dstAccess,
                              VkImageAspectFlags p_aspect = VK_IMAGE_ASPECT_COLOR_BIT, uint32_t p_layers = 1, uint32_t p_mipLevels = 1,
                              uint32_t p_baseLayer = 0 )
    {
        VkImageMemoryBarrier2 barrier{};
        barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        barrier.srcStageMask                    = p_srcStage;
        barrier.srcAccessMask                   = p_srcAccess;
        barrier.dstStageMask                    = p_dstStage;
        barrier.dstAccessMask                   = p_dstAccess;
        barrier.oldLayout                       = p_oldLayout;
        barrier.newLayout                       = p_newLayout;
        barrier.image                           = p_image;
        barrier.subresourceRange.aspectMask     = p_aspect;
        barrier.subresourceRange.baseMipLevel   = 0;
        barrier.subresourceRange.levelCount     = p_mipLevels;
        barrier.subresourceRange.baseArrayLayer = p_baseLayer;
        barrier.subresourceRange.layerCount     = p_layers;

        VkDependencyInfo dep{};
        dep.sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dep.imageMemoryBarrierCount = 1;
        dep.pImageMemoryBarriers    = &barrier;
        vkCmdPipelineBarrier2( p_cmd, &dep );
    }
    // compute-write → fragment-read on storage buffers.
    inline void memoryBarrier( VkCommandBuffer p_cmd, VkPipelineStageFlags2 p_srcStage, VkAccessFlags2 p_srcAccess, VkPipelineStageFlags2 p_dstStage,
                               VkAccessFlags2 p_dstAccess )
    {
        VkMemoryBarrier2 barrier{};
        barrier.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
        barrier.srcStageMask  = p_srcStage;
        barrier.srcAccessMask = p_srcAccess;
        barrier.dstStageMask  = p_dstStage;
        barrier.dstAccessMask = p_dstAccess;

        VkDependencyInfo dep{};
        dep.sType              = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dep.memoryBarrierCount = 1;
        dep.pMemoryBarriers    = &barrier;
        vkCmdPipelineBarrier2( p_cmd, &dep );
    }

    inline VkShaderModule loadSpv( VkDevice p_device, const char* p_path )
    {
        std::ifstream file( p_path, std::ios::ate | std::ios::binary );
        if ( !file.is_open() ) throw std::runtime_error( std::string( "[VkUtil] Failed to open shader: " ) + p_path );

        const size_t      size = static_cast<size_t>( file.tellg() );
        std::vector<char> buf( size );
        file.seekg( 0 );
        file.read( buf.data(), static_cast<std::streamsize>( size ) );

        VkShaderModuleCreateInfo ci{};
        ci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        ci.codeSize = size;
        ci.pCode    = reinterpret_cast<const uint32_t*>( buf.data() );

        VkShaderModule mod{};
        if ( vkCreateShaderModule( p_device, &ci, nullptr, &mod ) != VK_SUCCESS )
            throw std::runtime_error( std::string( "[VkUtil] Failed to create shader module: " ) + p_path );
        return mod;
    }

    inline std::string resolveShaderPath( const char* p_fileName )
    {
        namespace fs = std::filesystem;

        const auto tryPath = []( const fs::path& p ) -> std::string {
            if ( std::ifstream( p ).good() ) return p.string();
            return {};
        };

        if ( std::string found = tryPath( TPath::assetRoot() / "shaders" / p_fileName ); !found.empty() )
            return found;

        if ( std::string found = tryPath( TPath::executableDir() / "shaders" / p_fileName ); !found.empty() )
            return found;

        if ( std::string found = tryPath( fs::path( "shaders" ) / p_fileName ); !found.empty() ) return found;

#ifdef TOMOS_SHADER_DIR
        if ( std::string found = tryPath( fs::path( TOMOS_SHADER_DIR ) / p_fileName ); !found.empty() ) return found;
#endif

        throw std::runtime_error( std::string( "[VkUtil] Shader not found: " ) + p_fileName );
    }
}  // namespace Tomos::VkUtil
