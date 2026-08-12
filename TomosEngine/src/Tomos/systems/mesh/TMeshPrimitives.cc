#include "Tomos/systems/mesh/TMeshPrimitives.hh"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <glm/gtc/constants.hpp>
#include <limits>
#include <vector>

#include "Tomos/gpu/vulkan/TVkBuffer.hh"
#include "Tomos/gpu/vulkan/TVkGpu.hh"

namespace Tomos::TMeshPrimitives
{
    namespace
    {
        struct TVertex
        {
            glm::vec3 m_pos;
            glm::vec3 m_nor;
            glm::vec2 m_uv;
        };

        void uploadMesh( TVkGpu& p_gpu, TVkMesh& p_mesh, const std::vector<TVertex>& p_verts, const std::vector<uint16_t>& p_indices )
        {
            glm::vec3 bmin( std::numeric_limits<float>::max() );
            glm::vec3 bmax( std::numeric_limits<float>::lowest() );

            std::vector<glm::vec3> pos( p_verts.size() );
            std::vector<glm::vec3> nor( p_verts.size() );
            std::vector<glm::vec2> uv( p_verts.size() );
            for ( size_t i = 0; i < p_verts.size(); ++i )
            {
                pos[ i ] = p_verts[ i ].m_pos;
                nor[ i ] = p_verts[ i ].m_nor;
                uv[ i ]  = p_verts[ i ].m_uv;
                bmin     = glm::min( bmin, pos[ i ] );
                bmax     = glm::max( bmax, pos[ i ] );
            }
            p_mesh.m_aabb.m_min = bmin;
            p_mesh.m_aabb.m_max = bmax;

            {
                const size_t sz   = pos.size() * sizeof( glm::vec3 );
                p_mesh.m_position = std::make_unique<TVkBuffer>( p_gpu.device(), p_gpu.physDevice(), sz, TBufUsage::Vertex | TBufUsage::CopyDst );
                p_gpu.uploadBuffer( *p_mesh.m_position, pos.data(), sz );
            }
            {
                const size_t sz = nor.size() * sizeof( glm::vec3 );
                p_mesh.m_normal = std::make_unique<TVkBuffer>( p_gpu.device(), p_gpu.physDevice(), sz, TBufUsage::Vertex | TBufUsage::CopyDst );
                p_gpu.uploadBuffer( *p_mesh.m_normal, nor.data(), sz );
            }
            {
                const size_t sz   = uv.size() * sizeof( glm::vec2 );
                p_mesh.m_texCoord = std::make_unique<TVkBuffer>( p_gpu.device(), p_gpu.physDevice(), sz, TBufUsage::Vertex | TBufUsage::CopyDst );
                p_gpu.uploadBuffer( *p_mesh.m_texCoord, uv.data(), sz );
            }
            {
                // Flat tangent fallback — forward pipeline always binds a tangent stream.
                std::vector<glm::vec4> tan( p_verts.size(), glm::vec4( 1.0f, 0.0f, 0.0f, 1.0f ) );
                const size_t           sz = tan.size() * sizeof( glm::vec4 );
                p_mesh.m_tangent          = std::make_unique<TVkBuffer>( p_gpu.device(), p_gpu.physDevice(), sz, TBufUsage::Vertex | TBufUsage::CopyDst );
                p_gpu.uploadBuffer( *p_mesh.m_tangent, tan.data(), sz );
            }

            p_mesh.m_is16BitIndex = true;
            p_mesh.m_drawCount    = static_cast<uint32_t>( p_indices.size() );
            const size_t idxSz    = p_indices.size() * sizeof( uint16_t );
            p_mesh.m_index        = std::make_unique<TVkBuffer>( p_gpu.device(), p_gpu.physDevice(), idxSz, TBufUsage::Index | TBufUsage::CopyDst );
            p_gpu.uploadBuffer( *p_mesh.m_index, p_indices.data(), idxSz );
        }

