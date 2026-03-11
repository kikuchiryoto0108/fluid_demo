/*********************************************************************
 * \file   game_controller.cpp
 * \brief  ゲームパッド入力モジュール - WinMM/HID対応コントローラー
 * 
 * \author Ryoto Kikuchi
 * \date   2025/06/27
 *********************************************************************/
#include "pch.h"
#include "game_controller.h"
#include <iostream>
#include <vector>

//==========================================================
// 静的メンバ変数の定義
//==========================================================
HANDLE GameController::s_hidHandle = INVALID_HANDLE_VALUE;
int GameController::s_extendedType = 0;
bool GameController::s_hidInitialized = false;

//==========================================================
// GetGamepadValue - WinMMからゲームパッドの生の値を取得
//==========================================================
int GameController::GetGamepadValue(int id, int func) {
    JOYINFOEX ji;
    ji.dwSize = sizeof(JOYINFOEX);
    ji.dwFlags = JOY_RETURNALL;

    if (joyGetPosEx(id, &ji) != JOYERR_NOERROR)
        return 32767;

    switch (func) {
    case 0: return ji.dwXpos;
    case 1: return ji.dwYpos;
    case 2: return ji.dwZpos;
    case 3: return ji.dwButtons;
    case 4: return ji.dwRpos;
    case 5: return ji.dwUpos;
    case 6: return ji.dwVpos;
    case 7: return ji.dwButtonNumber;
    case 8: return ji.dwPOV;
    case 9: return ji.dwReserved1;
    case 10: return ji.dwReserved2;
    default: return 0;
    }
}

//==========================================================
// GetWinMMState - WinMMを使用した基本入力状態の取得
//==========================================================
bool GameController::GetWinMMState(GamepadState& state) {
    static int s_workingControllerId = -1;

    // --- 動作するコントローラーIDを探す ---
    if (s_workingControllerId == -1) {
        for (int id = 0; id < 16; id++) {
            int testValue = GetGamepadValue(id, 3); // ボタン状態を取得してテスト
            if (testValue != 32767) { // エラーでない場合
                s_workingControllerId = id;
                break;
            }
        }

        if (s_workingControllerId == -1) {
            state.connected = false;
            return false;
        }
    }

    // --- 入力値の取得 ---
    int leftX = GetGamepadValue(s_workingControllerId, 0);
    int leftY = GetGamepadValue(s_workingControllerId, 1);
    // 右スティックは dwRpos (4) と dwUpos (5)
    int rightX = GetGamepadValue(s_workingControllerId, 4);
    int rightY = GetGamepadValue(s_workingControllerId, 5);
    // トリガーは Z/Rなどの組合せだが既存のマッピングを維持
    int triggerL = GetGamepadValue(s_workingControllerId, 2);
    int triggerR = GetGamepadValue(s_workingControllerId, 6);
    int buttons = GetGamepadValue(s_workingControllerId, 3);

    // --- デバイス切断検出 ---
    if (leftX == 32767) {
        s_workingControllerId = -1; // IDをリセットして再検出させる
        state.connected = false;
        return false;
    }

    state.connected = true;

    // --- 軸値を正規化 ---
    state.leftStickX = (float)(leftX - 32760) / 32767.0f;
    state.leftStickY = (float)(leftY - 32760) / 32767.0f;
    state.rightStickX = (float)(rightX - 32760) / 32767.0f;
    state.rightStickY = (float)(rightY - 32760) / 32767.0f;
    // トリガーは0..65535を0..1へ
    state.leftTrigger = (float)triggerL / 65535.0f;
    state.rightTrigger = (float)triggerR / 65535.0f;

    // --- ボタン状態の設定 ---
    for (int i = 0; i < 32; i++) {
        state.buttons[i] = (buttons & (1 << i)) != 0;
    }

    return true;
}

//==========================================================
// TryOpenPS5 - PS5 DualSenseコントローラーのHIDデバイスを開く
//==========================================================
bool GameController::TryOpenPS5() {
    GUID hidGuid;
    HidD_GetHidGuid(&hidGuid);

    HDEVINFO deviceInfoSet = SetupDiGetClassDevsA(&hidGuid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (deviceInfoSet == INVALID_HANDLE_VALUE) return false;

    SP_DEVICE_INTERFACE_DATA deviceInterfaceData = {};
    deviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(deviceInfoSet, nullptr, &hidGuid, i, &deviceInterfaceData); ++i) {
        DWORD requiredSize = 0;
        SetupDiGetDeviceInterfaceDetailA(deviceInfoSet, &deviceInterfaceData, nullptr, 0, &requiredSize, nullptr);
        if (requiredSize == 0) continue;

        std::vector<uint8_t> buffer(requiredSize);
        auto pDetailData = reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA_A>(buffer.data());
        pDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);

        if (!SetupDiGetDeviceInterfaceDetailA(deviceInfoSet, &deviceInterfaceData, pDetailData, requiredSize, nullptr, nullptr)) continue;

        HANDLE hDev = CreateFileA(pDetailData->DevicePath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        if (hDev == INVALID_HANDLE_VALUE) continue;

        HIDD_ATTRIBUTES attrib = {};
        attrib.Size = sizeof(HIDD_ATTRIBUTES);

        if (HidD_GetAttributes(hDev, &attrib)) {
            // PS5 DualSense チェック
            if (attrib.VendorID == 0x054C && (attrib.ProductID == 0x0CE6 || attrib.ProductID == 0x0DF2)) {
                s_hidHandle = hDev;
                SetupDiDestroyDeviceInfoList(deviceInfoSet);
                return true;
            }
        }
        CloseHandle(hDev);
    }

    SetupDiDestroyDeviceInfoList(deviceInfoSet);
    return false;
}

