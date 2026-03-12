//==============================================================================
//
//  ゲームコントローラー入力管理（SDL3版） [game_controller.h]
//  Author : Ryoto Kikuchi
//  Date   : 2026/1/5
//------------------------------------------------------------------------------
//
//==============================================================================
#pragma once
#include <SDL3/SDL.h>
#include <cmath>

//==============================================================================
// ゲームパッド状態構造体
//==============================================================================
struct GamepadState {
    // 左スティック（-1.0 ~ 1.0）
    float leftStickX = 0.0f;
    float leftStickY = 0.0f;

    // 右スティック（-1.0 ~ 1.0）
    float rightStickX = 0.0f;
    float rightStickY = 0.0f;

    // トリガー（0.0 ~ 1.0）
    float leftTrigger = 0.0f;
    float rightTrigger = 0.0f;

    // 十字キー
    bool dpadUp = false;
    bool dpadDown = false;
    bool dpadLeft = false;
    bool dpadRight = false;

    // メインボタン（位置ベース: South/East/West/North）
    // Xbox:   A(下/South) B(右/East) X(左/West) Y(上/North)
    // PS:     Cross(下)   Circle(右) Square(左) Triangle(上)
    // Switch: B(下)       A(右)      Y(左)      X(上)
    bool buttonDown = false;   // South (A / Cross / B)
    bool buttonRight = false;  // East  (B / Circle / A)
    bool buttonLeft = false;   // West  (X / Square / Y)
    bool buttonUp = false;     // North (Y / Triangle / X)

    // ショルダーボタン
    bool buttonL1 = false;
    bool buttonR1 = false;

    // トリガーボタン（デジタル判定）
    bool buttonL2 = false;
    bool buttonR2 = false;

    // スティック押し込み
    bool buttonL3 = false;
    bool buttonR3 = false;

    // システムボタン
    bool buttonStart = false;
    bool buttonSelect = false;

    // 接続状態
    bool connected = false;

    bool IsAnyButtonPressed() const {
        return buttonDown || buttonRight || buttonLeft || buttonUp ||
            buttonL1 || buttonR1 || buttonL2 || buttonR2 ||
            buttonL3 || buttonR3 ||
            buttonStart || buttonSelect ||
            dpadUp || dpadDown || dpadLeft || dpadRight;
    }

    static float ApplyDeadzone(float value, float deadzone = 0.15f) {
        // デッドゾーン内なら0を返す
        if (fabs(value) < deadzone) return 0.0f;
        float sign = (value > 0) ? 1.0f : -1.0f;

        // デッドゾーンの外側を0.0 ~ 1.0に再マッピング
        float adjustedValue = (fabs(value) - deadzone) / (1.0f - deadzone);
        return sign * adjustedValue;
    }
};

//==============================================================================
// ゲームコントローラークラス（SDL3版）
//==============================================================================
class GameController {
public:
    // 初期化・終了
    static bool Initialize();
    static void Finalize();
    static void Update();

    // バイブレーション
    static void StartVibration(float intensity, float duration);
    static void StopVibration();
    static bool IsVibrating();

    //==========================================================================
    // Press判定（押している間ずっとtrue）
    //==========================================================================
    static bool IsPressed_ButtonDown() { return s_currentState.buttonDown; }
    static bool IsPressed_ButtonRight() { return s_currentState.buttonRight; }
    static bool IsPressed_ButtonLeft() { return s_currentState.buttonLeft; }
    static bool IsPressed_ButtonUp() { return s_currentState.buttonUp; }
    static bool IsPressed_L1() { return s_currentState.buttonL1; }
    static bool IsPressed_R1() { return s_currentState.buttonR1; }
    static bool IsPressed_L2() { return s_currentState.buttonL2; }
    static bool IsPressed_R2() { return s_currentState.buttonR2; }
    static bool IsPressed_L3() { return s_currentState.buttonL3; }
    static bool IsPressed_R3() { return s_currentState.buttonR3; }
    static bool IsPressed_Start() { return s_currentState.buttonStart; }
    static bool IsPressed_Select() { return s_currentState.buttonSelect; }
    static bool IsPressed_DpadUp() { return s_currentState.dpadUp; }
    static bool IsPressed_DpadDown() { return s_currentState.dpadDown; }
    static bool IsPressed_DpadLeft() { return s_currentState.dpadLeft; }
    static bool IsPressed_DpadRight() { return s_currentState.dpadRight; }

