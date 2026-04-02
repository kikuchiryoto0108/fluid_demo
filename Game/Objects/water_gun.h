//==============================================================================
//  File   : water_gun.h
//  Brief  : 水鉄砲クラス - チャージで発射時間が伸びる
//==============================================================================
#pragma once

#include "main.h"
#include <DirectXMath.h>

using namespace DirectX;

namespace Engine {
    class SPHFluid;
}

namespace Game {

    class Player;

    //==========================================================
    // 水鉄砲クラス
    //==========================================================
    class WaterGun {
    public:
        WaterGun();
        ~WaterGun();

        void Initialize(Engine::SPHFluid* fluid);
        void Update(Player* owner, float deltaTime);
        void Draw();

        bool IsCharging() const { return m_isCharging; }
        bool IsFiring() const { return m_isFiring; }
        bool IsInitialized() const { return m_fluid != nullptr; }
        float GetChargeRatio() const;

    private:
        void FireContinuous(Player* owner);
        XMFLOAT3 GetFireDirection(Player* owner) const;
        XMFLOAT3 GetMuzzlePosition(Player* owner) const;

    private:
        Engine::SPHFluid* m_fluid = nullptr;

        // チャージ関連
        bool m_isCharging = false;
        float m_chargeTime = 0.0f;
        float m_maxChargeTime = 3.0f;       // 最大チャージ時間

        // 発射関連
        bool m_isFiring = false;            // 発射中フラグ
        float m_fireTime = 0.0f;            // 残り発射時間
        float m_fireTimer = 0.0f;           // 発射間隔タイマー
        float m_storedChargeRatio = 0.0f;   // 発射開始時のチャージ率を保存

        // 発射時間パラメータ
        float m_minFireDuration = 0.1f;     // 最小発射時間（チャージなし）
        float m_maxFireDuration = 2.0f;     // 最大発射時間（フルチャージ）

        // 発射パラメータ
        float m_fireInterval = 0.03f;       // 発射間隔
        float m_minSpeed = 20.0f;           // 最小発射速度
        float m_maxSpeed = 50.0f;           // 最大発射速度
        uint32_t m_particlesPerShot = 50;    // 粒子数/発射
        float m_minSpread = 0.02f;          // 最小拡散（フルチャージ）
        float m_maxSpread = 0.08f;          // 最大拡散（チャージなし）
    };

} // namespace Game
