#include "Tomos/systems/asset/TAssetLoadQueue.hh"

#include <chrono>
#include <utility>

#include "Tomos/core/scene/TScene.hh"
#include "Tomos/gpu/vulkan/TVkGpu.hh"
#include "Tomos/systems/animation/TAnimatorComponent.hh"
#include "Tomos/systems/mesh/TMeshComponent.hh"
#include "Tomos/util/logger/TLogger.hh"

namespace Tomos
{
    const std::string TAssetLoadQueue::g_sEmptyError{};

    TAssetLoadQueue::TAssetLoadQueue( TAssetSystem& p_assets ) : m_assets( p_assets )
    {
        m_worker = std::thread( [ this ] { workerLoop(); } );
    }

    TAssetLoadQueue::~TAssetLoadQueue() { shutdown(); }

    void TAssetLoadQueue::shutdown()
    {
        {
            std::lock_guard lock( m_mutex );
            if ( m_stop ) return;
            m_stop = true;
        }
        m_cv.notify_all();
        if ( m_worker.joinable() ) m_worker.join();
    }

    TAssetLoadHandle TAssetLoadQueue::requestLoad( const std::string& p_path, const std::string& p_preferredName )
    {
        const std::string stable = TAssetSystem::makeStableId( p_path );

        // Already registered — synthesize a Ready handle.
        if ( TGpuAsset* existing = m_assets.findByPath( p_path ) )
        {
            std::lock_guard lock( m_mutex );
            if ( const auto it = m_byStableId.find( stable ); it != m_byStableId.end() )
            {
                TEntry& e = m_entries[ it->second ];
                e.m_status = TAssetLoadStatus::Ready;
                e.m_asset  = existing;
                return TAssetLoadHandle{ e.m_id };
            }

            const uint64_t id = m_nextId++;
            TEntry         e{};
            e.m_id            = id;
            e.m_path          = p_path;
            e.m_preferredName = p_preferredName;
            e.m_stableId      = stable;
            e.m_status        = TAssetLoadStatus::Ready;
            e.m_asset         = existing;
            m_entries.emplace( id, std::move( e ) );
            m_byStableId.emplace( stable, id );
            return TAssetLoadHandle{ id };
        }

        std::lock_guard lock( m_mutex );
        if ( const auto it = m_byStableId.find( stable ); it != m_byStableId.end() )
        {
            TEntry& e = m_entries[ it->second ];
            if ( !p_preferredName.empty() && e.m_preferredName.empty() ) e.m_preferredName = p_preferredName;
            return TAssetLoadHandle{ e.m_id };
        }

        const uint64_t id = m_nextId++;
        TEntry         e{};
        e.m_id            = id;
        e.m_path          = p_path;
        e.m_preferredName = p_preferredName;
        e.m_stableId      = stable;
        e.m_status        = TAssetLoadStatus::Pending;
        m_entries.emplace( id, std::move( e ) );
        m_byStableId.emplace( stable, id );
        m_decodeQueue.push_back( id );
        m_cv.notify_one();
        return TAssetLoadHandle{ id };
    }

    TAssetLoadStatus TAssetLoadQueue::status( TAssetLoadHandle p_handle ) const
    {
        std::lock_guard lock( m_mutex );
        const auto      it = m_entries.find( p_handle.m_id );
        if ( it == m_entries.end() ) return TAssetLoadStatus::Failed;
        return it->second.m_status;
    }

    bool TAssetLoadQueue::isReady( TAssetLoadHandle p_handle ) const { return status( p_handle ) == TAssetLoadStatus::Ready; }

    bool TAssetLoadQueue::isFailed( TAssetLoadHandle p_handle ) const { return status( p_handle ) == TAssetLoadStatus::Failed; }

    TGpuAsset* TAssetLoadQueue::asset( TAssetLoadHandle p_handle )
    {
        std::lock_guard lock( m_mutex );
        const auto      it = m_entries.find( p_handle.m_id );
        if ( it == m_entries.end() || it->second.m_status != TAssetLoadStatus::Ready ) return nullptr;
        return it->second.m_asset;
    }

    const std::string& TAssetLoadQueue::error( TAssetLoadHandle p_handle ) const
    {
        std::lock_guard lock( m_mutex );
        const auto      it = m_entries.find( p_handle.m_id );
        if ( it == m_entries.end() ) return g_sEmptyError;
        return it->second.m_error;
    }

