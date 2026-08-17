#include "Tomos/core/scene/TScene.hh"

#include <vector>

namespace Tomos
{
    TScene::TScene( std::string p_name ) : TSceneNode( std::move( p_name ) ) {}

    TScene::~TScene()
    {
        if ( m_active ) deactivate();
    }

    void TScene::activate()
    {
        if ( m_active ) return;  // idempotent — avoid double script/audio attach
        m_active = true;
        attachComponents( m_ecsInstance );
    }

    void TScene::deactivate()
    {
        detachComponents();
        m_active = false;
    }

    void TScene::computeTransforms()
    {
        // Identity transform acts as the root parent — global matrix starts as I,
        // so the scene root's global matrix equals its local matrix.
        static const TTransform sIdentity;
        bool                    rootDirty = m_transform.updateGlobal( sIdentity, false );

        std::vector<TTransformTask> stack;
        for ( auto& child : getChildren() ) stack.push_back( { child.get(), &m_transform, rootDirty } );

        while ( !stack.empty() )
        {
            auto [ node, parentTransform, parentDirty ] = stack.back();
            stack.pop_back();
            bool dirty = node->m_transform.updateGlobal( *parentTransform, parentDirty );
            for ( auto& child : node->getChildren() ) stack.push_back( { child.get(), &node->m_transform, dirty } );
        }
    }
}  // namespace Tomos
