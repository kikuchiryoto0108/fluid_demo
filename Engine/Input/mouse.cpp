/*********************************************************************
 * \file   mouse.cpp
 * \brief  マウス入力モジュール実装
 *
 * \author Ryoto Kikuchi
 * \date   2026/3/10
 *********************************************************************/
#include "pch.h"
#include "mouse.h"
#include <windowsx.h>
#include <cassert>
#include <cstring>

 //==============================================================================
 // シングルトンインスタンス
 //==============================================================================
Mouse& Mouse::Instance() {
    static Mouse instance;
    return instance;
}

//==============================================================================
// 初期化
//------------------------------------------------------------------------------
// Raw Input デバイスを登録して高精度なマウス移動量を取得可能にする
//==============================================================================
void Mouse::Initialize(HWND window) {
    assert(window != nullptr);

    m_window = window;
    m_currentState = {};
    m_previousState = {};
    m_lastAbsX = 0;
    m_lastAbsY = 0;
    m_relativeX = INT32_MAX;
    m_relativeY = INT32_MAX;
    m_inFocus = true;

    //--------------------------------------------------------------------------
    // Raw Input デバイス登録
    // 相対座標モードで高精度なマウス移動量を取得するために必要
    //--------------------------------------------------------------------------
    RAWINPUTDEVICE rid = {};
    rid.usUsagePage = 0x01;          // HID_USAGE_PAGE_GENERIC
    rid.usUsage = 0x02;              // HID_USAGE_GENERIC_MOUSE
    rid.dwFlags = RIDEV_INPUTSINK;   // バックグラウンドでも受信
    rid.hwndTarget = window;
    RegisterRawInputDevices(&rid, 1, sizeof(RAWINPUTDEVICE));

    //--------------------------------------------------------------------------
    // イベントハンドル作成
    // スレッド間でモード切替を安全に行うための同期オブジェクト
    //--------------------------------------------------------------------------
    if (!m_scrollWheelEvent) {
        m_scrollWheelEvent = CreateEventEx(
            nullptr, nullptr,
            CREATE_EVENT_MANUAL_RESET,
            EVENT_MODIFY_STATE | SYNCHRONIZE
        );
    }
    if (!m_relativeReadEvent) {
        m_relativeReadEvent = CreateEventEx(
            nullptr, nullptr,
            CREATE_EVENT_MANUAL_RESET,
            EVENT_MODIFY_STATE | SYNCHRONIZE
        );
    }
    if (!m_absoluteModeEvent) {
        m_absoluteModeEvent = CreateEventEx(
            nullptr, nullptr,
            0,
            EVENT_MODIFY_STATE | SYNCHRONIZE
        );
    }
    if (!m_relativeModeEvent) {
        m_relativeModeEvent = CreateEventEx(
            nullptr, nullptr,
            0,
            EVENT_MODIFY_STATE | SYNCHRONIZE
        );
    }
}

//==============================================================================
// 終了処理
//==============================================================================
void Mouse::Finalize() {
    auto safeClose = [](HANDLE& h) {
        if (h) {
            CloseHandle(h);
            h = nullptr;
        }
        };

    safeClose(m_scrollWheelEvent);
    safeClose(m_relativeReadEvent);
    safeClose(m_absoluteModeEvent);
    safeClose(m_relativeModeEvent);
}

//==============================================================================
// 更新
//==============================================================================
void Mouse::Update() {
    // 前フレームの状態を保存
    m_previousState = m_currentState;

    // ホイールリセットイベントチェック
    if (WaitForSingleObjectEx(m_scrollWheelEvent, 0, FALSE) == WAIT_OBJECT_0) {
        m_currentState.scrollWheel = 0;
        ResetEvent(m_scrollWheelEvent);
    }

    // 相対座標モードの読み取り完了チェック
    if (m_currentState.mode == MouseMode::Relative) {
        if (WaitForSingleObjectEx(m_relativeReadEvent, 0, FALSE) == WAIT_OBJECT_0) {
            // 読み取り済みなら座標をリセット
            m_currentState.x = 0;
            m_currentState.y = 0;
        } else {
            // 未読み取りならイベントをセット
            SetEvent(m_relativeReadEvent);
        }
    }
}

