//==============================================================================
//  File   : bullet_manager.cpp
//  Brief  : 弾丸マネージャー - 弾丸とプレイヤーの衝突判定処理
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/12
//------------------------------------------------------------------------------
//
//==============================================================================
#include "pch.h"
#include "bullet_manager.h"
#include "Game/Managers/player_manager.h"
#include "Game/Objects/player.h"

namespace Game {

    // Update関数内で壁との衝突をチェック
    void BulletManager::Update(float deltaTime) {
        // 弾の更新（1回だけ）
        for (auto& b : m_bullets) {
            if (!b || !b->IsActive()) continue;
            b->Update(deltaTime);
        }

        // プレイヤーとの衝突判定
        CheckBulletPlayerHits();

        // 非アクティブな弾を削除
        m_bullets.erase(
            std::remove_if(m_bullets.begin(), m_bullets.end(),
                [](const std::unique_ptr<Bullet>& b) { return !b || !b->IsActive(); }),
            m_bullets.end());
    }

//==========================================================
// 弾丸とプレイヤーの衝突チェック
//==========================================================
void BulletManager::CheckBulletPlayerHits() {
    // 全プレイヤーIDをチェック（現在は1と2）
    for (int pid = 1; pid <= 2; ++pid) {
        Player* player = PlayerManager::GetInstance().GetPlayer(pid);
        if (!player || !player->IsAlive()) continue;

        Engine::BoxCollider* playerCol = player->GetColliderPtr();
        if (!playerCol) continue;

        for (auto& b : m_bullets) {
            if (!b || !b->IsActive()) continue;

            // --- 自分の弾には当たらない ---
            if (b->ownerPlayerId == player->GetPlayerId()) continue;

            // --- 弾のcolliderとプレイヤーのcolliderで直接AABB交差判定 ---
            if (b->GetCollider()->Intersects(playerCol)) {
                b->Deactivate();
                player->TakeDamage(1);
                break;  // この弾は消えたので次の弾へ
            }
        }
    }
}
} // namespace Game
