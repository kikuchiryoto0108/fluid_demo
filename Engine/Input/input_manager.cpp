/*********************************************************************
 * \file   input_manager.cpp
 * \brief  統合入力マネージャー - キーボード・マウス・ゲームパッド統合
 * 
 * \author Ryoto Kikuchi
 * \date   2025
 *********************************************************************/
#include "pch.h"
#include "input_manager.h"
#include <cmath>

namespace Engine {

//==========================================================
// Initialize - 入力デバイスの初期化
//==========================================================
void InputManager::Initialize(HWND hWnd) {
    m_hWnd = hWnd;
    Keyboard_Initialize();
    Mouse_Initialize(hWnd);
    GameController::Initialize();
    m_command = {};
    m_prevCommand = {};
}

//==========================================================
// Finalize - 入力デバイスの終了処理
//==========================================================
void InputManager::Finalize() {
    SetMouseLocked(false);
    GameController::Shutdown();
    Mouse_Finalize();
}

//==========================================================
// Update - 入力状態の更新
//==========================================================
void InputManager::Update() {
    m_prevCommand = m_command;
    m_command = {};

    //==========================================================
    // キーボード入力
    //==========================================================
    float kbForward = 0.0f, kbRight = 0.0f;
    if (Keyboard_IsKeyDown(KK_W)) kbForward += 1.0f;
    if (Keyboard_IsKeyDown(KK_S)) kbForward -= 1.0f;
    if (Keyboard_IsKeyDown(KK_A)) kbRight -= 1.0f;
    if (Keyboard_IsKeyDown(KK_D)) kbRight += 1.0f;

    m_command.jump = Keyboard_IsKeyDown(KK_SPACE);
    m_command.sprint = Keyboard_IsKeyDown(KK_LEFTSHIFT);
    m_command.crouch = Keyboard_IsKeyDown(KK_LEFTCONTROL);
    m_command.reload = Keyboard_IsKeyDown(KK_R);
    m_command.interact = Keyboard_IsKeyDown(KK_E);
    m_command.melee = Keyboard_IsKeyDown(KK_V);
    m_command.pause = Keyboard_IsKeyDown(KK_ESCAPE);
    m_command.scoreboard = Keyboard_IsKeyDown(KK_TAB);

    m_command.weapon1 = Keyboard_IsKeyDown(KK_D1);
    m_command.weapon2 = Keyboard_IsKeyDown(KK_D2);
    m_command.weapon3 = Keyboard_IsKeyDown(KK_D3);
    m_command.weapon4 = Keyboard_IsKeyDown(KK_D4);

    //==========================================================
    // マウス入力
    //==========================================================
    Mouse_State mouseState;
    Mouse_GetState(&mouseState);

    m_command.fire = mouseState.leftButton;
    m_command.aim = mouseState.rightButton;

    m_command.mousePosition = { static_cast<float>(mouseState.x), static_cast<float>(mouseState.y) };

    if (mouseState.positionMode == MOUSE_POSITION_MODE_RELATIVE) {
        m_command.lookX = static_cast<float>(mouseState.x);
        m_command.lookY = static_cast<float>(mouseState.y);
    }

    if (mouseState.scrollWheelValue > 0) m_command.weaponNext = true;
    if (mouseState.scrollWheelValue < 0) m_command.weaponPrev = true;

    //==========================================================
    // ゲームパッド入力
    //==========================================================
    GamepadState pad;
    if (GameController::GetState(pad) && pad.connected) {
        float lx = pad.leftStickX;
        float ly = pad.leftStickY;
        float rx = pad.rightStickX;
        float ry = pad.rightStickY;

        if (std::fabsf(lx) > STICK_DEADZONE || std::fabsf(ly) > STICK_DEADZONE) {
            m_command.moveForward += -ly;
            m_command.moveRight += lx;
        }

        if (std::fabsf(rx) > STICK_DEADZONE || std::fabsf(ry) > STICK_DEADZONE) {
            m_command.lookX += rx * GAMEPAD_LOOK_SENSITIVITY;
            m_command.lookY += ry * GAMEPAD_LOOK_SENSITIVITY;
        }

        m_command.leftStick = { lx, ly };
        m_command.rightStick = { rx, ry };

        // --- ボタンマッピング ---
        // SOUTH=A(jump), EAST=B, WEST=X(reload), NORTH=Y
        if (pad.buttons[0]) m_command.jump = true;        // A/Cross (Jump)
        if (pad.buttons[2]) m_command.reload = true;      // X/Square (Reload)
        if (pad.buttons[3]) m_command.interact = true;    // Y/Triangle (Interact)
        if (pad.buttons[4]) m_command.aim = true;         // L1 (Aim)
        if (pad.buttons[5]) m_command.fire = true;        // R1 (Fire)
        if (pad.buttons[7]) m_command.pause = true;       // Options/Start (Pause)
        if (pad.buttons[8]) m_command.sprint = true;      // L3 (Sprint)
        if (pad.buttons[9]) m_command.melee = true;       // R3 (Melee)
        if (pad.leftTrigger > 0.5f) m_command.aim = true; // L2 (Aim)
        if (pad.rightTrigger > 0.5f) m_command.fire = true; // R2 (Fire)
    }

    //==========================================================
    // 入力値のクランプ
    //==========================================================
    m_command.moveForward += kbForward;
    m_command.moveRight += kbRight;
    if (m_command.moveForward > 1.0f) m_command.moveForward = 1.0f;
    if (m_command.moveForward < -1.0f) m_command.moveForward = -1.0f;
    if (m_command.moveRight > 1.0f) m_command.moveRight = 1.0f;
    if (m_command.moveRight < -1.0f) m_command.moveRight = -1.0f;

    //==========================================================
    // トリガー検出（押した瞬間）
    //==========================================================
    m_command.jumpTrigger = m_command.jump && !m_prevCommand.jump;
    m_command.fireTrigger = m_command.fire && !m_prevCommand.fire;
    m_command.aimTrigger = m_command.aim && !m_prevCommand.aim;
    m_command.reloadTrigger = m_command.reload && !m_prevCommand.reload;
    m_command.pauseTrigger = m_command.pause && !m_prevCommand.pause;
}

//==========================================================
// SetMouseLocked - マウスロック制御（FPS用）
//==========================================================
void InputManager::SetMouseLocked(bool locked) {
    m_mouseLocked = locked;
    if (locked) {
        Mouse_SetMode(MOUSE_POSITION_MODE_RELATIVE);
        Mouse_SetVisible(false);
    } else {
        Mouse_SetMode(MOUSE_POSITION_MODE_ABSOLUTE);
        Mouse_SetVisible(true);
    }
}

} // namespace Engine
