//==============================================================================
//
//  インプットマネージャー [input_manager.cpp]
//  Author : Ryoto Kikuchi
//  Date   : 2026/1/5
//------------------------------------------------------------------------------
//
//==============================================================================
#include "pch.h"
#include "input_manager.h"
#include <cstring>
#include "main.h"

//==============================================================================
// シングルトンインスタンス
//==============================================================================
InputManager& InputManager::Instance() {
    static InputManager instance;
    return instance;
}

//==============================================================================
// 初期化
//==============================================================================
void InputManager::Initialize(HWND hwnd) {
    m_hwnd = hwnd;

    // 各デバイスの初期化
    Keyboard::Instance().Initialize();
    Mouse::Instance().Initialize(hwnd);
    GameController::Initialize();

    // 状態初期化
    m_command = {};
    m_prevCommand = {};
    m_inputType = InputType::MouseKeyboard;
    m_mouseLocked = false;
    m_cameraDebugMode = false;
}

//==============================================================================
// 終了処理
//==============================================================================
void InputManager::Finalize() {
    SetMouseLocked(false);
    GameController::Finalize();
    Mouse::Instance().Finalize();
}

//==============================================================================
// 更新
//==============================================================================
void InputManager::Update() {
    // 前フレームの状態を保存
    m_prevCommand = m_command;

    // コマンドをクリア
    m_command = {};

    // 各デバイスの状態を更新
    Keyboard::Instance().Update();
    Mouse::Instance().Update();
    GameController::Update();

    // 入力を収集
    UpdateKeyboardInput();
    UpdateMouseInput();
    UpdateGamepadInput();

    // 入力タイプを検出
    DetectInputType();

    // アナログ移動量を計算
    UpdateAnalogMovement();

    // トリガー判定
    UpdateTriggers();

    // 攻撃方向を計算
    UpdateAttackDirection();
}

//==============================================================================
// キーボード入力の収集
//==============================================================================
void InputManager::UpdateKeyboardInput() {
    auto& kb = Keyboard::Instance();

    // 移動キー
    m_cache.keyW = kb.IsPressed(Key::W);
    m_cache.keyA = kb.IsPressed(Key::A);
    m_cache.keyS = kb.IsPressed(Key::S);
    m_cache.keyD = kb.IsPressed(Key::D);

    // アクションキー
    m_cache.keySpace = kb.IsPressed(Key::Space);
    m_cache.keyShift = kb.IsPressed(Key::LeftShift);
    m_cache.keyEnter = kb.IsPressed(Key::Enter);
    m_cache.keyEscape = kb.IsPressed(Key::Escape);
    m_cache.keyR = kb.IsPressed(Key::R);

    // 矢印キー（カメラ操作用）
    m_cache.keyUp = kb.IsPressed(Key::Up);
    m_cache.keyDown = kb.IsPressed(Key::Down);
    m_cache.keyLeft = kb.IsPressed(Key::Left);
    m_cache.keyRight = kb.IsPressed(Key::Right);

    // コマンドに反映
    m_command.moveUp = m_cache.keyW;
    m_command.moveDown = m_cache.keyS;
    m_command.moveLeft = m_cache.keyA;
    m_command.moveRight = m_cache.keyD;
    m_command.jump = m_cache.keySpace;
    m_command.dash = kb.IsTrigger(Key::LeftShift);
    m_command.enter = m_cache.keyEnter;
    m_command.pause = m_cache.keyEscape;
    m_command.reload = m_cache.keyR;
}

//==============================================================================
// マウス入力の収集
//==============================================================================
void InputManager::UpdateMouseInput() {
    auto& mouse = Mouse::Instance();

    // ボタン状態（これは常に取得可能）
    m_cache.mouseLeft = mouse.IsPressed(MouseButton::Left);
    m_cache.mouseRight = mouse.IsPressed(MouseButton::Right);

    // 座標とホイール
    m_cache.mouseX = mouse.GetX();
    m_cache.mouseY = mouse.GetY();
    m_cache.mouseWheel = mouse.GetScrollWheel();

    // 相対座標モードの場合、移動量を視点操作に使用
    if (mouse.GetMode() == MouseMode::Relative) {
        m_cache.mouseDeltaX = m_cache.mouseX;
        m_cache.mouseDeltaY = m_cache.mouseY;
        m_command.lookX += m_cache.mouseDeltaX * MOUSE_SENSITIVITY;
        m_command.lookY += m_cache.mouseDeltaY * MOUSE_SENSITIVITY;
    } else {
        m_cache.mouseDeltaX = 0;
        m_cache.mouseDeltaY = 0;
    }

    // 攻撃入力
    m_command.attack = m_command.attack || m_cache.mouseLeft;

    // マウス座標を保存
    m_command.mousePosition = {
        static_cast<float>(m_cache.mouseX),
        static_cast<float>(m_cache.mouseY)
    };
}