//==========================================================
// TryOpenPS4 - PS4 DualShock4コントローラーのHIDデバイスを開く
//==========================================================
bool GameController::TryOpenPS4() {
    GUID hidGuid;
    HidD_GetHidGuid(&hidGuid);

    HDEVINFO deviceInfoSet = SetupDiGetClassDevsA(&hidGuid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (deviceInfoSet == INVALID_HANDLE_VALUE) return false;

    SP_DEVICE_INTERFACE_DATA deviceInterfaceData = {};
    deviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(deviceInfoSet, nullptr, &hidGuid, i, &deviceInterfaceData); ++i) {
        DWORD requiredSize = 0;
        SetupDiGetDeviceInterfaceDetailA(deviceInfoSet, &deviceInterfaceData, nullptr, 0, &requiredSize, nullptr);
        if (requiredSize == 0) continue;

        std::vector<uint8_t> buffer(requiredSize);
        auto pDetailData = reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA_A>(buffer.data());
        pDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);

        if (!SetupDiGetDeviceInterfaceDetailA(deviceInfoSet, &deviceInterfaceData, pDetailData, requiredSize, nullptr, nullptr)) continue;

        HANDLE hDev = CreateFileA(pDetailData->DevicePath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        if (hDev == INVALID_HANDLE_VALUE) continue;

        HIDD_ATTRIBUTES attrib = {};
        attrib.Size = sizeof(HIDD_ATTRIBUTES);

        if (HidD_GetAttributes(hDev, &attrib)) {
            // PS4 DualShock4 チェック
            if (attrib.VendorID == 0x054C && (attrib.ProductID == 0x09CC || attrib.ProductID == 0x05C4 || attrib.ProductID == 0x0BA0)) {
                s_hidHandle = hDev;
                SetupDiDestroyDeviceInfoList(deviceInfoSet);
                return true;
            }
        }
        CloseHandle(hDev);
    }

    SetupDiDestroyDeviceInfoList(deviceInfoSet);
    return false;
}

//==========================================================
// TryOpenSwitch - Nintendo SwitchコントローラーのHIDデバイスを開く
//==========================================================
bool GameController::TryOpenSwitch() {
    GUID hidGuid;
    HidD_GetHidGuid(&hidGuid);

    HDEVINFO deviceInfoSet = SetupDiGetClassDevsA(&hidGuid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (deviceInfoSet == INVALID_HANDLE_VALUE) return false;

    SP_DEVICE_INTERFACE_DATA deviceInterfaceData = {};
    deviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(deviceInfoSet, nullptr, &hidGuid, i, &deviceInterfaceData); ++i) {
        DWORD requiredSize = 0;
        SetupDiGetDeviceInterfaceDetailA(deviceInfoSet, &deviceInterfaceData, nullptr, 0, &requiredSize, nullptr);
        if (requiredSize == 0) continue;

        std::vector<uint8_t> buffer(requiredSize);
        auto pDetailData = reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA_A>(buffer.data());
        pDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);

        if (!SetupDiGetDeviceInterfaceDetailA(deviceInfoSet, &deviceInterfaceData, pDetailData, requiredSize, nullptr, nullptr)) continue;

        HANDLE hDev = CreateFileA(pDetailData->DevicePath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        if (hDev == INVALID_HANDLE_VALUE) continue;

        HIDD_ATTRIBUTES attrib = {};
        attrib.Size = sizeof(HIDD_ATTRIBUTES);

        if (HidD_GetAttributes(hDev, &attrib)) {
            // Nintendo Switch チェック
            if (attrib.VendorID == 0x057E && (attrib.ProductID == 0x2009 || attrib.ProductID == 0x2006 || attrib.ProductID == 0x2007)) {
                s_hidHandle = hDev;
                SetupDiDestroyDeviceInfoList(deviceInfoSet);
                return true;
            }
        }
        CloseHandle(hDev);
    }

    SetupDiDestroyDeviceInfoList(deviceInfoSet);
    return false;
}