    TSceneNode* TAssetLoadQueue::takeRoot( TAssetLoadHandle p_handle, TScene& p_scene )
    {
        TNodeHandle        h;
        const TLevelStore* store = nullptr;
        {
            std::lock_guard lock( m_mutex );
            const auto      it = m_entries.find( p_handle.m_id );
            if ( it == m_entries.end() ) return nullptr;
            h                       = it->second.m_root;
            store                   = it->second.m_rootStore;
            it->second.m_root       = {};
            it->second.m_rootStore  = nullptr;
        }
        if ( !h.valid() || store != &p_scene.store() ) return nullptr;
        return p_scene.store().getNode( h );
    }

    void TAssetLoadQueue::onComplete( TAssetLoadHandle p_handle, TCompleteFn p_fn )
    {
        TAssetLoadStatus st     = TAssetLoadStatus::Pending;
        bool             runNow = false;
        {
            std::lock_guard lock( m_mutex );
            const auto      it = m_entries.find( p_handle.m_id );
            if ( it == m_entries.end() ) return;
            if ( it->second.m_status == TAssetLoadStatus::Ready || it->second.m_status == TAssetLoadStatus::Failed )
            {
                st     = it->second.m_status;
                runNow = true;
            }
            else
            {
                it->second.m_callbacks.push_back( std::move( p_fn ) );
                return;
            }
        }
        if ( runNow ) p_fn( st );
    }

    std::vector<TAssetLoadQueue::TCompleteFn> TAssetLoadQueue::takeCallbacks( TEntry& p_entry, TAssetLoadStatus p_status )
    {
        p_entry.m_status             = p_status;
        std::vector<TCompleteFn> cbs = std::move( p_entry.m_callbacks );
        p_entry.m_callbacks.clear();
        return cbs;
    }

    static void runLoadCallbacks( std::vector<std::function<void( TAssetLoadStatus )>>& p_cbs, TAssetLoadStatus p_status )
    {
        for ( auto& fn : p_cbs )
        {
            try
            {
                fn( p_status );
            }
            catch ( const std::exception& e )
            {
                TLOG_ERROR() << "[TAssetLoadQueue] onComplete threw: " << e.what();
            }
        }
        p_cbs.clear();
    }

    void TAssetLoadQueue::rebindScene( TScene& p_scene ) const
    {
        std::vector<TSceneNode*> stack;
        stack.push_back( &p_scene );
        while ( !stack.empty() )
        {
            TSceneNode* node = stack.back();
            stack.pop_back();
            for ( TSceneNode* child : node->getChildren() )
                if ( child ) stack.push_back( child );

            for ( TComponent* comp : node->getComponents() )
            {
                if ( auto* mesh = dynamic_cast<TMeshComponent*>( comp ) ) mesh->rebind( m_assets );
                if ( auto* anim = dynamic_cast<TAnimatorComponent*>( comp ) ) anim->rebind( m_assets );
            }
        }
    }