        void addQuad( std::vector<TVertex>& p_verts, std::vector<uint16_t>& p_indices, const glm::vec3& p_a, const glm::vec3& p_b, const glm::vec3& p_c,
                      const glm::vec3& p_d, const glm::vec3& p_n )
        {
            const auto base = static_cast<uint16_t>( p_verts.size() );
            p_verts.push_back( { p_a, p_n, { 0, 0 } } );
            p_verts.push_back( { p_b, p_n, { 1, 0 } } );
            p_verts.push_back( { p_c, p_n, { 1, 1 } } );
            p_verts.push_back( { p_d, p_n, { 0, 1 } } );
            p_indices.push_back( base );
            p_indices.push_back( static_cast<uint16_t>( base + 1 ) );
            p_indices.push_back( static_cast<uint16_t>( base + 2 ) );
            p_indices.push_back( base );
            p_indices.push_back( static_cast<uint16_t>( base + 2 ) );
            p_indices.push_back( static_cast<uint16_t>( base + 3 ) );
        }
    }  // namespace

    std::unique_ptr<TVkMesh> makeBox( TVkGpu& p_gpu )
    {
        constexpr float       h = 0.5f;
        std::vector<TVertex>  verts;
        std::vector<uint16_t> indices;
        verts.reserve( 24 );
        indices.reserve( 36 );

        addQuad( verts, indices, { -h, -h, h }, { h, -h, h }, { h, h, h }, { -h, h, h }, { 0, 0, 1 } );
        addQuad( verts, indices, { h, -h, -h }, { -h, -h, -h }, { -h, h, -h }, { h, h, -h }, { 0, 0, -1 } );
        addQuad( verts, indices, { -h, -h, -h }, { -h, -h, h }, { -h, h, h }, { -h, h, -h }, { -1, 0, 0 } );
        addQuad( verts, indices, { h, -h, h }, { h, -h, -h }, { h, h, -h }, { h, h, h }, { 1, 0, 0 } );
        addQuad( verts, indices, { -h, h, h }, { h, h, h }, { h, h, -h }, { -h, h, -h }, { 0, 1, 0 } );
        addQuad( verts, indices, { -h, -h, -h }, { h, -h, -h }, { h, -h, h }, { -h, -h, h }, { 0, -1, 0 } );

        auto mesh = std::make_unique<TVkMesh>();
        uploadMesh( p_gpu, *mesh, verts, indices );
        return mesh;
    }

    std::unique_ptr<TVkMesh> makePlane( TVkGpu& p_gpu )
    {
        constexpr float       h = 0.5f;
        std::vector<TVertex>  verts;
        std::vector<uint16_t> indices;
        addQuad( verts, indices, { -h, 0, h }, { h, 0, h }, { h, 0, -h }, { -h, 0, -h }, { 0, 1, 0 } );

        auto mesh = std::make_unique<TVkMesh>();
        uploadMesh( p_gpu, *mesh, verts, indices );
        return mesh;
    }

    std::unique_ptr<TVkMesh> makeUVSphere( TVkGpu& p_gpu, int p_segments, int p_rings )
    {
        const int       segments = std::max( p_segments, 3 );
        const int       rings    = std::max( p_rings, 2 );
        constexpr float radius   = 0.5f;

        std::vector<TVertex>  verts;
        std::vector<uint16_t> indices;
        verts.reserve( static_cast<size_t>( ( rings + 1 ) * ( segments + 1 ) ) );

        for ( int y = 0; y <= rings; ++y )
        {
            const float v    = static_cast<float>( y ) / static_cast<float>( rings );
            const float phi  = v * glm::pi<float>();
            const float sinP = std::sin( phi );
            const float cosP = std::cos( phi );
            for ( int x = 0; x <= segments; ++x )
            {
                const float     u     = static_cast<float>( x ) / static_cast<float>( segments );
                const float     theta = u * glm::two_pi<float>();
                const glm::vec3 n{ std::sin( theta ) * sinP, cosP, std::cos( theta ) * sinP };
                verts.push_back( { n * radius, n, { u, v } } );
            }
        }

        for ( int y = 0; y < rings; ++y )
        {
            for ( int x = 0; x < segments; ++x )
            {
                const auto i0 = static_cast<uint16_t>( y * ( segments + 1 ) + x );
                const auto i1 = static_cast<uint16_t>( i0 + 1 );
                const auto i2 = static_cast<uint16_t>( i0 + segments + 1 );
                const auto i3 = static_cast<uint16_t>( i2 + 1 );
                indices.push_back( i0 );
                indices.push_back( i2 );
                indices.push_back( i1 );
                indices.push_back( i1 );
                indices.push_back( i2 );
                indices.push_back( i3 );
            }
        }

        auto mesh = std::make_unique<TVkMesh>();
        uploadMesh( p_gpu, *mesh, verts, indices );
        return mesh;
    }
}  // namespace Tomos::TMeshPrimitives
