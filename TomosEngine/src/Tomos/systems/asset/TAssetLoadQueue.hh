#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "Tomos/systems/asset/TAssetSystem.hh"
#include "Tomos/systems/asset/TGltfLoader.hh"

namespace Tomos
{
    class TVkGpu;
    class TScene;

    enum class TAssetLoadStatus : uint8_t
    {
        Pending   = 0,
        Uploading = 1,
        Ready     = 2,
        Failed    = 3,
    };

    struct TAssetLoadHandle
    {
        uint64_t m_id = 0;

        [[nodiscard]] bool valid() const { return m_id != 0; }
        friend bool        operator==( const TAssetLoadHandle& p_a, const TAssetLoadHandle& p_b ) { return p_a.m_id == p_b.m_id; }
    };

    // One worker for Assimp/stb decode; GPU upload + register on main via tick().
    class TAssetLoadQueue
    {
    public:
        explicit TAssetLoadQueue( TAssetSystem& p_assets );
        ~TAssetLoadQueue();

        TAssetLoadQueue( const TAssetLoadQueue& )            = delete;
        TAssetLoadQueue& operator=( const TAssetLoadQueue& ) = delete;

        void setGpu( TVkGpu* p_gpu ) { m_gpu = p_gpu; }

        // Dedupes by stable path id. Optional preferred registry name (serializer).
        TAssetLoadHandle requestLoad( const std::string& p_path, const std::string& p_preferredName = {} );

        [[nodiscard]] TAssetLoadStatus   status( TAssetLoadHandle p_handle ) const;
        [[nodiscard]] bool               isReady( TAssetLoadHandle p_handle ) const;
        [[nodiscard]] bool               isFailed( TAssetLoadHandle p_handle ) const;
        [[nodiscard]] TGpuAsset*         asset( TAssetLoadHandle p_handle );
        [[nodiscard]] const std::string& error( TAssetLoadHandle p_handle ) const;

        // Claim hierarchy built during upload (Sandbox). Null after take or if unused.
        std::shared_ptr<TSceneNode> takeRoot( TAssetLoadHandle p_handle );

        using TCompleteFn = std::function<void( TAssetLoadStatus )>;
        void onComplete( TAssetLoadHandle p_handle, TCompleteFn p_fn );

        // Main thread: drain decoded packages → upload → register → callbacks.
        void tick( TScene* p_scene = nullptr );

        // Main thread only — pumps tick until Ready/Failed.
        void waitUntilReady( TAssetLoadHandle p_handle, TScene* p_scene = nullptr );
        void waitUntil( const std::function<bool()>& p_pred, TScene* p_scene = nullptr );

        void shutdown();

    private:
        struct TEntry
        {
            uint64_t                    m_id = 0;
            std::string                 m_path;
            std::string                 m_preferredName;
            std::string                 m_stableId;
            TAssetLoadStatus            m_status = TAssetLoadStatus::Pending;
            std::string                 m_error;
            TGpuAsset*                  m_asset = nullptr;
            std::shared_ptr<TSceneNode> m_root;
            std::vector<TCompleteFn>    m_callbacks;
        };

        struct TDecodedJob
        {
            uint64_t        m_id = 0;
            TCpuGltfPackage m_package;
        };

        void workerLoop();
        static std::vector<TCompleteFn> takeCallbacks( TEntry& p_entry, TAssetLoadStatus p_status );
        void                            rebindScene( TScene& p_scene ) const;

        TAssetSystem& m_assets;
        TVkGpu*       m_gpu = nullptr;

        mutable std::mutex                        m_mutex;
        std::condition_variable                   m_cv;
        std::thread                               m_worker;
        bool                                      m_stop   = false;
        uint64_t                                  m_nextId = 1;
        std::unordered_map<uint64_t, TEntry>      m_entries;
        std::unordered_map<std::string, uint64_t> m_byStableId;
        std::vector<uint64_t>                     m_decodeQueue;
        std::vector<TDecodedJob>                  m_decoded;
        static const std::string                  g_sEmptyError;
    };
}  // namespace Tomos
