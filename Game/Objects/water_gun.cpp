//==============================================================================
//  File   : water_gun.cpp
//  Brief  : 水鉄砲クラス - チャージで発射時間が伸びる
//==============================================================================
#include "pch.h"
#include "water_gun.h"
#include "player.h"
#include "camera.h"
#include "Engine/Physics/sph_fluid.h"
#include "Engine/Input/input_manager.h"
#include <cmath>
#include <cstdlib>

namespace Game {

    //==========================================================
    // コンストラクタ
    //==========================================================
    WaterGun::WaterGun() {
    }

    //==========================================================
    // デストラクタ
    //==========================================================
    WaterGun::~WaterGun() {
    }

    //==========================================================
    // 初期化
    //==========================================================
    void WaterGun::Initialize(Engine::SPHFluid* fluid) {
        m_fluid = fluid;
        m_isCharging = false;
        m_isFiring = false;
        m_chargeTime = 0.0f;
        m_fireTime = 0.0f;
        m_fireTimer = 0.0f;
        m_storedChargeRatio = 0.0f;
    }

    //==========================================================
    // チャージ率取得（0.0 ~ 1.0）
    //==========================================================
    float WaterGun::GetChargeRatio() const {
        if (m_maxChargeTime <= 0.0f) return 0.0f;
        return std::min(m_chargeTime / m_maxChargeTime, 1.0f);
    }

    //==========================================================
    // 発射方向を取得（カメラの向き）
    //==========================================================
    XMFLOAT3 WaterGun::GetFireDirection(Player* owner) const {
        Camera& camera = GetMainCamera();

        XMFLOAT3 dir = {
            camera.Atposition.x - camera.position.x,
            camera.Atposition.y - camera.position.y,
            camera.Atposition.z - camera.position.z
        };

        float len = sqrtf(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
        if (len > 0.0001f) {
            dir.x /= len;
            dir.y /= len;
            dir.z /= len;
        } else {
            dir = { 0.0f, 0.0f, 1.0f };
        }

        return dir;
    }

    //==========================================================
    // 銃口位置を取得
    //==========================================================
    XMFLOAT3 WaterGun::GetMuzzlePosition(Player* owner) const {
        if (!owner) return { 0.0f, 0.0f, 0.0f };

        XMFLOAT3 pos = owner->GetPosition();
        XMFLOAT3 dir = GetFireDirection(owner);

        pos.x += dir.x * 1.0f;
        pos.y += 1.0f;
        pos.z += dir.z * 1.0f;

        return pos;
    }

    //==========================================================
    // 更新処理
    //==========================================================
    void WaterGun::Update(Player* owner, float deltaTime) {
        if (!m_fluid || !owner) return;

        bool leftPressed = Mouse::Instance().IsPressed(MouseButton::Left);

        //----------------------------------------------------------
        // 発射中の処理
        //----------------------------------------------------------
        if (m_isFiring) {
            m_fireTimer += deltaTime;

            // 発射間隔ごとに水を出す
            if (m_fireTimer >= m_fireInterval) {
                FireContinuous(owner);
                m_fireTimer = 0.0f;
            }

            // 発射時間を減らす
            m_fireTime -= deltaTime;

            // 発射終了
            if (m_fireTime <= 0.0f) {
                m_isFiring = false;
                m_fireTime = 0.0f;
                m_storedChargeRatio = 0.0f;
            }

            // 発射中はチャージできない
            return;
        }

        //----------------------------------------------------------
        // チャージ処理
        //----------------------------------------------------------
        if (leftPressed) {
            // チャージ中
            if (!m_isCharging) {
                m_isCharging = true;
                m_chargeTime = 0.0f;
            }

            m_chargeTime += deltaTime;
            if (m_chargeTime > m_maxChargeTime) {
                m_chargeTime = m_maxChargeTime;
            }
        } else {
            // 離した → 発射開始
            if (m_isCharging) {
                float chargeRatio = GetChargeRatio();

                // 発射時間を計算（チャージ率に応じて）
                m_fireTime = m_minFireDuration + (m_maxFireDuration - m_minFireDuration) * chargeRatio;
                m_storedChargeRatio = chargeRatio;
                m_isFiring = true;
                m_fireTimer = 0.0f;

                char buf[128];
                sprintf_s(buf, "WaterGun: Fire! charge=%.0f%% duration=%.2fs\n",
                    chargeRatio * 100.0f, m_fireTime);
                OutputDebugStringA(buf);

                m_isCharging = false;
                m_chargeTime = 0.0f;
            }
        }
    }

    //==========================================================
    // 連続発射（発射中に呼ばれる）
    //==========================================================
    void WaterGun::FireContinuous(Player* owner) {
        if (!m_fluid || !owner) return;

        // 保存されたチャージ率を使用
        float chargeRatio = m_storedChargeRatio;

        // チャージに応じたパラメータ
        float speed = m_minSpeed + (m_maxSpeed - m_minSpeed) * chargeRatio;
        float spread = m_maxSpread - (m_maxSpread - m_minSpread) * chargeRatio;

        XMFLOAT3 muzzlePos = GetMuzzlePosition(owner);
        XMFLOAT3 baseDir = GetFireDirection(owner);

        for (uint32_t i = 0; i < m_particlesPerShot; ++i) {
            // 拡散を加える
            float randX = (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 2.0f * spread;
            float randY = (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 2.0f * spread;
            float randZ = (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 2.0f * spread;

            XMFLOAT3 dir = {
                baseDir.x + randX,
                baseDir.y + randY,
                baseDir.z + randZ
            };

            // 正規化
            float len = sqrtf(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
            if (len > 0.0001f) {
                dir.x /= len;
                dir.y /= len;
                dir.z /= len;
            }

            // 発射位置を少しランダムにずらす
            XMFLOAT3 spawnPos = {
                muzzlePos.x + (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 0.1f,
                muzzlePos.y + (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 0.1f,
                muzzlePos.z + (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 0.1f
            };

            // 速度にランダム性を加える
            float speedVariation = speed * (0.9f + static_cast<float>(rand()) / RAND_MAX * 0.2f);

            m_fluid->SpawnParticleWithVelocity(spawnPos, {
                dir.x * speedVariation,
                dir.y * speedVariation,
                dir.z * speedVariation
                });
        }
    }

    //==========================================================
    // 描画
    //==========================================================
    void WaterGun::Draw() {
        // TODO: チャージゲージのUI描画
    }

} // namespace Game
