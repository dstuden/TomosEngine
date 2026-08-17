#include "Tomos/gpu/vulkan/renderer/TVkClusteredRenderer.hh"

#include <stdexcept>
#include <vector>

#include "Tomos/gpu/TGpuEnums.hh"
#include "Tomos/gpu/vulkan/TVkGpu.hh"
#include "Tomos/gpu/vulkan/TVkUtil.hh"
#include "Tomos/gpu/vulkan/post/TPostBloom.hh"
#include "Tomos/gpu/vulkan/post/TPostFog.hh"
#include "Tomos/gpu/vulkan/post/TPostSSAO.hh"
#include "Tomos/gpu/vulkan/post/TPostTonemap.hh"

namespace Tomos
{
    TVkClusteredRenderer::TVkClusteredRenderer( TVkGpu& p_gpu ) : m_gpu( p_gpu )
    {
        m_renderExtent = m_gpu.extent();
        createDepth();
        createHdr();
        createSceneColor();
        createShadowResources();
        createLayouts();
        createParticleResources();
        createFrameResources();
        createPipelines();
        initPostStack();
    }

    TVkClusteredRenderer::~TVkClusteredRenderer()
    {
        const VkDevice device = m_gpu.device();

        destroyGraphicsPipelines();

        vkDestroyPipelineLayout( device, m_shadowPipeLayout, nullptr );
        vkDestroyPipelineLayout( device, m_cullPipeLayout, nullptr );
        vkDestroyPipelineLayout( device, m_forwardPipeLayout, nullptr );
        vkDestroyPipelineLayout( device, m_spritePipeLayout, nullptr );
        vkDestroyPipelineLayout( device, m_particleSimPipeLayout, nullptr );
        vkDestroyPipelineLayout( device, m_particleDrawPipeLayout, nullptr );

        vkDestroyDescriptorSetLayout( device, m_sceneLayout, nullptr );
        vkDestroyDescriptorSetLayout( device, m_cullLayout, nullptr );
        vkDestroyDescriptorSetLayout( device, m_spriteSetLayout, nullptr );
        vkDestroyDescriptorSetLayout( device, m_spriteTexLayout, nullptr );
        vkDestroyDescriptorSetLayout( device, m_particleSimLayout, nullptr );
        vkDestroyDescriptorSetLayout( device, m_particleDrawLayout, nullptr );

        for ( VkImageView view : m_shadowLayerViews )
            if ( view != VK_NULL_HANDLE ) vkDestroyImageView( device, view, nullptr );
    }

    TPostContext TVkClusteredRenderer::makePostContext()
    {
        TPostContext ctx{};
        ctx.m_gpu          = &m_gpu;
        ctx.m_extent       = m_renderExtent;
        ctx.m_hdr          = &m_hdrA;
        ctx.m_hdrOther     = &m_hdrB;
        ctx.m_depth        = &m_depth;
        ctx.m_outputFormat = m_gpu.swapFormat();
        return ctx;
    }

    void TVkClusteredRenderer::onResize()
    {
        // Swapchain rebuild only: in editor mode HDR/sceneColor follow the Scene
        // panel (renderExtent), not the swapchain. Recreating them here would
        // destroy views still referenced by ImGui_ImplVulkan_AddTexture.
        if ( m_gpu.presentMode() == TVkGpu::TPresentMode::EditorViewport )
        {
            if ( m_renderExtent.width == 0 || m_renderExtent.height == 0 )
            {
                m_renderExtent = m_gpu.extent();
                onRenderExtentChanged( m_renderExtent );
            }
            // Tonemap pipelines key off swap format — refresh post if format changed.
            m_post.onResize( makePostContext() );
            return;
        }

        m_renderExtent = m_gpu.extent();
        onRenderExtentChanged( m_renderExtent );
    }

    void TVkClusteredRenderer::onRenderExtentChanged( VkExtent2D p_extent )
    {
        if ( p_extent.width == 0 || p_extent.height == 0 ) return;
        // Skip no-op recreates (would still destroy/rebuild images and invalidate ImGui tex).
        // Compare against actual image size — callers may have already updated m_renderExtent.
        if ( m_sceneColor.valid() && m_hdrA.valid() && m_depth.valid() && m_sceneColor.width() == p_extent.width && m_sceneColor.height() == p_extent.height )
        {
            m_renderExtent = p_extent;
            return;
        }

        m_renderExtent = p_extent;

        createDepth();
        createHdr();
        createSceneColor();

        m_post.onResize( makePostContext() );
    }

