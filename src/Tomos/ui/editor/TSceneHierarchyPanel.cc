#include "Tomos/ui/editor/TSceneHierarchyPanel.hh"

#include <cctype>
#include <imgui.h>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

#include "Tomos/core/app/TApplication.hh"
#include "Tomos/systems/TComponentRegistry.hh"
#include "Tomos/systems/physics/TColliderComponent.hh"
#include "Tomos/systems/script/TScriptComponent.hh"
#include "Tomos/systems/sprite/TSpriteComponent.hh"
#include "Tomos/ui/editor/TAssetBrowserPanel.hh"

namespace Tomos
{
    namespace
    {
        struct TPendingOps
        {
            struct TReparent
            {
                uint64_t m_nodeId   = 0;
                uint64_t m_parentId = 0;
            };

            std::vector<TReparent> m_reparents;
            uint64_t               m_addChildUnder = 0;
            uint64_t               m_duplicateId   = 0;
            uint64_t               m_deleteId      = 0;

            static constexpr uint64_t g_kSceneRoot = ~uint64_t{ 0 };
        };

        bool nameMatchesFilter( const std::string& p_name, const std::string& p_filter )
        {
            if ( p_filter.empty() ) return true;
            auto lower = []( std::string p_s )
            {
                for ( char& c : p_s ) c = static_cast<char>( std::tolower( static_cast<unsigned char>( c ) ) );
                return p_s;
            };
            return lower( p_name ).find( lower( p_filter ) ) != std::string::npos;
        }

        bool subtreeMatches( const TSceneNode& p_node, const std::string& p_filter )
        {
            if ( nameMatchesFilter( p_node.m_name, p_filter ) ) return true;
            for ( TSceneNode* child : p_node.getChildren() )
                if ( child != nullptr && subtreeMatches( *child, p_filter ) ) return true;
            return false;
        }

        std::string componentBadges( const TSceneNode& p_node )
        {
            std::string badges;
            auto&       reg = TComponentRegistry::get();
            for ( TComponent* c : p_node.getComponents() )
            {
                if ( c == nullptr ) continue;
                std::string t = reg.typeOf( *c );
                if ( t.empty() )
                {
                    if ( dynamic_cast<TScriptComponent*>( c ) )
                        t = "script";
                    else
                        continue;
                }
                char tag = static_cast<char>( std::toupper( static_cast<unsigned char>( t[ 0 ] ) ) );
                if ( t == "skinnedMesh" ) tag = 'K';
                if ( t == "rigidBody" ) tag = 'B';
                if ( t == "particleEmitter" ) tag = 'P';
                if ( !badges.empty() ) badges += ' ';
                badges += tag;

                if ( auto* col = dynamic_cast<TColliderComponent*>( c ) )
                    if ( !col->m_enabled ) badges += "!";
                if ( auto* spr = dynamic_cast<TSpriteComponent*>( c ) )
                    if ( !spr->m_visible ) badges += "~";
            }
            return badges;
        }

        void deepDuplicate( TScene& p_scene, TSceneResourceBag& p_bag, const TSceneNode& p_src, TSceneNode& p_dst,
                            std::unordered_map<uint64_t, TSceneNode*>& p_srcToDst, std::vector<TPendingSkinnedJoints>& p_pendingSkinned )
        {
            p_srcToDst[ p_src.m_id ] = &p_dst;

            auto&                assets = TApplication::get().assetSystem();
            TComponentResolveCtx ctx{ assets, p_bag, TApplication::get().gpu(), nullptr, &p_scene.store(), &p_pendingSkinned };

            for ( TComponent* c : p_src.getComponents() )
            {
                if ( c == nullptr ) continue;
                if ( const auto* sc = dynamic_cast<TScriptComponent*>( c ) )
                {
                    const std::string scriptType = sc->typeName();
                    if ( scriptType.empty() ) continue;
                    if ( auto loaded = TScriptComponent::makeFromType( scriptType ) ) p_dst.addComponent( std::move( loaded ) );
                    continue;
                }
                const auto json = TComponentRegistry::get().saveComponent( *c, ctx );
                if ( json.is_null() || json.empty() ) continue;
                if ( auto loaded = TComponentRegistry::get().loadComponent( json, ctx ) ) p_dst.addComponent( std::move( loaded ) );
            }

            for ( TSceneNode* child : p_src.getChildren() )
            {
                if ( child == nullptr ) continue;
                TSceneNode& dup = p_scene.createNode( child->m_name );
                dup.m_dynamic   = child->m_dynamic;
                dup.m_transform.setLocalTRS( child->m_transform.translation(), child->m_transform.rotation(), child->m_transform.scale() );
                deepDuplicate( p_scene, p_bag, *child, dup, p_srcToDst, p_pendingSkinned );
                p_dst.addChild( &dup );
            }
        }

