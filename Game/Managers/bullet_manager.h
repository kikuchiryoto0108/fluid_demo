//==============================================================================
//  File   : bullet_manager.h
//  Brief  : 弾丸マネージャー - 弾丸オブジェクトの一括管理
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/12
//------------------------------------------------------------------------------
//
//==============================================================================
#pragma once

#include "Game/Objects/bullet.h"
#include <memory>
#include <vector>

namespace Game {

    class Player;
    class DestructibleWall;

//==========================================================
// 弾丸マネージャークラス（シングルトン）
//==========================================================
class BulletManager {
private:
    std::vector<std::unique_ptr<Bullet>> m_bullets;

    BulletManager() = default;

    std::vector<DestructibleWall*> m_walls;  // 破壊可能な壁への参照

public:
    // --- シングルトンアクセス ---
    static BulletManager& GetInstance() {
        static BulletManager instance;
        return instance;
    }

    // --- 弾丸の追加 ---
    void Add(std::unique_ptr<Bullet> bullet) {
        m_bullets.push_back(std::move(bullet));
    }

    // --- 更新処理 ---
    void Update(float deltaTime);

    // --- 描画処理 ---
    void Draw() {
        for (auto& b : m_bullets) {
            b->Draw();
        }
    }

    // --- 全弾丸クリア ---
    void Clear() {
        m_bullets.clear();
    }

    // --- 弾丸数取得 ---
    size_t Count() const { return m_bullets.size(); }

    // --- コピー禁止 ---
    BulletManager(const BulletManager&) = delete;
    BulletManager& operator=(const BulletManager&) = delete;

    // 破壊可能な壁の登録
    void RegisterWall(DestructibleWall* wall) {
        if (wall) {
            m_walls.push_back(wall);
        }
    }

    // 壁の登録解除
    void ClearWalls() {
        m_walls.clear();
    }

private:
    // --- 弾丸とプレイヤーの衝突チェック ---
    void CheckBulletPlayerHits();

    void CheckBulletWallHits();
};

} // namespace Game