//==========================================================
// ReadPS5Extended - PS5 DualSenseの拡張機能（ジャイロ・タッチパッド）読み取り
//==========================================================
bool GameController::ReadPS5Extended(GamepadState& state) {
    if (s_hidHandle == INVALID_HANDLE_VALUE) return false;

    uint8_t buffer[256] = {};
    DWORD bytesRead = 0;

    if (!ReadFile(s_hidHandle, buffer, sizeof(buffer), &bytesRead, nullptr)) {
        return false;
    }

    if (bytesRead < 64 || buffer[0] != 0x01) return false;

    state.extendedType = 2;

    // --- ジャイロ ---
    state.gyro.available = true;
    state.gyro.gyroPitch = (float)(int16_t)(buffer[16] | (buffer[17] << 8)) * 0.001064f;
    state.gyro.gyroYaw = (float)(int16_t)(buffer[18] | (buffer[19] << 8)) * 0.001064f;
    state.gyro.gyroRoll = (float)(int16_t)(buffer[20] | (buffer[21] << 8)) * 0.001064f;

    // --- タッチパッド ---
    state.touchpad.available = true;
    state.touchpad.touched = (buffer[33] & 0x80) == 0;
    if (state.touchpad.touched) {
        state.touchpad.x = (float)(buffer[34] | ((buffer[35] & 0x0F) << 8)) / 1920.0f;
        state.touchpad.y = (float)((buffer[36] << 4) | ((buffer[35] & 0xF0) >> 4)) / 1080.0f;
    }

    return true;
}

//==========================================================
// ReadPS4Extended - PS4 DualShock4の拡張機能（ジャイロ・タッチパッド）読み取り
//==========================================================
bool GameController::ReadPS4Extended(GamepadState& state) {
    if (s_hidHandle == INVALID_HANDLE_VALUE) return false;

    uint8_t buffer[256] = {};
    DWORD bytesRead = 0;

    if (!ReadFile(s_hidHandle, buffer, sizeof(buffer), &bytesRead, nullptr)) {
        return false;
    }

    if (bytesRead < 64) return false;

    state.extendedType = 1;

    // --- PS4のジャイロ（PS5とオフセットが少し違う場合がある） ---
    state.gyro.available = true;
    state.gyro.gyroPitch = (float)(int16_t)(buffer[13] | (buffer[14] << 8)) * 0.00875f;
    state.gyro.gyroYaw = (float)(int16_t)(buffer[15] | (buffer[16] << 8)) * 0.00875f;
    state.gyro.gyroRoll = (float)(int16_t)(buffer[17] | (buffer[18] << 8)) * 0.00875f;

    // --- PS4のタッチパッド ---
    state.touchpad.available = true;
    state.touchpad.touched = (buffer[35] & 0x80) == 0;
    if (state.touchpad.touched) {
        state.touchpad.x = (float)(buffer[36] | ((buffer[37] & 0x0F) << 8)) / 1920.0f;
        state.touchpad.y = (float)((buffer[38] << 4) | ((buffer[37] & 0xF0) >> 4)) / 1080.0f;
    }

    return true;
}

//==========================================================
// ReadSwitchExtended - Nintendo Switchの拡張機能（ジャイロのみ）読み取り
//==========================================================
bool GameController::ReadSwitchExtended(GamepadState& state) {
    if (s_hidHandle == INVALID_HANDLE_VALUE) return false;

    uint8_t buffer[256] = {};
    DWORD bytesRead = 0;

    if (!ReadFile(s_hidHandle, buffer, sizeof(buffer), &bytesRead, nullptr)) {
        return false;
    }

    if (bytesRead < 64) return false;

    uint8_t reportId = buffer[0];
    if (reportId != 0x30 && reportId != 0x21) return false;

    state.extendedType = 3;

    // --- Switchのジャイロ（タッチパッドはなし） ---
    state.gyro.available = true;
    int16_t gyroX = (int16_t)(buffer[19] | (buffer[20] << 8));
    int16_t gyroY = (int16_t)(buffer[21] | (buffer[22] << 8));
    int16_t gyroZ = (int16_t)(buffer[23] | (buffer[24] << 8));

    const float SWITCH_GYRO_SCALE = 0.07f;
    state.gyro.gyroPitch = gyroX * SWITCH_GYRO_SCALE;
    state.gyro.gyroYaw = gyroY * SWITCH_GYRO_SCALE;
    state.gyro.gyroRoll = gyroZ * SWITCH_GYRO_SCALE;

    // Switchにはタッチパッドなし
    state.touchpad.available = false;
    state.touchpad.touched = false;

    return true;
}