    void TVkClusteredRenderer::initPostStack()
    {
        // Fixed legal order: SSAO → fog → bloom → tonemap (tonemap always last).
        m_post.add( std::make_unique<TPostSSAO>() );
        m_post.add( std::make_unique<TPostFog>() );
        m_post.add( std::make_unique<TPostBloom>() );
        m_post.add( std::make_unique<TPostTonemap>() );

        m_post.onResize( makePostContext() );
    }

    TImgFormat TVkClusteredRenderer::imgFormatFromVk( VkFormat p_format )
    {
        switch ( p_format )
        {
            case VK_FORMAT_B8G8R8A8_SRGB:
                return TImgFormat::B8G8R8A8Srgb;
            case VK_FORMAT_B8G8R8A8_UNORM:
                return TImgFormat::B8G8R8A8Unorm;
            case VK_FORMAT_R8G8B8A8_SRGB:
                return TImgFormat::RGBA8Srgb;
            case VK_FORMAT_R8G8B8A8_UNORM:
                return TImgFormat::RGBA8Unorm;
            default:
                return TImgFormat::B8G8R8A8Srgb;
        }
    }

    void TVkClusteredRenderer::createDepth()
    {
        const VkExtent2D extent = m_renderExtent;

        m_depth = TVkImage{};
        m_depth = TVkImage( m_gpu.device(), m_gpu.physDevice(),
                            { extent.width, extent.height, 1, 1, TImgFormat::D32Float, TImgUsage::DepthAttachment | TImgUsage::Sampled, TTexFilter::Nearest,
                              TTexAddr::Clamp,
                              /*sampled=*/true, /*shadow=*/false } );
    }

    void TVkClusteredRenderer::createHdr()
    {
        const VkExtent2D extent = m_renderExtent;

        const TVkImageDesc desc{ extent.width,       extent.height,   1,    1,    TImgFormat::RGBA16Float, TImgUsage::ColorAttachment | TImgUsage::Sampled,
                                 TTexFilter::Linear, TTexAddr::Clamp, true, false };
        m_hdrA = TVkImage{};
        m_hdrB = TVkImage{};
        m_hdrA = TVkImage( m_gpu.device(), m_gpu.physDevice(), desc );
        m_hdrB = TVkImage( m_gpu.device(), m_gpu.physDevice(), desc );
    }

    void TVkClusteredRenderer::createSceneColor()
    {
        const VkExtent2D   extent = m_renderExtent;
        const TVkImageDesc desc{
                extent.width,       extent.height,   1,    1,    imgFormatFromVk( m_gpu.swapFormat() ), TImgUsage::ColorAttachment | TImgUsage::Sampled,
                TTexFilter::Linear, TTexAddr::Clamp, true, false };
        m_sceneColor = TVkImage{};
        m_sceneColor = TVkImage( m_gpu.device(), m_gpu.physDevice(), desc );
        ++m_sceneColorGeneration;
    }

    void TVkClusteredRenderer::createShadowResources()
    {
        m_shadowMaps = TVkImage( m_gpu.device(), m_gpu.physDevice(),
                                 { g_kShadowMapSize, g_kShadowMapSize, g_kMaxShadowMaps, 1, TImgFormat::D32Float,
                                   TImgUsage::DepthAttachment | TImgUsage::Sampled, TTexFilter::Linear, TTexAddr::Clamp, /*sampled=*/true, /*shadow=*/true } );

        for ( uint32_t i = 0; i < g_kMaxShadowMaps; ++i )
        {
            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image                           = m_shadowMaps.handle();
            viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format                          = VK_FORMAT_D32_SFLOAT;
            viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
            viewInfo.subresourceRange.baseMipLevel   = 0;
            viewInfo.subresourceRange.levelCount     = 1;
            viewInfo.subresourceRange.baseArrayLayer = i;
            viewInfo.subresourceRange.layerCount     = 1;

            if ( vkCreateImageView( m_gpu.device(), &viewInfo, nullptr, &m_shadowLayerViews[ i ] ) != VK_SUCCESS )
                throw std::runtime_error( "[TVkClusteredRenderer] Failed to create shadow layer view" );
        }
    }