//==============================================================================
// モード切替イベント処理
//==============================================================================
void Mouse::ProcessModeSwitch() {
    HANDLE events[] = { m_scrollWheelEvent, m_absoluteModeEvent, m_relativeModeEvent };
    DWORD result = WaitForMultipleObjectsEx(3, events, FALSE, 0, FALSE);

    switch (result) {
    case WAIT_OBJECT_0:
        // ホイールリセット
        m_currentState.scrollWheel = 0;
        ResetEvent(m_scrollWheelEvent);
        break;

    case WAIT_OBJECT_0 + 1:
        //----------------------------------------------------------------------
        // 絶対座標モードに切替
        //----------------------------------------------------------------------
        m_currentState.mode = MouseMode::Absolute;
        ClipCursor(nullptr);  // カーソルクリップ解除
        ShowCursor(TRUE);      // カーソル表示

        // 保存していた座標にカーソルを戻す
        {
            POINT pt = { m_lastAbsX, m_lastAbsY };
            if (MapWindowPoints(m_window, nullptr, &pt, 1)) {
                SetCursorPos(pt.x, pt.y);
            }
        }
        m_currentState.x = m_lastAbsX;
        m_currentState.y = m_lastAbsY;
        break;

    case WAIT_OBJECT_0 + 2:
        //----------------------------------------------------------------------
        // 相対座標モードに切替
        //----------------------------------------------------------------------
        ResetEvent(m_relativeReadEvent);
        m_currentState.mode = MouseMode::Relative;
        m_currentState.x = 0;
        m_currentState.y = 0;
        m_relativeX = INT32_MAX;
        m_relativeY = INT32_MAX;
        ShowCursor(FALSE);     // カーソル非表示
        ClipToWindow();        // カーソルをウィンドウ内にクリップ
        break;
    }
}

//==============================================================================
// Windowsメッセージ処理
//==============================================================================
void Mouse::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    // モード切替イベントをチェック
    ProcessModeSwitch();

    switch (message) {
        //--------------------------------------------------------------------------
        // ウィンドウアクティブ状態変化
        //--------------------------------------------------------------------------
    case WM_ACTIVATEAPP:
        if (wParam) {
            // アクティブになった
            m_inFocus = true;
            if (m_currentState.mode == MouseMode::Relative) {
                m_currentState.x = 0;
                m_currentState.y = 0;
                ShowCursor(FALSE);
                ClipToWindow();
            }
        } else {
            // 非アクティブになった
            int wheel = m_currentState.scrollWheel;
            m_currentState = {};
            m_currentState.scrollWheel = wheel;  // ホイール値は保持
            m_inFocus = false;
        }
        return;

        //--------------------------------------------------------------------------
        // Raw Input（相対座標モード用）
        // 高精度なマウス移動量を取得
        //--------------------------------------------------------------------------
    case WM_INPUT:
        if (m_inFocus && m_currentState.mode == MouseMode::Relative) {
            RAWINPUT raw = {};
            UINT rawSize = sizeof(raw);
            GetRawInputData(
                reinterpret_cast<HRAWINPUT>(lParam),
                RID_INPUT,
                &raw,
                &rawSize,
                sizeof(RAWINPUTHEADER)
            );

            if (raw.header.dwType == RIM_TYPEMOUSE) {
                if (!(raw.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE)) {
                    // 相対座標（通常のマウス）
                    m_currentState.x = raw.data.mouse.lLastX;
                    m_currentState.y = raw.data.mouse.lLastY;
                    ResetEvent(m_relativeReadEvent);
                } else if (raw.data.mouse.usFlags & MOUSE_VIRTUAL_DESKTOP) {
                    // 絶対座標（リモートデスクトップ等）
                    // 移動量を計算
                    int width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
                    int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
                    int x = static_cast<int>((raw.data.mouse.lLastX / 65535.0f) * width);
                    int y = static_cast<int>((raw.data.mouse.lLastY / 65535.0f) * height);

                    if (m_relativeX == INT32_MAX) {
                        m_currentState.x = 0;
                        m_currentState.y = 0;
                    } else {
                        m_currentState.x = x - m_relativeX;
                        m_currentState.y = y - m_relativeY;
                    }
                    m_relativeX = x;
                    m_relativeY = y;
                    ResetEvent(m_relativeReadEvent);
                }
            }
        }
        return;

        //--------------------------------------------------------------------------
        // ボタン操作
        //--------------------------------------------------------------------------
    case WM_LBUTTONDOWN:
        m_currentState.buttons[static_cast<int>(MouseButton::Left)] = true;
        break;
    case WM_LBUTTONUP:
        m_currentState.buttons[static_cast<int>(MouseButton::Left)] = false;
        break;
    case WM_RBUTTONDOWN:
        m_currentState.buttons[static_cast<int>(MouseButton::Right)] = true;
        break;
    case WM_RBUTTONUP:
        m_currentState.buttons[static_cast<int>(MouseButton::Right)] = false;
        break;
    case WM_MBUTTONDOWN:
        m_currentState.buttons[static_cast<int>(MouseButton::Middle)] = true;
        break;
    case WM_MBUTTONUP:
        m_currentState.buttons[static_cast<int>(MouseButton::Middle)] = false;
        break;

        //--------------------------------------------------------------------------
        // ホイール
        //--------------------------------------------------------------------------
    case WM_MOUSEWHEEL:
        m_currentState.scrollWheel += GET_WHEEL_DELTA_WPARAM(wParam);
        return;

        //--------------------------------------------------------------------------
        // 拡張ボタン（サイドボタン）
        //--------------------------------------------------------------------------
    case WM_XBUTTONDOWN:
        if (GET_XBUTTON_WPARAM(wParam) == XBUTTON1)
            m_currentState.buttons[static_cast<int>(MouseButton::X1)] = true;
        else if (GET_XBUTTON_WPARAM(wParam) == XBUTTON2)
            m_currentState.buttons[static_cast<int>(MouseButton::X2)] = true;
        break;
    case WM_XBUTTONUP:
        if (GET_XBUTTON_WPARAM(wParam) == XBUTTON1)
            m_currentState.buttons[static_cast<int>(MouseButton::X1)] = false;
        else if (GET_XBUTTON_WPARAM(wParam) == XBUTTON2)
            m_currentState.buttons[static_cast<int>(MouseButton::X2)] = false;
        break;

        //--------------------------------------------------------------------------
        // マウス移動
        //--------------------------------------------------------------------------
    case WM_MOUSEMOVE:
    case WM_MOUSEHOVER:
        break;

    default:
        return;
    }

    //--------------------------------------------------------------------------
    // 絶対座標モードの場合、座標を更新
    //--------------------------------------------------------------------------
    if (m_currentState.mode == MouseMode::Absolute) {
        m_currentState.x = GET_X_LPARAM(lParam);
        m_currentState.y = GET_Y_LPARAM(lParam);
        m_lastAbsX = m_currentState.x;
        m_lastAbsY = m_currentState.y;
    }
}

