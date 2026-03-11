//==============================================================================
//  File   : main.h
//  Brief  : 共通ヘッダー - 全体で使用するインクルードとマクロ定義
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/11
//------------------------------------------------------------------------------
//
//==============================================================================
#pragma once

#pragma warning(push)
#pragma warning(disable:4005)

#define _CRT_SECURE_NO_WARNINGS  // scanfのwarning防止
#include <stdio.h>

//==========================================================
// Winsock2ヘッダー（windows.hより先にインクルード必須）
//==========================================================
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

//==========================================================
// DirectXヘッダー
//==========================================================
#include <d3d11.h>
#include <d3dcompiler.h>

#define DIRECTINPUT_VERSION 0x0800  // 警告対策
#include "dinput.h"
#include "mmsystem.h"

#pragma warning(pop)

#include <DirectXMath.h>
using namespace DirectX;

//==========================================================
// マクロ定義
//==========================================================
static constexpr int SCREEN_WIDTH(1920);   // ウインドウの幅
static constexpr int SCREEN_HEIGHT(1080);  // ウインドウの高さ
