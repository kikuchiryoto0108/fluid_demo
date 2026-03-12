//==============================================================================
//
//  ゲームコントローラー入力管理（SDL3版） [game_controller.cpp]
//  Author : Ryoto Kikuchi
//  Date   : 2026/1/5
//------------------------------------------------------------------------------
//
//==============================================================================
#include "pch.h"
#include "game_controller.h"
#include <algorithm>

//==============================================================================
// 静的メンバ変数の定義
//==============================================================================
SDL_Gamepad* GameController::s_gamepad = nullptr;
SDL_JoystickID GameController::s_gamepadID = 0;
GamepadState GameController::s_currentState = {};
GamepadState GameController::s_prevState = {};
bool GameController::s_isVibrating = false;
Uint64 GameController::s_vibrationEndTime = 0;
bool GameController::s_sdlInitialized = false;

//==============================================================================
// 初期化
//==============================================================================
bool GameController::Initialize() {
    s_gamepad = nullptr;
    s_gamepadID = 0;
    s_currentState = {};
    s_prevState = {};
    s_isVibrating = false;
    s_vibrationEndTime = 0;

    // SDL Gamepad サブシステムを初期化
    // 注意: 他でSDL_Init済みの場合は SDL_INIT_GAMEPAD のみ追加
    if (!SDL_WasInit(SDL_INIT_GAMEPAD)) {
        if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD)) {
            SDL_Log("SDL_InitSubSystem(SDL_INIT_GAMEPAD) failed: %s", SDL_GetError());
            return false;
        }
    }
    s_sdlInitialized = true;

    // 接続されているゲームパッドを探す
    TryOpenGamepad();

    return true;
}

//==============================================================================
// ゲームパッドを開く
//==============================================================================
void GameController::TryOpenGamepad() {
    if (s_gamepad) return;  // 既に開いている

    // 接続されているゲームパッドのリストを取得
    int count = 0;
    SDL_JoystickID* gamepads = SDL_GetGamepads(&count);

    if (gamepads && count > 0) {
        // 最初のゲームパッドを開く
        s_gamepad = SDL_OpenGamepad(gamepads[0]);
        if (s_gamepad) {
            s_gamepadID = gamepads[0];
            SDL_Log("Gamepad connected: %s", SDL_GetGamepadName(s_gamepad));
        }
    }

    SDL_free(gamepads);
}

//==============================================================================
// 終了処理
//==============================================================================
void GameController::Finalize() {
    StopVibration();

    if (s_gamepad) {
        SDL_CloseGamepad(s_gamepad);
        s_gamepad = nullptr;
        s_gamepadID = 0;
    }

    if (s_sdlInitialized) {
        SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
        s_sdlInitialized = false;
    }

    s_currentState = {};
    s_prevState = {};
}

//==============================================================================
// 更新（毎フレーム呼び出し）
//==============================================================================
void GameController::Update() {
    // SDLイベントを更新（ゲームパッドの接続/切断を検出）
    SDL_UpdateGamepads();

    // ゲームパッドが切断された場合
    if (s_gamepad && !SDL_GamepadConnected(s_gamepad)) {
        SDL_Log("Gamepad disconnected");
        SDL_CloseGamepad(s_gamepad);
        s_gamepad = nullptr;
        s_gamepadID = 0;
    }

    // ゲームパッドが接続されていなければ再接続を試みる
    if (!s_gamepad) {
        TryOpenGamepad();
    }

    UpdateState();

    // バイブレーションの時間管理
    if (s_isVibrating) {
        Uint64 now = SDL_GetTicks();
        if (now >= s_vibrationEndTime) {
            StopVibration();
        }
    }
}