    //==========================================================================
    // Trigger判定（押した瞬間だけtrue）
    //==========================================================================
    static bool IsTrigger_ButtonDown() { return s_currentState.buttonDown && !s_prevState.buttonDown; }
    static bool IsTrigger_ButtonRight() { return s_currentState.buttonRight && !s_prevState.buttonRight; }
    static bool IsTrigger_ButtonLeft() { return s_currentState.buttonLeft && !s_prevState.buttonLeft; }
    static bool IsTrigger_ButtonUp() { return s_currentState.buttonUp && !s_prevState.buttonUp; }
    static bool IsTrigger_L1() { return s_currentState.buttonL1 && !s_prevState.buttonL1; }
    static bool IsTrigger_R1() { return s_currentState.buttonR1 && !s_prevState.buttonR1; }
    static bool IsTrigger_L2() { return s_currentState.buttonL2 && !s_prevState.buttonL2; }
    static bool IsTrigger_R2() { return s_currentState.buttonR2 && !s_prevState.buttonR2; }
    static bool IsTrigger_L3() { return s_currentState.buttonL3 && !s_prevState.buttonL3; }
    static bool IsTrigger_R3() { return s_currentState.buttonR3 && !s_prevState.buttonR3; }
    static bool IsTrigger_Start() { return s_currentState.buttonStart && !s_prevState.buttonStart; }
    static bool IsTrigger_Select() { return s_currentState.buttonSelect && !s_prevState.buttonSelect; }
    static bool IsTrigger_DpadUp() { return s_currentState.dpadUp && !s_prevState.dpadUp; }
    static bool IsTrigger_DpadDown() { return s_currentState.dpadDown && !s_prevState.dpadDown; }
    static bool IsTrigger_DpadLeft() { return s_currentState.dpadLeft && !s_prevState.dpadLeft; }
    static bool IsTrigger_DpadRight() { return s_currentState.dpadRight && !s_prevState.dpadRight; }

    //==========================================================================
    // Release判定（離した瞬間だけtrue）
    //==========================================================================
    static bool IsRelease_ButtonDown() { return !s_currentState.buttonDown && s_prevState.buttonDown; }
    static bool IsRelease_ButtonRight() { return !s_currentState.buttonRight && s_prevState.buttonRight; }
    static bool IsRelease_ButtonLeft() { return !s_currentState.buttonLeft && s_prevState.buttonLeft; }
    static bool IsRelease_ButtonUp() { return !s_currentState.buttonUp && s_prevState.buttonUp; }
    static bool IsRelease_L1() { return !s_currentState.buttonL1 && s_prevState.buttonL1; }
    static bool IsRelease_R1() { return !s_currentState.buttonR1 && s_prevState.buttonR1; }
    static bool IsRelease_L2() { return !s_currentState.buttonL2 && s_prevState.buttonL2; }
    static bool IsRelease_R2() { return !s_currentState.buttonR2 && s_prevState.buttonR2; }
    static bool IsRelease_L3() { return !s_currentState.buttonL3 && s_prevState.buttonL3; }
    static bool IsRelease_R3() { return !s_currentState.buttonR3 && s_prevState.buttonR3; }
    static bool IsRelease_Start() { return !s_currentState.buttonStart && s_prevState.buttonStart; }
    static bool IsRelease_Select() { return !s_currentState.buttonSelect && s_prevState.buttonSelect; }
    static bool IsRelease_DpadUp() { return !s_currentState.dpadUp && s_prevState.dpadUp; }
    static bool IsRelease_DpadDown() { return !s_currentState.dpadDown && s_prevState.dpadDown; }
    static bool IsRelease_DpadLeft() { return !s_currentState.dpadLeft && s_prevState.dpadLeft; }
    static bool IsRelease_DpadRight() { return !s_currentState.dpadRight && s_prevState.dpadRight; }

    //==========================================================================
    // スティック・トリガー値取得
    //==========================================================================
    static float GetLeftStickX() { return s_currentState.leftStickX; }
    static float GetLeftStickY() { return s_currentState.leftStickY; }
    static float GetRightStickX() { return s_currentState.rightStickX; }
    static float GetRightStickY() { return s_currentState.rightStickY; }
    static float GetLeftTrigger() { return s_currentState.leftTrigger; }
    static float GetRightTrigger() { return s_currentState.rightTrigger; }

    // 接続状態
    static bool IsConnected() { return s_currentState.connected; }

    // コントローラー名取得（デバッグ用）
    static const char* GetControllerName();

    // 現在の状態を取得
    static bool GetState(GamepadState& outState) {
        outState = s_currentState;
        return s_currentState.connected;
    }

private:
    static bool UpdateState();
    static void TryOpenGamepad();
    static float NormalizeAxisValue(Sint16 value);

private:
    static SDL_Gamepad* s_gamepad;
    static SDL_JoystickID s_gamepadID;
    static GamepadState s_currentState;
    static GamepadState s_prevState;
    static bool s_isVibrating;
    static Uint64 s_vibrationEndTime;
    static bool s_sdlInitialized;
};