//==============================================================================
//  File   : main.cpp
//  Brief  : メインエントリーポイント - ウィンドウ作成とメッセージループ
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/12
//------------------------------------------------------------------------------
//
//==============================================================================
#include "pch.h"
#include "main.h"
#include "Game/game_manager.h"
#include "Engine/system.h"
#include "Engine/Input/keyboard.h"
#include "Engine/Input/mouse.h"
#include "Engine/Core/timer.h"
#include <Windows.h>

//==========================================================
// ライブラリのリンク
//==========================================================
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dinput8.lib")

//==========================================================
// マクロ定義
//==========================================================
#define CLASS_NAME      "DX21 Window"
#define WINDOW_CAPTION  "3Dtest - Player1(1キー,TPS) Player2(2キー,FPS)"

//==========================================================
// プロトタイプ宣言
//==========================================================
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

//==========================================================
// GetWorldObjects - worldObjectsへのアクセス関数（既存互換）
//==========================================================
std::vector<std::shared_ptr<Game::GameObject>>& GetWorldObjects() {
    return Game::GameManager::Instance().GetWorldObjects();
}

//==========================================================
// WinMain - アプリケーションエントリーポイント
//==========================================================
int APIENTRY WinMain(HINSTANCE hInstance,
    HINSTANCE hPrevInstance, LPSTR lpCmd, int nCmdShow) {

    // --- COM初期化 ---
    HRESULT hr = CoInitializeEx(nullptr, COINITBASE_MULTITHREADED);

    // --- ウィンドウクラス登録 ---
    WNDCLASS wc;
    ZeroMemory(&wc, sizeof(WNDCLASS));
    wc.lpfnWndProc = WndProc;
    wc.lpszClassName = CLASS_NAME;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BACKGROUND + 1);
    RegisterClass(&wc);

    // --- ウィンドウサイズ調整 ---
    RECT rc = { 0, 0, 1280, 720 };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW ^ (WS_THICKFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX), FALSE);

    // --- ウィンドウ作成 ---
    HWND hWnd = CreateWindow(
        CLASS_NAME,
        WINDOW_CAPTION,
        WS_OVERLAPPEDWINDOW ^ (WS_THICKFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX),
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rc.right - rc.left,
        rc.bottom - rc.top,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    // --- エンジン初期化 ---
    if (!Engine::System::GetInstance().Initialize(hInstance, hWnd, true)) {
        return -1;
    }

    // --- ゲーム初期化（エンジンは準備完了） ---
    if (FAILED(Game::GameManager::Instance().Initialize())) {
        return -1;
    }

    // --- ウィンドウ表示 ---
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    // --- メッセージループ初期化 ---
    MSG msg;
    ZeroMemory(&msg, sizeof(MSG));

    double exec_last_time = 0.0;
    double fps_last_time = 0.0;
    double current_time = 0.0;
    ULONG frame_count = 0;
    double fps = 0.0l;

    exec_last_time = fps_last_time = SystemTimer_GetTime();

    //==========================================================
    // メインループ
    //==========================================================
    while (1) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                break;
            } else {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        } else {
            current_time = SystemTimer_GetTime();
            double elapsed_time = current_time - fps_last_time;

            // --- FPS計測（1秒ごと） ---
            if (elapsed_time >= 1.0f) {
                fps = frame_count / elapsed_time;
                fps_last_time = current_time;
                frame_count = 0;
            }

            elapsed_time = current_time - exec_last_time;

            // --- 60FPS固定更新 ---
            if (elapsed_time >= (1.0 / 60.0)) {
                exec_last_time = current_time;
                Game::GameManager::Instance().Update();
                Game::GameManager::Instance().Draw();
                keycopy();
                frame_count++;
            }
        }
    }

    //==========================================================
    // 終了処理
    //==========================================================
    Game::GameManager::Instance().Finalize();
    Engine::System::GetInstance().Finalize();
    return (int)msg.wParam;
}

//==========================================================
// WndProc - ウィンドウプロシージャ
//==========================================================
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {

        // --- キーボード・マウス共通メッセージ ---
    case WM_ACTIVATEAPP:
    case WM_SYSKEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYUP:
        Keyboard_ProcessMessage(uMsg, wParam, lParam);
        Mouse_ProcessMessage(uMsg, wParam, lParam);
        break;

        // --- キーボード入力 ---
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            SendMessage(hWnd, WM_CLOSE, 0, 0);
        }
        Keyboard_ProcessMessage(uMsg, wParam, lParam);
        break;

        // --- マウス入力 ---
    case WM_INPUT:
    case WM_MOUSEMOVE:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_MOUSEWHEEL:
    case WM_XBUTTONDOWN:
    case WM_XBUTTONUP:
    case WM_MOUSEHOVER:
        Mouse_ProcessMessage(uMsg, wParam, lParam);
        break;

        // --- ウィンドウ終了確認 ---
    case WM_CLOSE:
        if (MessageBoxA(hWnd, "Are you sure you want to quit?",
            "Confirm", MB_OKCANCEL | MB_DEFBUTTON2) == IDOK) {
            DestroyWindow(hWnd);
        } else {
            return 0;
        }
        break;

        // --- ウィンドウ破棄 ---
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}