//==============================================================================
// 状態更新（内部処理）
//==============================================================================
bool GameController::UpdateState() {
    // 前フレームの状態を保存
    s_prevState = s_currentState;

    // ゲームパッドが接続されていない場合
    if (!s_gamepad) {
        s_currentState.connected = false;
        return false;
    }

    s_currentState.connected = true;

    //==========================================================================
    // 十字キー
    //==========================================================================
    s_currentState.dpadUp = SDL_GetGamepadButton(s_gamepad, SDL_GAMEPAD_BUTTON_DPAD_UP);
    s_currentState.dpadDown = SDL_GetGamepadButton(s_gamepad, SDL_GAMEPAD_BUTTON_DPAD_DOWN);
    s_currentState.dpadLeft = SDL_GetGamepadButton(s_gamepad, SDL_GAMEPAD_BUTTON_DPAD_LEFT);
    s_currentState.dpadRight = SDL_GetGamepadButton(s_gamepad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT);

    //==========================================================================
    // メインボタン（SDL3では SOUTH/EAST/WEST/NORTH で位置ベース指定）
    // SOUTH=下(A), EAST=右(B), WEST=左(X), NORTH=上(Y)
    //==========================================================================
    s_currentState.buttonDown = SDL_GetGamepadButton(s_gamepad, SDL_GAMEPAD_BUTTON_SOUTH);
    s_currentState.buttonRight = SDL_GetGamepadButton(s_gamepad, SDL_GAMEPAD_BUTTON_EAST);
    s_currentState.buttonLeft = SDL_GetGamepadButton(s_gamepad, SDL_GAMEPAD_BUTTON_WEST);
    s_currentState.buttonUp = SDL_GetGamepadButton(s_gamepad, SDL_GAMEPAD_BUTTON_NORTH);

    //==========================================================================
    // ショルダーボタン
    //==========================================================================
    s_currentState.buttonL1 = SDL_GetGamepadButton(s_gamepad, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
    s_currentState.buttonR1 = SDL_GetGamepadButton(s_gamepad, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);

    //==========================================================================
    // スティック押し込み
    //==========================================================================
    s_currentState.buttonL3 = SDL_GetGamepadButton(s_gamepad, SDL_GAMEPAD_BUTTON_LEFT_STICK);
    s_currentState.buttonR3 = SDL_GetGamepadButton(s_gamepad, SDL_GAMEPAD_BUTTON_RIGHT_STICK);

    //==========================================================================
    // システムボタン
    //==========================================================================
    s_currentState.buttonStart = SDL_GetGamepadButton(s_gamepad, SDL_GAMEPAD_BUTTON_START);
    s_currentState.buttonSelect = SDL_GetGamepadButton(s_gamepad, SDL_GAMEPAD_BUTTON_BACK);

    //==========================================================================
    // トリガー（アナログ値: 0 ~ 32767）
    //==========================================================================
    Sint16 leftTriggerRaw = SDL_GetGamepadAxis(s_gamepad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
    Sint16 rightTriggerRaw = SDL_GetGamepadAxis(s_gamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);

    // 0.0 ~ 1.0 に正規化（トリガーは0から始まる）
    s_currentState.leftTrigger = leftTriggerRaw / 32767.0f;
    s_currentState.rightTrigger = rightTriggerRaw / 32767.0f;

    // デジタルボタンとしての判定（50%以上で押下扱い）
    s_currentState.buttonL2 = (s_currentState.leftTrigger > 0.5f);
    s_currentState.buttonR2 = (s_currentState.rightTrigger > 0.5f);

    //==========================================================================
    // スティック（-32768 ~ 32767）
    //==========================================================================
    Sint16 leftX = SDL_GetGamepadAxis(s_gamepad, SDL_GAMEPAD_AXIS_LEFTX);
    Sint16 leftY = SDL_GetGamepadAxis(s_gamepad, SDL_GAMEPAD_AXIS_LEFTY);
    Sint16 rightX = SDL_GetGamepadAxis(s_gamepad, SDL_GAMEPAD_AXIS_RIGHTX);
    Sint16 rightY = SDL_GetGamepadAxis(s_gamepad, SDL_GAMEPAD_AXIS_RIGHTY);

    // -1.0 ~ 1.0 に正規化
    float rawLeftX = NormalizeAxisValue(leftX);
    float rawLeftY = NormalizeAxisValue(leftY);
    float rawRightX = NormalizeAxisValue(rightX);
    float rawRightY = NormalizeAxisValue(rightY);

    // デッドゾーン適用（SDLは上がマイナスなのでY軸はそのまま）
    s_currentState.leftStickX = GamepadState::ApplyDeadzone(rawLeftX);
    s_currentState.leftStickY = GamepadState::ApplyDeadzone(rawLeftY);
    s_currentState.rightStickX = GamepadState::ApplyDeadzone(rawRightX);
    s_currentState.rightStickY = GamepadState::ApplyDeadzone(rawRightY);

    return true;
}

//==============================================================================
// 軸の値を正規化（-1.0 ~ 1.0）
//==============================================================================
float GameController::NormalizeAxisValue(Sint16 value) {
    if (value >= 0) {
        return value / 32767.0f;
    } else {
        return value / 32768.0f;
    }
}

//==============================================================================
// バイブレーション開始
//==============================================================================
void GameController::StartVibration(float intensity, float duration) {
    if (!s_gamepad) return;

    // 強度を0.0~1.0にクランプ
    intensity = std::max(0.0f, std::min(1.0f, intensity));

    // SDL_RumbleGamepad は 0 ~ 65535 の範囲
    Uint16 motorStrength = static_cast<Uint16>(intensity * 65535.0f);
    Uint32 durationMs = static_cast<Uint32>(duration * 1000.0f);

    // バイブレーション実行
    // 引数: ゲームパッド, 低周波モーター, 高周波モーター, 継続時間(ms)
    if (SDL_RumbleGamepad(s_gamepad, motorStrength, motorStrength, durationMs)) {
        s_isVibrating = true;
        s_vibrationEndTime = SDL_GetTicks() + durationMs;
    } else {
        // バイブレーション非対応のコントローラー
        SDL_Log("Rumble not supported: %s", SDL_GetError());
    }
}

//==============================================================================
// バイブレーション停止
//==============================================================================
void GameController::StopVibration() {
    if (s_gamepad) {
        SDL_RumbleGamepad(s_gamepad, 0, 0, 0);
    }
    s_isVibrating = false;
}

//==============================================================================
// バイブレーション中かどうか
//==============================================================================
bool GameController::IsVibrating() {
    return s_isVibrating;
}

//==============================================================================
// コントローラー名取得（デバッグ用）
//==============================================================================
const char* GameController::GetControllerName() {
    if (s_gamepad) {
        return SDL_GetGamepadName(s_gamepad);
    }
    return "No gamepad";
}