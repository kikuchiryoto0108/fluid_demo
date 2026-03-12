//==============================================================================
//  File   : camera.cpp
//  Brief  : カメラシステム - 視点制御とプレイヤー追従処理
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/12
//------------------------------------------------------------------------------
//
//==============================================================================
#include "pch.h"
#include "camera.h"
#include "Engine/Input/input_manager.h"
#include "player.h"
#include "Game/Managers/player_manager.h"
#include "Game/Map/map.h"
#include "NetWork/network_manager.h"
#include <cmath>
#include <stdio.h>
#include <iostream>

namespace Game {

//==========================================================
// メインカメラのシングルトン
//==========================================================
static Camera s_mainCamera = {
    XMFLOAT3(0.0f, 0.0f, -4.0f),
    XMFLOAT3(0.0f, 0.0f, 1.0f),
    XMFLOAT3(0.0f, 1.0f, 0.0f),
    45.0f,
    0.5f,
    1000.0f,
    0.0f,
    0.0f
};

Camera& GetMainCamera() {
    return s_mainCamera;
}

//==========================================================
// プレイヤー初期化用（外部からは呼ばない）
//==========================================================
void InitializePlayer(Map* map, ID3D11ShaderResourceView* texture) {
    InitializePlayers(map, texture);
}

//==========================================================
// プレイヤー描画用
//==========================================================
void DrawPlayer() {
    DrawPlayers();
}

//==========================================================
// ローカルプレイヤーのGameObject取得
//==========================================================
GameObject* GetLocalPlayerGameObject() {
    return GetActivePlayerGameObject();
}

//==========================================================
// UpdatePlayerラッパー（main.cppから呼び出し）
//==========================================================
void UpdatePlayer() {
    UpdatePlayers();
}

//==========================================================
// ネットワーク用: プレイヤー強制移動
//==========================================================
void ForceUpdatePlayerPosition(const XMFLOAT3& pos, const XMFLOAT3& rot) {
    Player* activePlayer = GetActivePlayer();
    if (activePlayer) {
        activePlayer->ForceSetPosition(pos);
        activePlayer->ForceSetRotation(rot);
    }
}

//==========================================================
// カメラ更新処理
//==========================================================
void Camera::Update(void) {
    // CameraManagerに処理を委譲
    CameraManager::GetInstance().Update(1.0f / 60.0f);
}

//==========================================================
// CameraManager実装
//==========================================================
CameraManager* CameraManager::instance = nullptr;

CameraManager& CameraManager::GetInstance() {
    if (!instance) {
        instance = new CameraManager();
    }
    return *instance;
}

//==========================================================
// CameraManager更新処理
//==========================================================
void CameraManager::Update(float deltaTime) {
    // --- 定数定義 ---
    constexpr float rotateSpeed = 2.0f;

    // InputManagerからコマンドを取得
    InputManager& inputMgr = InputManager::Instance();
    const InputCommand& cmd = inputMgr.GetCommand();

    // --- マウス右クリックでカメラ操作モード切替 ---
    static bool wasRightButtonDown = false;
    bool rightButtonDown = Mouse::Instance().IsPressed(MouseButton::Right);

    if (rightButtonDown && !wasRightButtonDown) {
        // 右クリック開始: マウスをロック（相対座標モード）
        inputMgr.SetMouseLocked(true);
    } else if (!rightButtonDown && wasRightButtonDown) {
        // 右クリック終了: マウスをアンロック（絶対座標モード）
        inputMgr.SetMouseLocked(false);
    }
    wasRightButtonDown = rightButtonDown;

    // --- カメラ回転入力（InputManagerのlookX/lookYを使用） ---
    // lookX/lookYにはマウス移動量とゲームパッド右スティックの両方が統合されている
    if (inputMgr.IsMouseLocked() ||
        std::fabs(cmd.rightStick.x) > 0.01f ||
        std::fabs(cmd.rightStick.y) > 0.01f) {
        rotation += cmd.lookX;
        pitch -= cmd.lookY;

        // ピッチ角度を制限
        if (pitch > 89.0f) pitch = 89.0f;
        if (pitch < -89.0f) pitch = -89.0f;
    }

    // --- カメラデバッグモード（矢印キーでの操作） ---
    if (inputMgr.IsCameraDebugMode()) {
        DirectX::XMFLOAT2 debugMove = inputMgr.GetCameraDebugMove();
        // デバッグモードでは矢印キーで直接回転を制御
        rotation += debugMove.x * deltaTime;
        pitch += debugMove.y * deltaTime;

        if (pitch > 89.0f) pitch = 89.0f;
        if (pitch < -89.0f) pitch = -89.0f;
    }

    // --- アクティブプレイヤーに基づいてカメラを更新 ---
    PlayerManager& playerMgr = PlayerManager::GetInstance();
    UpdateCameraForPlayer(playerMgr.GetActivePlayerId());
}

//==========================================================
// プレイヤー用カメラ更新
//==========================================================
void CameraManager::UpdateCameraForPlayer(int playerId) {
    Player* player = PlayerManager::GetInstance().GetPlayer(playerId);
    if (!player) return;

    // --- プレイヤーの回転をカメラの回転に同期 ---
    XMFLOAT3 playerRot = player->GetRotation();
    if (player->GetViewMode() == ViewMode::FIRST_PERSON) {
        // 1人称: カメラの向きにプレイヤーを合わせる（逆転の逆）
        playerRot.y = rotation;
        player->ForceSetRotation(playerRot);
    } else {
        // 3人称: カメラの向きにプレイヤーを合わせる
        playerRot.y = rotation;
        player->ForceSetRotation(playerRot);
    }

    // --- カメラの方向ベクトル計算 ---
    float yawRad = XMConvertToRadians(rotation);
    float pitchRad = XMConvertToRadians(pitch);

    float cosYaw = cosf(yawRad);
    float sinYaw = sinf(yawRad);
    float cosPitch = cosf(pitchRad);
    float sinPitch = sinf(pitchRad);

    XMFLOAT3 forward = {
        sinYaw * cosPitch,
        sinPitch,
        cosYaw * cosPitch
    };

    XMFLOAT3 playerPos = player->GetPosition();
    ViewMode viewMode = player->GetViewMode();

    if (viewMode == ViewMode::THIRD_PERSON) {
        // --- 3人称視点: プレイヤーの後ろから見る ---
        constexpr float cameraDistance = 5.0f;
        constexpr float cameraHeight = 2.0f;

        s_mainCamera.position.x = playerPos.x - forward.x * cameraDistance;
        s_mainCamera.position.y = playerPos.y + cameraHeight - forward.y * cameraDistance;
        s_mainCamera.position.z = playerPos.z - forward.z * cameraDistance;

        // 注視点はプレイヤーの中心
        s_mainCamera.Atposition.x = playerPos.x;
        s_mainCamera.Atposition.y = playerPos.y + 1.0f;
        s_mainCamera.Atposition.z = playerPos.z;
    } else { // FIRST_PERSON
        // --- 1人称視点: プレイヤーの目線 ---
        s_mainCamera.position.x = playerPos.x;
        s_mainCamera.position.y = playerPos.y + 1.0f; // 目の高さ
        s_mainCamera.position.z = playerPos.z;

        s_mainCamera.Atposition.x = s_mainCamera.position.x + forward.x;
        s_mainCamera.Atposition.y = s_mainCamera.position.y + forward.y;
        s_mainCamera.Atposition.z = s_mainCamera.position.z + forward.z;
    }
}

//==========================================================
// グローバル関数
//==========================================================
void InitializeCameraSystem() {
    // 初期化は特に必要なし（シングルトンが自動的に初期化される）
}

void UpdateCameraSystem() {
    CameraManager::GetInstance().Update(1.0f / 60.0f);
}

} // namespace Game