//==============================================================================
// ゲームパッド入力の収集
//==============================================================================
void InputManager::UpdateGamepadInput() {
    GamepadState pad;
    if (!GameController::GetState(pad) || !pad.connected) {
        return;
    }

    // スティック
    m_cache.stickLX = pad.leftStickX;
    m_cache.stickLY = pad.leftStickY;
    m_cache.stickRX = pad.rightStickX;
    m_cache.stickRY = pad.rightStickY;

    // D-Pad
    m_cache.dpadUp = pad.dpadUp;
    m_cache.dpadDown = pad.dpadDown;
    m_cache.dpadLeft = pad.dpadLeft;
    m_cache.dpadRight = pad.dpadRight;

    // ボタン
    m_cache.padJump = pad.buttonDown;       // A/Cross
    m_cache.padAttack = pad.buttonR2;     // X/Square
    m_cache.padEnter = pad.buttonRight;     // B/Circle
    m_cache.padDash = pad.buttonL2;

    // コマンドに反映（OR合成）
    m_command.jump = m_command.jump || GameController::IsTrigger_ButtonDown();
    m_command.attack = m_command.attack || GameController::IsTrigger_R2();
    m_command.dash = m_command.dash || GameController::IsTrigger_L2();
    m_command.enter = m_command.enter || GameController::IsTrigger_ButtonRight();
    m_command.pause = m_command.pause || GameController::IsTrigger_Start();
    m_command.reload = m_command.reload || GameController::IsTrigger_ButtonUp();

    // 移動（D-PadとスティックのOR）
    if (m_cache.dpadUp || m_cache.stickLY < -MOVE_THRESHOLD) {
        m_command.moveUp = true;
    }
    if (m_cache.dpadDown || m_cache.stickLY > MOVE_THRESHOLD) {
        m_command.moveDown = true;
    }
    if (m_cache.dpadLeft || m_cache.stickLX < -MOVE_THRESHOLD) {
        m_command.moveLeft = true;
    }
    if (m_cache.dpadRight || m_cache.stickLX > MOVE_THRESHOLD) {
        m_command.moveRight = true;
    }

    // 右スティックで視点操作
    if (std::fabs(m_cache.stickRX) > STICK_DEADZONE ||
        std::fabs(m_cache.stickRY) > STICK_DEADZONE) {
        m_command.lookX += m_cache.stickRX * STICK_SENSITIVITY;
        m_command.lookY += m_cache.stickRY * STICK_SENSITIVITY;
    }

    // スティック生値を保存
    m_command.leftStick = { m_cache.stickLX, m_cache.stickLY };
    m_command.rightStick = { m_cache.stickRX, m_cache.stickRY };
}

//==============================================================================
// 入力タイプの検出
//==============================================================================
void InputManager::DetectInputType() {
    // ゲームパッド入力があるか
    bool hasGamepadInput =
        std::fabs(m_cache.stickLX) > STICK_DEADZONE ||
        std::fabs(m_cache.stickLY) > STICK_DEADZONE ||
        std::fabs(m_cache.stickRX) > STICK_DEADZONE ||
        std::fabs(m_cache.stickRY) > STICK_DEADZONE ||
        m_cache.dpadUp || m_cache.dpadDown ||
        m_cache.dpadLeft || m_cache.dpadRight ||
        m_cache.padJump || m_cache.padAttack || m_cache.padDash;

    // キーボード/マウス入力があるか
    bool hasKBMInput =
        m_cache.keyW || m_cache.keyA || m_cache.keyS || m_cache.keyD ||
        m_cache.keySpace || m_cache.keyShift || m_cache.keyEnter ||
        m_cache.mouseLeft || m_cache.mouseRight ||
        m_cache.mouseDeltaX != 0 || m_cache.mouseDeltaY != 0;

    if (hasGamepadInput) {
        m_inputType = InputType::Gamepad;
    } else if (hasKBMInput) {
        m_inputType = InputType::MouseKeyboard;
    }
}