        void drawNode( TSceneEditorContext& p_ctx, TSceneNode* p_node, TPendingOps& p_ops, const std::string& p_filter )
        {
            if ( p_node == nullptr || !subtreeMatches( *p_node, p_filter ) ) return;

            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
            if ( p_node->m_id == p_ctx.m_selectedId ) flags |= ImGuiTreeNodeFlags_Selected;

            const bool isLeaf = p_node->getChildren().empty();
            if ( isLeaf ) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

            const std::string badges = componentBadges( *p_node );
            const std::string label  = badges.empty() ? p_node->m_name : ( p_node->m_name + "  [" + badges + "]" );

            const bool open        = ImGui::TreeNodeEx( reinterpret_cast<void*>( static_cast<uintptr_t>( p_node->m_id ) ), flags, "%s", label.c_str() );
            const bool needTreePop = open && !isLeaf;

            if ( ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen() ) p_ctx.select( p_node->m_id );

            if ( ImGui::BeginDragDropSource() )
            {
                const uint64_t id = p_node->m_id;
                ImGui::SetDragDropPayload( "TOMOS_NODE", &id, sizeof( id ) );
                ImGui::Text( "%s", p_node->m_name.c_str() );
                ImGui::EndDragDropSource();
            }

            if ( ImGui::BeginDragDropTarget() )
            {
                if ( const ImGuiPayload* payload = ImGui::AcceptDragDropPayload( "TOMOS_NODE" ) )
                {
                    const uint64_t draggedId = *static_cast<const uint64_t*>( payload->Data );
                    if ( draggedId != p_node->m_id ) p_ops.m_reparents.push_back( { draggedId, p_node->m_id } );
                }
                if ( const ImGuiPayload* payload = ImGui::AcceptDragDropPayload( TAssetBrowserPanel::g_kPayloadMesh ) )
                    TAssetBrowserPanel::applyDrop( p_ctx, *p_node, *payload );
                if ( const ImGuiPayload* payload = ImGui::AcceptDragDropPayload( TAssetBrowserPanel::g_kPayloadGpuAsset ) )
                    TAssetBrowserPanel::applyDrop( p_ctx, *p_node, *payload );
                if ( const ImGuiPayload* payload = ImGui::AcceptDragDropPayload( TAssetBrowserPanel::g_kPayloadAudio ) )
                    TAssetBrowserPanel::applyDrop( p_ctx, *p_node, *payload );
                if ( const ImGuiPayload* payload = ImGui::AcceptDragDropPayload( TAssetBrowserPanel::g_kPayloadTexture ) )
                    TAssetBrowserPanel::applyDrop( p_ctx, *p_node, *payload );
                ImGui::EndDragDropTarget();
            }

            if ( ImGui::BeginPopupContextItem() )
            {
                p_ctx.select( p_node->m_id );
                if ( ImGui::MenuItem( "Add Child" ) ) p_ops.m_addChildUnder = p_node->m_id;
                if ( ImGui::MenuItem( "Duplicate" ) ) p_ops.m_duplicateId = p_node->m_id;
                if ( ImGui::MenuItem( "Delete" ) ) p_ops.m_deleteId = p_node->m_id;
                ImGui::EndPopup();
            }

            if ( needTreePop )
            {
                for ( TSceneNode* child : p_node->getChildren() ) drawNode( p_ctx, child, p_ops, p_filter );
                ImGui::TreePop();
            }
        }

