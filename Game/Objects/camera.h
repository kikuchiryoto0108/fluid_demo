//==============================================================================
//  File   : camera.h
//  Brief  : カメラシステム - 3D視点管理とプレイヤー追従
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/12
//------------------------------------------------------------------------------
//
//==============================================================================
#pragma once
#include "main.h"

namespace Game {

//==========================================================
// カメラモード列挙型
//==========================================================
enum class CameraMode {
    THIRD_PERSON,  // TPS（三人称視点）
    FIRST_PERSON   // FPS（一人称視点）
};

//==========================================================
// カメラクラス
//==========================================================
class Camera {
public:
    XMFLOAT3 position;      // カメラ位置
    XMFLOAT3 Atposition;    // 注視点
    XMFLOAT3 Upvector;      // 上方向ベクトル
    float fov;              // 視野角
    float nearclip;         // ニアクリップ
    float farclip;          // ファークリップ
    float rotation;         // 水平回転角
    float pitch;            // 垂直回転角

    // --- 更新処理 ---
    void Update(void);
};

// --- メインカメラの取得 ---
Camera& GetMainCamera();

//==========================================================
// カメラマネージャークラス（シングルトン）
//==========================================================
class CameraManager {
private:
    static CameraManager* instance;
    float rotation;     // 水平回転角
    float pitch;        // 垂直回転角
    
public:
    // --- シングルトンアクセス ---
    static CameraManager& GetInstance();
    
    // --- 更新処理 ---
    void Update(float deltaTime);
    void UpdateCameraForPlayer(int playerId);
    
    // --- アクセサ ---
    float GetRotation() const { return rotation; }
    float GetPitch() const { return pitch; }
    void SetRotation(float rot) { rotation = rot; }
    void SetPitch(float p) { pitch = p; }
};

//==========================================================
// グローバル関数
//==========================================================
void InitializeCameraSystem();
void UpdateCameraSystem();

// --- プレイヤーアクセス用ブリッジ関数 ---
class GameObject;
GameObject* GetLocalPlayerGameObject();
void UpdatePlayer();
void ForceUpdatePlayerPosition(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& rot);

} // namespace Game