//==============================================================================
// アナログ移動量の計算
//==============================================================================
void InputManager::UpdateAnalogMovement() {
    m_command.moveAnalogX = 0.0f;
    m_command.moveAnalogY = 0.0f;

    if (m_inputType == InputType::Gamepad) {
        // ゲームパッド：スティック値を使用
        m_command.moveAnalogX = ApplyDeadzone(m_cache.stickLX, STICK_DEADZONE);
        m_command.moveAnalogY = ApplyDeadzone(m_cache.stickLY, STICK_DEADZONE);

        // D-Padは最大値で上書き
        if (m_cache.dpadLeft) m_command.moveAnalogX = -1.0f;
        if (m_cache.dpadRight) m_command.moveAnalogX = 1.0f;
        if (m_cache.dpadUp) m_command.moveAnalogY = -1.0f;
        if (m_cache.dpadDown) m_command.moveAnalogY = 1.0f;
    } else {
        // キーボード：デジタル値（0 or 1）
        if (m_cache.keyA) m_command.moveAnalogX -= 1.0f;
        if (m_cache.keyD) m_command.moveAnalogX += 1.0f;
        if (m_cache.keyW) m_command.moveAnalogY -= 1.0f;
        if (m_cache.keyS) m_command.moveAnalogY += 1.0f;
    }
}

//==============================================================================
// トリガー判定
//==============================================================================
void InputManager::UpdateTriggers() {
    m_command.jumpTrigger = m_command.jump && !m_prevCommand.jump;
    m_command.attackTrigger = m_command.attack && !m_prevCommand.attack;
    m_command.enterTrigger = m_command.enter && !m_prevCommand.enter;
    m_command.pauseTrigger = m_command.pause && !m_prevCommand.pause;
    m_command.reloadTrigger = m_command.reload && !m_prevCommand.reload;
}

//==============================================================================
// 攻撃方向の計算
//==============================================================================
void InputManager::UpdateAttackDirection() {
    m_command.hasAttackDirection = true;

    if (m_inputType == InputType::MouseKeyboard) {
        // マウス位置から方向を決定
        if (m_command.mousePosition.x < SCREEN_WIDTH * 0.5f) {
            m_command.attackDirection = { -1.0f, 0.0f };
        } else {
            m_command.attackDirection = { 1.0f, 0.0f };
        }
    } else {
        // ゲームパッド：左スティックから方向を決定
        if (std::fabs(m_command.leftStick.x) > STICK_DEADZONE) {
            m_command.attackDirection = (m_command.leftStick.x >= 0.0f)
                ? DirectX::XMFLOAT2{ 1.0f, 0.0f }
            : DirectX::XMFLOAT2{ -1.0f, 0.0f };
        }
    }
}

//==============================================================================
// マウスロック制御
//==============================================================================
void InputManager::SetMouseLocked(bool locked) {
    m_mouseLocked = locked;
    if (locked) {
        Mouse::Instance().SetMode(MouseMode::Relative);
        Mouse::Instance().SetVisible(false);
    } else {
        Mouse::Instance().SetMode(MouseMode::Absolute);
        Mouse::Instance().SetVisible(true);
    }
}

//==============================================================================
// カメラデバッグ移動
//==============================================================================
DirectX::XMFLOAT2 InputManager::GetCameraDebugMove() const {
    if (!m_cameraDebugMode) {
        return { 0.0f, 0.0f };
    }

    DirectX::XMFLOAT2 move{ 0.0f, 0.0f };

    if (m_cache.keyUp) move.y -= 50.0f;
    if (m_cache.keyDown) move.y += 50.0f;
    if (m_cache.keyLeft) move.x -= 50.0f;
    if (m_cache.keyRight) move.x += 50.0f;

    return move;
}

//==============================================================================
// バイブレーション
//==============================================================================
void InputManager::StartVibration(float intensity, float duration) {
    GameController::StartVibration(intensity, duration);
}

void InputManager::StopVibration() {
    GameController::StopVibration();
}

bool InputManager::IsVibrating() {
    return GameController::IsVibrating();
}