//==============================================================================
//  File   : collision_system.h
//  Brief  : コリジョンシステム - 動的オブジェクト間の衝突検出
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/12
//------------------------------------------------------------------------------
//
//==============================================================================
#pragma once

#include "collider.h"
#include "box_collider.h"
#include <vector>
#include <functional>
#include <unordered_map>

namespace Engine {

    //==========================================================
    // 衝突レイヤー定義（ビットフラグ）
    //==========================================================
    enum class CollisionLayer : uint32_t {
        NONE = 0,
        PLAYER = 1 << 0,      // プレイヤー
        ENEMY = 1 << 1,       // 敵
        PROJECTILE = 1 << 2,  // 弾丸・投射物
        TRIGGER = 1 << 3,     // トリガー領域
        ALL = 0xFFFFFFFF      // 全レイヤー
    };

    // --- レイヤーのビット演算ヘルパー ---
    inline CollisionLayer operator|(CollisionLayer a, CollisionLayer b) {
        return static_cast<CollisionLayer>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }

    inline bool HasFlag(CollisionLayer value, CollisionLayer flag) {
        return (static_cast<uint32_t>(value) & static_cast<uint32_t>(flag)) != 0;
    }

    //==========================================================
    // コライダーデータ構造体
    //==========================================================
    struct ColliderData {
        Collider* collider = nullptr;           // コライダーへのポインタ
        CollisionLayer layer = CollisionLayer::NONE;  // 所属レイヤー
        CollisionLayer mask = CollisionLayer::ALL;    // 衝突対象マスク
        void* userData = nullptr;               // ユーザーデータ
        uint32_t id = 0;                        // 一意のID
        bool enabled = true;                    // 有効フラグ
    };

    //==========================================================
    // 衝突情報構造体
    //==========================================================
    struct CollisionHit {
        ColliderData* dataA = nullptr;          // 衝突オブジェクトA
        ColliderData* dataB = nullptr;          // 衝突オブジェクトB
        XMFLOAT3 penetration = { 0, 0, 0 };     // めり込みベクトル
    };

    // 衝突コールバック型
    using CollisionCallback = std::function<void(const CollisionHit&)>;

    //==========================================================
    // CollisionSystem - 衝突検出システム（シングルトン）
    //==========================================================
    class CollisionSystem {
    public:
        static CollisionSystem& GetInstance();

        // --- 初期化・終了 ---
        void Initialize();
        void Shutdown();

        // --- コライダー管理 ---
        uint32_t Register(Collider* collider, CollisionLayer layer, CollisionLayer mask, void* userData);
        void Unregister(uint32_t id);
        void SetEnabled(uint32_t id, bool enabled);

        // --- 更新処理 ---
        void Update();
        void SetCallback(CollisionCallback callback) { m_callback = std::move(callback); }

    private:
        CollisionSystem() = default;

        // --- メンバ変数 ---
        std::unordered_map<uint32_t, ColliderData> m_colliders;  // 登録済みコライダー
        uint32_t m_nextId = 1;                                    // 次のID
        CollisionCallback m_callback;                             // 衝突時コールバック
    };

} // namespace Engine
