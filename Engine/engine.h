//==============================================================================
//  File   : engine.h
//  Brief  : エンジン統一ヘッダー - 全エンジンモジュールの一括インクルード
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/12
//------------------------------------------------------------------------------
// ゲームコード側でこのヘッダーをインクルードするだけで
// エンジンの全機能にアクセスできる便利ヘッダー。
// 各サブシステムのヘッダーを個別にインクルードする必要がない。
//==============================================================================
#pragma once

//==========================================================
// Core - エンジンコア機能
// --- レンダラー、タイマー、基本型定義など
//==========================================================
#include "Engine/Core/renderer.h"
#include "Engine/Core/timer.h"
#include "Engine/Core/types.h"

//==========================================================
// Input - 入力システム
// --- キーボード、マウス、ゲームパッド入力の統一管理
//==========================================================
#include "Engine/Input/input_manager.h"
#include "Engine/Input/keyboard.h"
#include "Engine/Input/mouse.h"
#include "Engine/Input/game_controller.h"

//==========================================================
// Graphics - グラフィックス描画
// --- 頂点、マテリアル、メッシュ、スプライト、プリミティブ描画
//==========================================================
#include "Engine/Graphics/vertex.h"
#include "Engine/Graphics/material.h"
#include "Engine/Graphics/mesh.h"
#include "Engine/Graphics/mesh_factory.h"
#include "Engine/Graphics/primitive.h"
#include "Engine/Graphics/sprite_2d.h"
#include "Engine/Graphics/sprite_3d.h"
#include "Engine/Graphics/texture_loader.h"

//==========================================================
// Collision - 衝突判定システム
// --- ボックス、球体コライダーおよびマップ衝突判定
//==========================================================
#include "Engine/Collision/collider.h"
#include "Engine/Collision/box_collider.h"
#include "Engine/Collision/sphere_collider.h"
#include "Engine/Collision/collision_system.h"
#include "Engine/Collision/map_collision.h"
#include "Engine/Collision/collision_manager.h"

//==========================================================
// System - システム統括
// --- 全サブシステムの初期化・更新・終了を一元管理
//==========================================================
#include "Engine/system.h"
