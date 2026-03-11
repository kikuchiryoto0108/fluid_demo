/*********************************************************************
 * \file   game_controller.h
 * \brief  ゲームパッド入力モジュール - WinMM/HID対応コントローラー
 * 
 * \author Ryoto Kikuchi
 * \date   2025/06/27
 *********************************************************************/
#pragma once
#include <windows.h>
#include <hidsdi.h>
#include <setupapi.h>
#include <mmsystem.h>
#include <cmath>
#include <string>

#pragma comment(lib, "hid.lib")
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "winmm.lib")

//==========================================================
// ゲームパッド状態構造体
//==========================================================
struct GamepadState {
    // --- 基本入力（WinMM） ---
    float leftStickX = 0.0f, leftStickY = 0.0f;
    float rightStickX = 0.0f, rightStickY = 0.0f;
    float leftTrigger = 0.0f, rightTrigger = 0.0f;
    bool buttons[32] = {};
    bool connected = false;

    // --- 拡張機能（HID） ---
    struct {
        float gyroPitch = 0.0f, gyroYaw = 0.0f, gyroRoll = 0.0f;
        bool available = false;
    } gyro;

    struct {
        float x = 0.0f, y = 0.0f;
        bool touched = false;
        bool available = false;
    } touchpad;

    int extendedType = 0; // 0=None, 1=PS4, 2=PS5, 3=Switch

    // --- 便利メソッド ---
    bool IsAnyButtonPressed() const {
        for (int i = 0; i < 32; i++) {
            if (buttons[i]) return true;
        }
        return false;
    }

    float GetTotalGyroRotation() const {
        if (!gyro.available) return 0.0f;
        return std::sqrt(gyro.gyroPitch * gyro.gyroPitch +
            gyro.gyroYaw * gyro.gyroYaw +
            gyro.gyroRoll * gyro.gyroRoll);
    }
};

//==========================================================
// ゲームコントローラークラス
//==========================================================
class GameController {
private:
    static HANDLE s_hidHandle;
    static int s_extendedType;
    static bool s_hidInitialized;

    // --- HID拡張機能用 ---
    static bool TryOpenPS5();
    static bool TryOpenPS4();
    static bool TryOpenSwitch();
    static bool ReadPS5Extended(GamepadState& state);
    static bool ReadPS4Extended(GamepadState& state);
    static bool ReadSwitchExtended(GamepadState& state);

public:
    //==========================================================
    // GetGamepadValue - WinMMからゲームパッドの生の値を取得
    //==========================================================
    static int GetGamepadValue(int id, int func);

    //==========================================================
    // Initialize - HID拡張機能の初期化（失敗してもOK）
    //==========================================================
    static bool Initialize() {
        if (TryOpenPS5()) {
            s_extendedType = 2;
            s_hidInitialized = true;
        }
        else if (TryOpenPS4()) {
            s_extendedType = 1;
            s_hidInitialized = true;
        }
        else if (TryOpenSwitch()) {
            s_extendedType = 3;
            s_hidInitialized = true;
        }
        else {
            s_extendedType = 0;
            s_hidInitialized = false;
        }

        return true; // WinMMは常に利用可能として扱う
    }

    //==========================================================
    // GetState - ゲームパッド状態の取得
    //==========================================================
    static bool GetState(GamepadState& state) {
        // --- 基本入力をWinMMで取得 ---
        bool winmmSuccess = GetWinMMState(state);

        // --- 拡張機能をHIDで取得（失敗してもOK） ---
        if (s_hidInitialized && winmmSuccess) {
            switch (s_extendedType) {
            case 2: ReadPS5Extended(state); break;
            case 1: ReadPS4Extended(state); break;
            case 3: ReadSwitchExtended(state); break;
            }
        }

        return winmmSuccess;
    }

    //==========================================================
    // Shutdown - リソースの解放
    //==========================================================
    static void Shutdown() {
        if (s_hidHandle != INVALID_HANDLE_VALUE) {
            CloseHandle(s_hidHandle);
            s_hidHandle = INVALID_HANDLE_VALUE;
        }
        s_hidInitialized = false;
        s_extendedType = 0;
    }

    //==========================================================
    // IsConnected - コントローラー接続確認
    //==========================================================
    static bool IsConnected() {
        JOYINFOEX ji = {};
        ji.dwSize = sizeof(JOYINFOEX);
        ji.dwFlags = JOY_RETURNBUTTONS;
        return joyGetPosEx(0, &ji) == JOYERR_NOERROR;
    }

    //==========================================================
    // GetExtendedTypeName - 拡張タイプ名の取得
    //==========================================================
    static const char* GetExtendedTypeName() {
        switch (s_extendedType) {
        case 1: return "PS4 Extended";
        case 2: return "PS5 Extended";
        case 3: return "Switch Extended";
        default: return "Basic Only";
        }
    }

private:
    static bool GetWinMMState(GamepadState& state);
};


//==========================================================
// ボタンインデックス対応表
//==========================================================
// [0]  = A/Cross/B (ジャンプ)
// [1]  = B/Circle/A
// [2]  = X/Square/Y  
// [3]  = Y/Triangle/X
// [4]  = L1/LB/L
// [5]  = R1/RB/R
// [6]  = Share/View/Minus (Select)
// [7]  = Options/Menu/Plus (Start)
// [8]  = L3/LS/LS
// [9]  = R3/RS/RS
// [10] = PS/Xbox/Home
// [11] = Touchpad (PS only)
// [12-15] = D-Pad Up/Down/Left/Right
