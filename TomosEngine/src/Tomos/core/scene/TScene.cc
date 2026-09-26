#include "Tomos/core/scene/TScene.hh"

#include <vector>

#include "Tomos/util/memory/TArenaAllocator.hh"
#include "Tomos/util/memory/TFrameAllocator.hh"
#include "Tomos/util/profile/TProfile.hh"

namespace Tomos
{
    TScene::TScene() { m_store = &m_levelStore; }

    TScene::TScene( std::string p_name ) : TSceneNode( std::move( p_name ) ) { m_store = &m_levelStore; }

    TScene::~TScene()
    {
        clearLevel();
        m_store = nullptr;
    }

    TSceneNode& TScene::createNode( std::string p_name )
    {
        TNodeHandle h = m_levelStore.createNode( std::move( p_name ) );
        return *m_levelStore.getNode( h );
    }

    void TScene::activate()
    {
        if ( m_active ) return;
        m_active = true;
        m_store  = &m_levelStore;
        attachComponents( m_ecsInstance );
    }

    void TScene::deactivate()
    {
        detachComponents();
        m_active = false;
    }

    void TScene::clearLevel()
    {
        if ( m_active ) deactivate();
        clearChildren();
        // Drop component list before the store frees them.
        m_components.clear();
        m_levelStore.clear();
    }

    void TScene::computeTransforms()
    {
        TOMOS_PROFILE_SCOPE( "Scene.Transforms" );
        TOMOS_HEAP_PROBE( "computeTransforms" );

        static const TTransform sIdentity;
        bool                    rootDirty = m_transform.updateGlobal( sIdentity, false );

        auto walk = [ & ]( auto& stack )
        {
            auto pushChildren = [ & ]( TSceneNode* node, const TTransform* parentXf, bool dirty )
            {
                for ( TSceneNode* child : node->getChildren() ) stack.push_back( { child, parentXf, dirty } );
            };

            pushChildren( this, &m_transform, rootDirty );
            while ( !stack.empty() )
            {
                auto [ node, parentTransform, parentDirty ] = stack.back();
                stack.pop_back();
                bool dirty = node->m_transform.updateGlobal( *parentTransform, parentDirty );
                pushChildren( node, &node->m_transform, dirty );
            }
        };

        // Frame arena inside a tick; heap fallback for tooling/tests.
        if ( TFrameAllocator* fa = TFrameAllocator::current() )
        {
            TArenaVector<TTransformTask> stack( TArenaAllocator<TTransformTask>( fa->arena() ) );
            walk( stack );
            return;
        }

        std::vector<TTransformTask> stack;
        walk( stack );
    }
}  // namespace Tomos
