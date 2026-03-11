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

//==========================================================
// 弾丸マネージャークラス（シングルトン）
//==========================================================
class BulletManager {
private:
    std::vector<std::unique_ptr<Bullet>> m_bullets;

    BulletManager() = default;

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
    void Update(float deltaTime) {
        for (auto& b : m_bullets) {
            b->Update(deltaTime);
        }

        // CollisionSystemのコールバックが動かない場合のフォールバック
        // 弾のcolliderとPlayerのcolliderを直接チェック
        CheckBulletPlayerHits();

        // 非アクティブな弾を削除
        m_bullets.erase(
            std::remove_if(m_bullets.begin(), m_bullets.end(),
                [](const std::unique_ptr<Bullet>& b) { return !b->active; }),
            m_bullets.end());
    }

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

private:
    // --- 弾丸とプレイヤーの衝突チェック ---
    void CheckBulletPlayerHits();
};

} // namespace Game
