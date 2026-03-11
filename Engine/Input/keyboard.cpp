/*********************************************************************
 * \file   keyboard.cpp
 * \brief  キーボード入力モジュール - DirectXTK改変版
 * 
 * \author Ryoto Kikuchi
 * \date   2020/06/07
 * 
 * DirectXTKより、なんちゃってC言語用にシェイプアップ改変
 * Licensed under the MIT License.
 * http://go.microsoft.com/fwlink/?LinkId=248929
 * http://go.microsoft.com/fwlink/?LinkID=615561
 *********************************************************************/
#include "pch.h"
#include "keyboard.h"

#include <assert.h>

static_assert(sizeof(Keyboard_State) == 256 / 8, "キーボード状態構造体のサイズ不一致");


//==========================================================
// 静的変数
//==========================================================
static Keyboard_State gState = {};
static Keyboard_State gStateOld = {};


//==========================================================
// keycopy - 前回のキー情報を保存
//==========================================================
void keycopy()
{
    gStateOld = gState;
}

//==========================================================
// keyDown - キー押下状態の設定
//==========================================================
static void keyDown(int key)
{
    if (key < 0 || key > 0xfe) { return; }

    unsigned int* p = (unsigned int*)&gState;
    unsigned int bf = 1u << (key & 0x1f);
 
    p[(key >> 5)] |= bf;
}


//==========================================================
// keyUp - キー解放状態の設定
//==========================================================
static void keyUp(int key)
{
    if (key < 0 || key > 0xfe) { return; }

    unsigned int* p = (unsigned int*)&gState;
    unsigned int bf = 1u << (key & 0x1f);
    p[(key >> 5)] &= ~bf;
}


//==========================================================
// Keyboard_Initialize - キーボードモジュールの初期化
//==========================================================
void Keyboard_Initialize(void)
{
    Keyboard_Reset();
}


//==========================================================
// Keyboard_IsKeyDown - 指定された状態でキーが押されているか確認
//==========================================================
bool Keyboard_IsKeyDown(Keyboard_Keys key, const Keyboard_State* pState)
{
    if (key <= 0xfe)
    {
        unsigned int* p = (unsigned int*)pState;
        unsigned int bf = 1u << (key & 0x1f);
        return (p[(key >> 5)] & bf) != 0;
    }
    return false;
}

//==========================================================
// Keyboard_IsKeyDownTrigger - キーが押された瞬間を検出
//==========================================================
bool Keyboard_IsKeyDownTrigger(Keyboard_Keys key)
{
    if (key <= 0xfe)
    {
        unsigned int* p = (unsigned int*)&gState;
        unsigned int* p2 = (unsigned int*)&gStateOld;

        unsigned int bf = 1u << (key & 0x1f);

        return ((p[(key >> 5)] & bf) ^ (p2[(key >> 5)] & bf)) & (p[(key >> 5)] & bf);
    }
    return false;
}


//==========================================================
// Keyboard_IsKeyUp - 指定された状態でキーが離されているか確認
//==========================================================
bool Keyboard_IsKeyUp(Keyboard_Keys key, const Keyboard_State* pState)
{
    if (key <= 0xfe)
    {
        unsigned int* p = (unsigned int*)pState;
        unsigned int bf = 1u << (key & 0x1f);
        return (p[(key >> 5)] & bf) == 0;
    }
    return false;
}


//==========================================================
// Keyboard_IsKeyDown - 現在の状態でキーが押されているか確認
//==========================================================
bool Keyboard_IsKeyDown(Keyboard_Keys key)
{
    return Keyboard_IsKeyDown(key, &gState);
}


//==========================================================
// Keyboard_IsKeyUp - 現在の状態でキーが離されているか確認
//==========================================================
bool Keyboard_IsKeyUp(Keyboard_Keys key)
{
    return Keyboard_IsKeyUp(key, &gState);
}


//==========================================================
// Keyboard_GetState - キーボードの現在の状態を取得する
//==========================================================
const Keyboard_State* Keyboard_GetState(void)
{
    return &gState;
}

//==========================================================
// Keyboard_GetStateOld - キーボードの前フレームの状態を取得する
//==========================================================
const Keyboard_State* Keyboard_GetStateOld(void)
{
    return &gStateOld;
}


//==========================================================
// Keyboard_Reset - キーボードの状態をリセットする
//==========================================================
void Keyboard_Reset(void)
{
    ZeroMemory(&gState, sizeof(Keyboard_State));
    ZeroMemory(&gStateOld, sizeof(Keyboard_State));
}


//==========================================================
// Keyboard_ProcessMessage - ウィンドウメッセージプロシージャフック関数
//==========================================================
void Keyboard_ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    bool down = false;

    switch (message)
    {
    case WM_ACTIVATEAPP:
        Keyboard_Reset();
        return;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        down = true;
        break;

    case WM_KEYUP:
    case WM_SYSKEYUP:
        break;

    default:
        return;
    }

    int vk = (int)wParam;
    switch (vk)
    {
    case VK_SHIFT:
        vk = (int)MapVirtualKey(((unsigned int)lParam & 0x00ff0000) >> 16u, MAPVK_VSC_TO_VK_EX);
        if (!down)
        {
            // 左シフトと右シフトの両方が同時に押された場合にクリアされるようにするための回避策
            keyUp(VK_LSHIFT);
            keyUp(VK_RSHIFT);
        }
        break;

    case VK_CONTROL:
        vk = ((UINT)lParam & 0x01000000) ? VK_RCONTROL : VK_LCONTROL;
        break;

    case VK_MENU:
        vk = ((UINT)lParam & 0x01000000) ? VK_RMENU : VK_LMENU;
        break;
    }

    if (down)
    {
        keyDown(vk);
    }
    else
    {
        keyUp(vk);
    }
}