//==============================================================================
// ボタン状態取得
//==============================================================================
bool Mouse::IsPressed(MouseButton button) const {
    return m_currentState.buttons[static_cast<int>(button)];
}

bool Mouse::IsTrigger(MouseButton button) const {
    int idx = static_cast<int>(button);
    return m_currentState.buttons[idx] && !m_previousState.buttons[idx];
}

bool Mouse::IsRelease(MouseButton button) const {
    int idx = static_cast<int>(button);
    return !m_currentState.buttons[idx] && m_previousState.buttons[idx];
}

//==============================================================================
// その他
//==============================================================================
void Mouse::ResetScrollWheel() {
    SetEvent(m_scrollWheelEvent);
}

Mouse::State Mouse::GetState() const {
    State state = m_currentState;

    // ホイールリセット待ちの場合は0を返す
    if (WaitForSingleObjectEx(m_scrollWheelEvent, 0, FALSE) == WAIT_OBJECT_0) {
        state.scrollWheel = 0;
    }

    // 相対座標読み取り済みの場合は0を返す
    if (state.mode == MouseMode::Relative) {
        if (WaitForSingleObjectEx(m_relativeReadEvent, 0, FALSE) == WAIT_OBJECT_0) {
            state.x = 0;
            state.y = 0;
        }
    }

    return state;
}

void Mouse::SetMode(MouseMode mode) {
    if (m_currentState.mode == mode) return;

    // モード切替イベントをセット
    SetEvent(mode == MouseMode::Absolute ? m_absoluteModeEvent : m_relativeModeEvent);

    // マウスホバーイベントを発生させてモード切替を処理させる
    TRACKMOUSEEVENT tme = {};
    tme.cbSize = sizeof(tme);
    tme.dwFlags = TME_HOVER;
    tme.hwndTrack = m_window;
    tme.dwHoverTime = 1;
    TrackMouseEvent(&tme);
}

void Mouse::SetVisible(bool visible) {
    // Relativeモードでは常に非表示
    if (m_currentState.mode == MouseMode::Relative) return;

    CURSORINFO info = { sizeof(CURSORINFO) };
    GetCursorInfo(&info);

    bool isVisible = (info.flags & CURSOR_SHOWING) != 0;
    if (isVisible != visible) {
        ShowCursor(visible);
    }
}

bool Mouse::IsVisible() const {
    if (m_currentState.mode == MouseMode::Relative) return false;

    CURSORINFO info = { sizeof(CURSORINFO) };
    GetCursorInfo(&info);
    return (info.flags & CURSOR_SHOWING) != 0;
}

bool Mouse::IsConnected() const {
    return GetSystemMetrics(SM_MOUSEPRESENT) != 0;
}

void Mouse::ClipToWindow() {
    assert(m_window != nullptr);

    RECT rect;
    GetClientRect(m_window, &rect);

    POINT ul = { rect.left, rect.top };
    POINT lr = { rect.right, rect.bottom };

    MapWindowPoints(m_window, nullptr, &ul, 1);
    MapWindowPoints(m_window, nullptr, &lr, 1);

    rect.left = ul.x;
    rect.top = ul.y;
    rect.right = lr.x;
    rect.bottom = lr.y;

    ClipCursor(&rect);
}