    void TVkClusteredRenderer::createLayouts()
    {
        auto makeLayout = [ & ]( const std::vector<VkDescriptorSetLayoutBinding>& p_bindings ) -> VkDescriptorSetLayout
        {
            VkDescriptorSetLayoutCreateInfo ci{};
            ci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            ci.bindingCount = static_cast<uint32_t>( p_bindings.size() );
            ci.pBindings    = p_bindings.data();
            VkDescriptorSetLayout layout{};
            if ( vkCreateDescriptorSetLayout( m_gpu.device(), &ci, nullptr, &layout ) != VK_SUCCESS )
                throw std::runtime_error( "[TVkClusteredRenderer] Failed to create descriptor set layout" );
            return layout;
        };

        m_sceneLayout = makeLayout( {
                { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
                { 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr },
                { 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
                { 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
                { 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
                { 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
                { 6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr },
        } );

        m_cullLayout = makeLayout( {
                { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
                { 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
                { 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
                { 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
                { 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
        } );

        m_spriteSetLayout = makeLayout( {
                { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr },
                { 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr },
        } );

        m_spriteTexLayout = makeLayout( {
                { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
        } );

        m_particleSimLayout = makeLayout( {
                { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
                { 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
                { 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
                { 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
                { 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
                { 5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
        } );

        m_particleDrawLayout = makeLayout( {
                { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr },
                { 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr },
                { 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr },
        } );
    }

    void TVkClusteredRenderer::createParticleResources()
    {
        const VkDevice         device     = m_gpu.device();
        const VkPhysicalDevice physDevice = m_gpu.physDevice();

        m_particleBuf      = TVkBuffer( device, physDevice, g_kMaxParticles * sizeof( TParticleData ), TBufUsage::Storage );
        m_freeListBuf      = TVkBuffer( device, physDevice, g_kMaxParticles * sizeof( uint32_t ), TBufUsage::Storage );
        m_drawIndexBuf     = TVkBuffer( device, physDevice, g_kMaxParticles * sizeof( uint32_t ), TBufUsage::Storage );
        m_particleCounters = TVkBuffer( device, physDevice, sizeof( TParticleCounters ), TBufUsage::Storage | TBufUsage::Indirect | TBufUsage::CopyDst );

        std::vector<uint32_t> freeList( g_kMaxParticles );
        for ( uint32_t i = 0; i < g_kMaxParticles; ++i ) freeList[ i ] = i;
        m_freeListBuf.upload( freeList.data(), 0, freeList.size() * sizeof( uint32_t ) );

        std::vector<TParticleData> zeros( g_kMaxParticles );
        m_particleBuf.upload( zeros.data(), 0, zeros.size() * sizeof( TParticleData ) );

        TParticleCounters counters{};
        counters.m_freeCount     = g_kMaxParticles;
        counters.m_aliveCount    = 0;
        counters.m_vertexCount   = 6;
        counters.m_instanceCount = 0;
        m_particleCounters.upload( &counters, 0, sizeof( counters ) );

        m_particlesInitialized = true;
    }

    void TVkClusteredRenderer::createFrameResources()
    {
        const VkDevice         device     = m_gpu.device();
        const VkPhysicalDevice physDevice = m_gpu.physDevice();

        m_frames.resize( g_kFramesInFlight );
        for ( uint32_t i = 0; i < g_kFramesInFlight; ++i )
        {
            TFrameResources& frame    = m_frames[ i ];
            TVkFrameData&    gpuFrame = m_gpu.frameData( i );

            frame.m_lightGrid      = TVkBuffer( device, physDevice, g_kClusterCount * sizeof( TLightCell ), TBufUsage::Storage );
            frame.m_lightIndices   = TVkBuffer( device, physDevice, g_kClusterCount * g_kMaxLightsPerCluster * sizeof( uint32_t ), TBufUsage::Storage );
            frame.m_counter        = TVkBuffer( device, physDevice, sizeof( uint32_t ), TBufUsage::Storage );
            frame.m_emitterBuf     = TVkBuffer( device, physDevice, g_kMaxEmitters * sizeof( TEmitterData ), TBufUsage::Storage );
            frame.m_particleSimUBO = TVkBuffer( device, physDevice, sizeof( TParticleSimUBO ), TBufUsage::Uniform );

            const VkDescriptorSetLayout layouts[] = { m_sceneLayout, m_cullLayout, m_spriteSetLayout, m_particleSimLayout, m_particleDrawLayout };
            VkDescriptorSet             sets[ 5 ]{};

            VkDescriptorSetAllocateInfo ai{};
            ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            ai.descriptorPool     = m_gpu.descPool();
            ai.descriptorSetCount = 5;
            ai.pSetLayouts        = layouts;
            if ( vkAllocateDescriptorSets( device, &ai, sets ) != VK_SUCCESS )
                throw std::runtime_error( "[TVkClusteredRenderer] Failed to allocate descriptor sets" );

            frame.m_sceneSet        = sets[ 0 ];
            frame.m_cullSet         = sets[ 1 ];
            frame.m_spriteSet       = sets[ 2 ];
            frame.m_particleSimSet  = sets[ 3 ];
            frame.m_particleDrawSet = sets[ 4 ];

            VkDescriptorBufferInfo sceneB{ gpuFrame.m_sceneUBO.handle(), 0, sizeof( TSceneUBO ) };
            VkDescriptorBufferInfo instB{ gpuFrame.m_instanceBuf.handle(), 0, gpuFrame.m_instanceBuf.size() };
            VkDescriptorBufferInfo lightB{ gpuFrame.m_lightBuf.handle(), 0, gpuFrame.m_lightBuf.size() };
            VkDescriptorBufferInfo gridB{ frame.m_lightGrid.handle(), 0, frame.m_lightGrid.size() };
            VkDescriptorBufferInfo idxB{ frame.m_lightIndices.handle(), 0, frame.m_lightIndices.size() };
            VkDescriptorBufferInfo cntB{ frame.m_counter.handle(), 0, frame.m_counter.size() };
            VkDescriptorBufferInfo sprB{ gpuFrame.m_spriteBuf.handle(), 0, gpuFrame.m_spriteBuf.size() };
            VkDescriptorBufferInfo boneB{ gpuFrame.m_boneBuf.handle(), 0, gpuFrame.m_boneBuf.size() };
            VkDescriptorImageInfo  shadowI{ m_shadowMaps.sampler(), m_shadowMaps.view(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };

            VkDescriptorBufferInfo simUboB{ frame.m_particleSimUBO.handle(), 0, sizeof( TParticleSimUBO ) };
            VkDescriptorBufferInfo partB{ m_particleBuf.handle(), 0, m_particleBuf.size() };
            VkDescriptorBufferInfo freeB{ m_freeListBuf.handle(), 0, m_freeListBuf.size() };
            VkDescriptorBufferInfo pcntB{ m_particleCounters.handle(), 0, m_particleCounters.size() };
            VkDescriptorBufferInfo drawB{ m_drawIndexBuf.handle(), 0, m_drawIndexBuf.size() };
            VkDescriptorBufferInfo emitB{ frame.m_emitterBuf.handle(), 0, frame.m_emitterBuf.size() };

            const std::array<VkWriteDescriptorSet, 23> writes{ {
                    { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, frame.m_sceneSet, 0, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, nullptr, &sceneB,
                      nullptr },
                    { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, frame.m_sceneSet, 1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &instB, nullptr },
                    { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, frame.m_sceneSet, 2, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &lightB,
                      nullptr },
                    { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, frame.m_sceneSet, 3, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &gridB, nullptr },
                    { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, frame.m_sceneSet, 4, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &idxB, nullptr },
                    { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, frame.m_sceneSet, 5, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &shadowI, nullptr,
                      nullptr },
                    { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, frame.m_sceneSet, 6, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &boneB, nullptr },
                    { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, frame.m_cullSet, 0, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, nullptr, &sceneB, nullptr },
                    { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, frame.m_cullSet, 1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &lightB, nullptr },
                    { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, frame.m_cullSet, 2, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &gridB, nullptr },
                    { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, frame.m_cullSet, 3, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &idxB, nullptr },
                    { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, frame.m_cullSet, 4, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &cntB, nullptr },
                    { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, frame.m_spriteSet, 0, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, nullptr, &sceneB,
                      nullptr },
                    { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, frame.m_spriteSet, 1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &sprB, nullptr },
                    { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, frame.m_particleSimSet, 0, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, nullptr, &simUboB,
                      nullptr },
                    { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, frame.m_particleSimSet, 1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &partB,
                      nullptr },
                    { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, frame.m_particleSimSet, 2, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &freeB,
                      nullptr },
                    { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, frame.m_particleSimSet, 3, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &pcntB,
                      nullptr },
                    { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, frame.m_particleSimSet, 4, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &drawB,
                      nullptr },
                    { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, frame.m_particleSimSet, 5, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &emitB,
                      nullptr },
                    { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, frame.m_particleDrawSet, 0, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, nullptr, &sceneB,
                      nullptr },
                    { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, frame.m_particleDrawSet, 1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &partB,
                      nullptr },
                    { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, frame.m_particleDrawSet, 2, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &drawB,
                      nullptr },
            } };
            vkUpdateDescriptorSets( device, static_cast<uint32_t>( writes.size() ), writes.data(), 0, nullptr );
        }
    }
}  // namespace Tomos