    void TAssetLoadQueue::tick( TScene& p_scene )
    {
        if ( m_gpu == nullptr ) return;

        std::vector<TDecodedJob> jobs;
        {
            std::lock_guard lock( m_mutex );
            jobs.swap( m_decoded );
            for ( auto& job : jobs )
            {
                auto it = m_entries.find( job.m_id );
                if ( it != m_entries.end() && it->second.m_status == TAssetLoadStatus::Pending ) it->second.m_status = TAssetLoadStatus::Uploading;
            }
        }

        bool registeredAny = false;
        for ( TDecodedJob& job : jobs )
        {
            std::string              preferredName;
            std::string              path;
            std::vector<TCompleteFn> cbs;
            TAssetLoadStatus         doneStatus = TAssetLoadStatus::Failed;

            {
                std::lock_guard lock( m_mutex );
                auto            it = m_entries.find( job.m_id );
                if ( it == m_entries.end() ) continue;
                preferredName = it->second.m_preferredName;
                path          = it->second.m_path;
            }

            if ( !job.m_package.m_ok )
            {
                {
                    std::lock_guard lock( m_mutex );
                    auto            it = m_entries.find( job.m_id );
                    if ( it == m_entries.end() ) continue;
                    it->second.m_error = job.m_package.m_error.empty() ? "decode failed" : job.m_package.m_error;
                    TLOG_ERROR() << "[TAssetLoadQueue] Decode failed '" << path << "': " << it->second.m_error;
                    cbs        = takeCallbacks( it->second, TAssetLoadStatus::Failed );
                    doneStatus = TAssetLoadStatus::Failed;
                }
                runLoadCallbacks( cbs, doneStatus );
                continue;
            }

            try
            {
                if ( !preferredName.empty() ) job.m_package.m_name = preferredName;

                TLoadResult result = TGltfLoader::uploadGpu( std::move( job.m_package ), *m_gpu, p_scene.store() );
                if ( result.m_asset == nullptr )
                {
                    {
                        std::lock_guard lock( m_mutex );
                        auto            it = m_entries.find( job.m_id );
                        if ( it == m_entries.end() ) continue;
                        it->second.m_error = "upload produced null asset";
                        cbs                = takeCallbacks( it->second, TAssetLoadStatus::Failed );
                        doneStatus         = TAssetLoadStatus::Failed;
                    }
                    runLoadCallbacks( cbs, doneStatus );
                    continue;
                }

                TGpuAsset*        raw        = result.m_asset.get();
                const TNodeHandle rootHandle = result.m_root != nullptr ? result.m_root->handle() : TNodeHandle{};
                m_assets.registerAsset( std::move( result.m_asset ) );
                registeredAny = true;

                {
                    std::lock_guard lock( m_mutex );
                    auto            it = m_entries.find( job.m_id );
                    if ( it == m_entries.end() ) continue;
                    it->second.m_asset     = raw;
                    it->second.m_root      = rootHandle;
                    it->second.m_rootStore = &p_scene.store();
                    cbs                    = takeCallbacks( it->second, TAssetLoadStatus::Ready );
                    doneStatus             = TAssetLoadStatus::Ready;
                }
                runLoadCallbacks( cbs, doneStatus );
            }
            catch ( const std::exception& e )
            {
                {
                    std::lock_guard lock( m_mutex );
                    auto            it = m_entries.find( job.m_id );
                    if ( it == m_entries.end() ) continue;
                    it->second.m_error = e.what();
                    TLOG_ERROR() << "[TAssetLoadQueue] Upload failed '" << path << "': " << it->second.m_error;
                    cbs        = takeCallbacks( it->second, TAssetLoadStatus::Failed );
                    doneStatus = TAssetLoadStatus::Failed;
                }
                runLoadCallbacks( cbs, doneStatus );
            }
        }

        if ( registeredAny ) rebindScene( p_scene );
    }

    void TAssetLoadQueue::waitUntilReady( TAssetLoadHandle p_handle, TScene& p_scene )
    {
        waitUntil( [ this, p_handle ] {
            const TAssetLoadStatus st = status( p_handle );
            return st == TAssetLoadStatus::Ready || st == TAssetLoadStatus::Failed;
        }, p_scene );
    }

    void TAssetLoadQueue::waitUntil( const std::function<bool()>& p_pred, TScene& p_scene )
    {
        while ( !p_pred() )
        {
            tick( p_scene );
            if ( p_pred() ) break;
            std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
        }
        // Final tick in case Ready landed between pred check and return.
        tick( p_scene );
    }

    void TAssetLoadQueue::workerLoop()
    {
        while ( true )
        {
            uint64_t id = 0;
            std::string path;
            {
                std::unique_lock lock( m_mutex );
                m_cv.wait( lock, [ this ] { return m_stop || !m_decodeQueue.empty(); } );
                if ( m_stop && m_decodeQueue.empty() ) return;
                if ( m_decodeQueue.empty() ) continue;
                id = m_decodeQueue.front();
                m_decodeQueue.erase( m_decodeQueue.begin() );
                const auto it = m_entries.find( id );
                if ( it == m_entries.end() ) continue;
                path = it->second.m_path;
            }

            TCpuGltfPackage pkg = TGltfLoader::decodeCpu( path );

            {
                std::lock_guard lock( m_mutex );
                if ( m_stop ) return;
                m_decoded.push_back( TDecodedJob{ id, std::move( pkg ) } );
            }
        }
    }
}  // namespace Tomos
