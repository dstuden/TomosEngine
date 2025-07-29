#pragma once
#include "FrameBuffer.hh"
#include "Shader.hh"
#include "Tomos/util/renderer/Material.hh"
#include "VertexArray.hh"

namespace Tomos
{
    struct DrawCall
    {
        std::shared_ptr<Shader>      m_shader;
        std::shared_ptr<Material>    m_material;
        std::shared_ptr<VertexArray> m_vertexArray;
        glm::mat4                    m_transform;
    };

    struct BatchData
    {
        std::shared_ptr<VertexArray> m_vertexArray;
        std::vector<glm::mat4>       m_transforms;
    };

    struct InstanceData
    {
        glm::mat4 m_transform;
    };

    struct MaterialBatchKey
    {
        std::shared_ptr<Shader>   m_shader;
        std::shared_ptr<Material> m_material;

        bool operator==( const MaterialBatchKey& other ) const { return m_shader == other.m_shader && m_material == other.m_material; }
    };

    struct MaterialBatchKeyHash
    {
        size_t operator()( const MaterialBatchKey& key ) const
        {
            return std::hash<std::shared_ptr<Shader>>()( key.m_shader ) ^ std::hash<std::shared_ptr<Material>>()( key.m_material );
        }
    };

    class Renderer
    {
    public:
        static void setClearedColor( const glm::vec4& p_color );
        static void clear( unsigned int p_mask );
        static void onWindowResize( unsigned int p_width, unsigned int p_height );

        // Individual draw call
        static void draw( const std::shared_ptr<Shader>& p_shader, const std::shared_ptr<Material>& p_material,
                          const std::shared_ptr<VertexArray>& p_vertexArray, const glm::mat4& p_transform, const glm::mat4& p_viewProjection );

        // Instanced draw call
        static void drawInstanced( const std::shared_ptr<Shader>& p_shader, const std::shared_ptr<Material>& p_material,
                                   const std::shared_ptr<VertexArray>& p_vertexArray, const std::vector<InstanceData>& p_instances,
                                   const glm::mat4& p_viewProjection, bool p_batched = false );

        // Batch rendering interface
        static void beginBatch();
        static void endBatch( const glm::mat4& p_viewProjection );
        static void addToBatch( const std::shared_ptr<Shader>& p_shader, const std::shared_ptr<Material>& p_material,
                                const std::shared_ptr<VertexArray>& p_vertexArray, const glm::mat4& p_transform );

        static void clearFrameBuffer();
        static void renderLayerFrameBufferToQuad( const std::shared_ptr<LayerFrameBuffer>& p_framebuffer, const std::shared_ptr<VertexArray>& p_quad,
                                                  const std::shared_ptr<Shader>& p_postProcessShader = nullptr );

    private:
        static std::unordered_map<MaterialBatchKey, std::unordered_map<std::shared_ptr<VertexArray>, BatchData>, MaterialBatchKeyHash> g_batches;
        static bool                                                                                                                    g_batching;
        static std::unordered_map<std::shared_ptr<VertexArray>, std::shared_ptr<StorageBuffer>>                                        g_instanceBuffers;
    };
}  // namespace Tomos