        void applyPending( TSceneEditorContext& p_ctx, TPendingOps& p_ops )
        {
            if ( p_ctx.m_scene == nullptr ) return;

            for ( const auto& op : p_ops.m_reparents )
            {
                TSceneNode* dragged = p_ctx.m_scene->findById( op.m_nodeId );
                if ( dragged == nullptr ) continue;

                if ( op.m_parentId == 0 )
                {
                    if ( TSceneNode* old = dragged->getParent() )
                        old->removeChild( dragged );
                    else
                        p_ctx.m_scene->removeChild( dragged );
                    p_ctx.m_scene->addChild( dragged );
                }
                else if ( TSceneNode* parent = p_ctx.m_scene->findById( op.m_parentId ) )
                {
                    dragged->reparent( parent );
                }
            }

            if ( p_ops.m_addChildUnder != 0 )
            {
                TSceneNode* parent = p_ops.m_addChildUnder == TPendingOps::g_kSceneRoot ? p_ctx.m_scene : p_ctx.m_scene->findById( p_ops.m_addChildUnder );
                if ( parent != nullptr )
                {
                    TSceneNode& child = p_ctx.m_scene->createNode( "New Node" );
                    parent->addChild( &child );
                    p_ctx.select( child.m_id );
                }
            }

            if ( p_ops.m_duplicateId != 0 )
            {
                if ( TSceneNode* src = p_ctx.m_scene->findById( p_ops.m_duplicateId ) )
                {
                    TSceneNode& dup = p_ctx.m_scene->createNode( src->m_name + " Copy" );
                    dup.m_dynamic   = src->m_dynamic;
                    dup.m_transform.setLocalTRS( src->m_transform.translation(), src->m_transform.rotation(), src->m_transform.scale() );
                    if ( p_ctx.m_bag != nullptr )
                    {
                        std::unordered_map<uint64_t, TSceneNode*> srcToDst;
                        std::vector<TPendingSkinnedJoints>        pendingSkinned;
                        deepDuplicate( *p_ctx.m_scene, *p_ctx.m_bag, *src, dup, srcToDst, pendingSkinned );
                        TComponentRegistry::wireSkinnedJoints( pendingSkinned, srcToDst );
                    }
                    if ( TSceneNode* parent = src->getParent() )
                        parent->addChild( &dup );
                    else
                        p_ctx.m_scene->addChild( &dup );
                    p_ctx.select( dup.m_id );
                }
            }

            if ( p_ops.m_deleteId != 0 )
            {
                if ( TSceneNode* node = p_ctx.m_scene->findById( p_ops.m_deleteId ) )
                {
                    if ( node->handle().valid() )
                        p_ctx.m_scene->store().destroyNode( node->handle() );
                    else if ( TSceneNode* parent = node->getParent() )
                        parent->removeChild( node );
                    else
                        p_ctx.m_scene->removeChild( node );
                    if ( p_ctx.m_selectedId == p_ops.m_deleteId ) p_ctx.clearSelection();
                }
            }
        }
    }  // namespace

    void TSceneHierarchyPanel::draw( TSceneEditorContext& p_ctx )
    {
        ImGui::Begin( "Hierarchy" );
        if ( p_ctx.m_scene == nullptr )
        {
            ImGui::TextDisabled( "No scene" );
            ImGui::End();
            return;
        }

        static char filterBuf[ 128 ] = {};
        ImGui::SetNextItemWidth( -1 );
        ImGui::InputTextWithHint( "##filter", "Filter…", filterBuf, sizeof( filterBuf ) );
        const std::string filter = filterBuf;

        TPendingOps ops;

        if ( ImGui::Button( "Add Root Node" ) ) ops.m_addChildUnder = TPendingOps::g_kSceneRoot;

        ImGui::Separator();

        ImGui::TextDisabled( "Scene root (drop here to unparent)" );
        if ( ImGui::BeginDragDropTarget() )
        {
            if ( const ImGuiPayload* payload = ImGui::AcceptDragDropPayload( "TOMOS_NODE" ) )
            {
                const uint64_t draggedId = *static_cast<const uint64_t*>( payload->Data );
                ops.m_reparents.push_back( { draggedId, 0 } );
            }
            ImGui::EndDragDropTarget();
        }

        for ( TSceneNode* child : p_ctx.m_scene->getChildren() ) drawNode( p_ctx, child, ops, filter );

        ImGui::End();

        applyPending( p_ctx, ops );
    }
}  // namespace Tomos
