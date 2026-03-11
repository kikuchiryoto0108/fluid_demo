/*********************************************************************
 * \file   input_manager.h
 * \brief  統合入力マネージャー - キーボード・マウス・ゲームパッド統合
 * 
 * \author Ryoto Kikuchi
 * \date   2025
 *********************************************************************/
#pragma once

#include "Engine/Input/keyboard.h"
#include "Engine/Input/mouse.h"
#include "Engine/Input/game_controller.h"
#include <DirectXMath.h>

namespace Engine {

//==========================================================
// FPSコマンド構造体 - ゲーム入力の抽象化
//==========================================================
struct FPSCommand {
    // --- 移動入力 ---
    float moveForward = 0.0f;   // W/S or 左スティックY (-1.0 ~ 1.0)
    float moveRight = 0.0f;     // A/D or 左スティックX (-1.0 ~ 1.0)
    float lookX = 0.0f;         // マウスX移動量 or 右スティックX
    float lookY = 0.0f;         // マウスY移動量 or 右スティックY

    // --- アクション入力（押下状態） ---
    bool jump = false;
    bool sprint = false;
    bool crouch = false;
    bool fire = false;
    bool aim = false;
    bool reload = false;
    bool interact = false;
    bool melee = false;

    // --- アクション入力（トリガー：押した瞬間） ---
    bool jumpTrigger = false;
    bool fireTrigger = false;
    bool aimTrigger = false;
    bool reloadTrigger = false;

    // --- 武器切り替え ---
    bool weaponNext = false;
    bool weaponPrev = false;
    bool weapon1 = false;
    bool weapon2 = false;
    bool weapon3 = false;
    bool weapon4 = false;

    // --- システム入力 ---
    bool pause = false;
    bool pauseTrigger = false;
    bool scoreboard = false;

    // --- 生のデバイス値 ---
    DirectX::XMFLOAT2 mousePosition = { 0.0f, 0.0f };
    DirectX::XMFLOAT2 leftStick = { 0.0f, 0.0f };
    DirectX::XMFLOAT2 rightStick = { 0.0f, 0.0f };
};

//==========================================================
// 入力マネージャークラス - シングルトン
//==========================================================
class InputManager {
private:
    FPSCommand m_command;
    FPSCommand m_prevCommand;
    bool m_mouseLocked = false;
    HWND m_hWnd = nullptr;

    // --- 定数 ---
    static constexpr float STICK_DEADZONE = 0.2f;
    static constexpr float GAMEPAD_LOOK_SENSITIVITY = 3.0f;

    InputManager() = default;

public:
    //==========================================================
    // GetInstance - シングルトンインスタンス取得
    //==========================================================
    static InputManager& GetInstance() {
        static InputManager instance;
        return instance;
    }

    // --- 基本操作 ---
    void Initialize(HWND hWnd);
    void Update();
    void Finalize();

    // --- コマンド取得 ---
    const FPSCommand& GetCommand() const { return m_command; }
    static const FPSCommand& Cmd() { return GetInstance().GetCommand(); }

    // --- マウスロック制御（FPS用） ---
    void SetMouseLocked(bool locked);
    bool IsMouseLocked() const { return m_mouseLocked; }

    // --- デバイス直接アクセス ---
    bool IsKeyDown(Keyboard_Keys key) const { return Keyboard_IsKeyDown(key); }
    bool IsKeyTrigger(Keyboard_Keys key) const { return Keyboard_IsKeyDownTrigger(key); }
    void GetMouseState(Mouse_State* state) const { Mouse_GetState(state); }
    bool GetGamepadState(GamepadState& state) const { return GameController::GetState(state); }

    // --- コピー禁止 ---
    InputManager(const InputManager&) = delete;
    InputManager& operator=(const InputManager&) = delete;
};

} // namespace Engine
