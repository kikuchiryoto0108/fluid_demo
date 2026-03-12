//==============================================================================
//
//  インプットマネージャー [input_manager.h]
//  Author : Ryoto Kikuchi
//  Date   : 2026/1/5
//------------------------------------------------------------------------------
//
//==============================================================================
#ifndef INPUT_MANAGER_H
#define INPUT_MANAGER_H

#include <DirectXMath.h>
#include "mouse.h"
#include "keyboard.h"
#include "game_controller.h"

//==============================================================================
// 入力コマンド構造体
//------------------------------------------------------------------------------
// ゲームロジックが使用する抽象化された入力データ
//==============================================================================
struct InputCommand {
    //==========================================================================
    // 移動入力
    //==========================================================================
    bool moveLeft = false;          ///< 左移動
    bool moveRight = false;         ///< 右移動
    bool moveUp = false;            ///< 上移動（前進）
    bool moveDown = false;          ///< 下移動（後退）

    float moveAnalogX = 0.0f;       ///< アナログ移動X (-1.0 ~ 1.0)
    float moveAnalogY = 0.0f;       ///< アナログ移動Y (-1.0 ~ 1.0)

    //==========================================================================
    // アクション入力
    //==========================================================================
    bool jump = false;              ///< ジャンプ
    bool jumpTrigger = false;       ///< ジャンプ（押した瞬間）
    bool dash = false;              ///< ダッシュ
    bool attack = false;            ///< 攻撃
    bool attackTrigger = false;     ///< 攻撃（押した瞬間）

    //==========================================================================
    // システム入力
    //==========================================================================
    bool enter = false;             ///< 決定
    bool enterTrigger = false;      ///< 決定（押した瞬間）
    bool pause = false;             ///< ポーズ
    bool pauseTrigger = false;      ///< ポーズ（押した瞬間）
    bool reload = false;            ///< リロード
    bool reloadTrigger = false;     ///< リロード（押した瞬間）

    //==========================================================================
    // カメラ入力
    //==========================================================================
    float lookX = 0.0f;             ///< 視点移動X（マウス移動量 or 右スティック）
    float lookY = 0.0f;             ///< 視点移動Y

    //==========================================================================
    // マウス座標
    //==========================================================================
    DirectX::XMFLOAT2 mousePosition = { 0.0f, 0.0f };       ///< スクリーン座標
    DirectX::XMFLOAT2 mouseWorldPosition = { 0.0f, 0.0f };  ///< ワールド座標

    //==========================================================================
    // スティック生値
    //==========================================================================
    DirectX::XMFLOAT2 leftStick = { 0.0f, 0.0f };
    DirectX::XMFLOAT2 rightStick = { 0.0f, 0.0f };

    //==========================================================================
    // 攻撃方向
    //==========================================================================
    DirectX::XMFLOAT2 attackDirection = { 1.0f, 0.0f };
    bool hasAttackDirection = false;
};

//==============================================================================
// 入力タイプ
//==============================================================================
enum class InputType {
    MouseKeyboard,
    Gamepad
};

//==============================================================================
// InputManagerクラス
//==============================================================================
class InputManager {
public:
    //==========================================================================
    // シングルトンアクセス
    //==========================================================================
    static InputManager& Instance();

    //==========================================================================
    // ライフサイクル
    //==========================================================================

    /// 初期化
    /// @param hwnd ウィンドウハンドル
    void Initialize(HWND hwnd);

    /// 終了処理
    void Finalize();

    /// 更新（毎フレーム呼ぶ）
    void Update();

    //==========================================================================
    // コマンド取得
    //==========================================================================

    /// 現在の入力コマンドを取得
    const InputCommand& GetCommand() const { return m_command; }

    /// ショートカット: InputManager::Cmd()
    static const InputCommand& Cmd() { return Instance().GetCommand(); }

    //==========================================================================
    // 入力タイプ
    //==========================================================================
    InputType GetInputType() const { return m_inputType; }

    //==========================================================================
    // マウス制御
    //==========================================================================

    /// マウスロック（FPSモード）
    void SetMouseLocked(bool locked);
    bool IsMouseLocked() const { return m_mouseLocked; }

    //==========================================================================
    // バイブレーション
    //==========================================================================
    static void StartVibration(float intensity, float duration);
    static void StopVibration();
    static bool IsVibrating();

    //==========================================================================
    // デバッグ用
    //==========================================================================
    DirectX::XMFLOAT2 GetCameraDebugMove() const;
    bool IsCameraDebugMode() const { return m_cameraDebugMode; }

private:
    //==========================================================================
    // コンストラクタ（シングルトン）
    //==========================================================================
    InputManager() = default;
    ~InputManager() = default;
    InputManager(const InputManager&) = delete;
    InputManager& operator=(const InputManager&) = delete;

    //==========================================================================
    // 内部処理
    //==========================================================================
    void UpdateKeyboardInput();
    void UpdateMouseInput();
    void UpdateGamepadInput();
    void UpdateAnalogMovement();
    void UpdateTriggers();
    void UpdateAttackDirection();
    void DetectInputType();

    //==========================================================================
    // デッドゾーン適用
    //==========================================================================
    float ApplyDeadzone(float value, float deadzone) const {
        if (std::fabs(value) < deadzone) return 0.0f;
        float sign = (value > 0.0f) ? 1.0f : -1.0f;
        return sign * (std::fabs(value) - deadzone) / (1.0f - deadzone);
    }

private:
    //==========================================================================
    // 定数
    //==========================================================================
    static constexpr float STICK_DEADZONE = 0.15f;
    static constexpr float MOVE_THRESHOLD = 0.1f;
    static constexpr float MOUSE_SENSITIVITY = 0.2f;
    static constexpr float STICK_SENSITIVITY = 3.0f;

    //==========================================================================
    // 状態
    //==========================================================================
    HWND m_hwnd = nullptr;
    InputCommand m_command = {};
    InputCommand m_prevCommand = {};
    InputType m_inputType = InputType::MouseKeyboard;
    bool m_mouseLocked = false;
    bool m_cameraDebugMode = false;

    //==========================================================================
    // キャッシュ（中間計算用）
    //==========================================================================
    struct InputCache {
        // キーボード
        bool keyW = false, keyA = false, keyS = false, keyD = false;
        bool keySpace = false, keyShift = false, keyEnter = false;
        bool keyEscape = false, keyR = false;
        bool key1 = false, key2 = false, key3 = false;
        bool keyUp = false, keyDown = false, keyLeft = false, keyRight = false;

        // マウス
        bool mouseLeft = false, mouseRight = false;
        int mouseX = 0, mouseY = 0;
        int mouseDeltaX = 0, mouseDeltaY = 0;
        int mouseWheel = 0;

        // ゲームパッド
        float stickLX = 0.0f, stickLY = 0.0f;
        float stickRX = 0.0f, stickRY = 0.0f;
        bool padJump = false, padAttack = false, padDash = false;
        bool padEnter = false;
        bool dpadUp = false, dpadDown = false, dpadLeft = false, dpadRight = false;
    };
    InputCache m_cache = {};
};

#endif // INPUT_MANAGER_H