//==============================================================================
//  File   : map.cpp
//  Brief  : 3次元マップデータ管理 - ブロックの生成と衝突判
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/12
//------------------------------------------------------------------------------
//
//==============================================================================
#include "pch.h"
#include "map.h"
#include "Game/Objects/game_object.h"
#include "map_renderer.h"
#include "Engine/Graphics/primitive.h"

namespace Game {

//==========================================================
// ヘルパー関数
//==========================================================

// --- Clamp関数（値を範囲内に制限） ---
template<typename T>
T Clamp(T value, T minValue, T maxValue)
{
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}

//==========================================================
// コンストラクタ
//==========================================================
Map::Map() {
    // マップデータを初期化（全て0で初期化）
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int z = 0; z < MAP_DEPTH; z++) {
            for (int x = 0; x < MAP_WIDTH; x++) {
                mapData[y][z][x] = 0;
            }
        }
    }
}

//==========================================================
// デストラクタ
//==========================================================
Map::~Map() {
}

//==========================================================
// 初期化処理
//==========================================================
HRESULT Map::Initialize(ID3D11ShaderResourceView* texture) {
    // サンプルマップデータを作成
    CreateSampleMap();
    GenerateBlockObjects(texture);

    return S_OK;
}

//==========================================================
// 終了処理
//==========================================================
void Map::Uninitialize()
{
    // 特に処理なし
}

//==========================================================
// ブロックデータの取得
//==========================================================
int Map::GetBlock(int x, int y, int z) const
{
    if (!IsValidPosition(x, y, z)) {
        return 0;  // 範囲外は0を返す
    }
    return mapData[y][z][x];
}

//==========================================================
// ブロックデータの設定
//==========================================================
void Map::SetBlock(int x, int y, int z, int value)
{
    if (IsValidPosition(x, y, z)) {
        mapData[y][z][x] = value;
    }
}

//==========================================================
// 位置の有効性チェック
//==========================================================
bool Map::IsValidPosition(int x, int y, int z) const
{
    return (x >= 0 && x < MAP_WIDTH &&
            y >= 0 && y < MAP_HEIGHT &&
            z >= 0 && z < MAP_DEPTH);
}

//==========================================================
// サンプルマップデータの作成
//==========================================================
void Map::CreateSampleMap() {
    // --- 50x50x50のサンプルマップデータ ---

    // 底面層（y=0）- 全面の床
    for (int z = 0; z < MAP_DEPTH; z++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            mapData[0][z][x] = 1;
        }
    }

    // 中間層（y=1,2,3）- 部分的な壁と構造物
    for (int y = 1; y < MAP_HEIGHT - 1; y++) {
        // 四隅の柱
        mapData[y][0][0] = 1;
        mapData[y][0][MAP_WIDTH - 1] = 1;
        mapData[y][MAP_DEPTH - 1][0] = 1;
        mapData[y][MAP_DEPTH - 1][MAP_WIDTH - 1] = 1;

        // 中央の構造物
        if (y == 1 || y == 2) {
            mapData[y][2][2] = 1;
        }

        if (y == 2) {
            mapData[y][1][2] = 1;
            mapData[y][3][2] = 1;
            mapData[y][2][1] = 1;
            mapData[y][2][3] = 1;
        }
    }

    // 最上層
    for (int z = 0; z < MAP_DEPTH; z++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            if ((x == 0 || x == MAP_WIDTH - 1) && (z == 0 || z == MAP_DEPTH - 1)) {
                mapData[MAP_HEIGHT - 1][z][x] = 1;
            }
        }
    }

    // ★★★ 中央に2x2の高さ1の囲い ★★★
    // マップ中央 = (25, 25) → 底面(y=0)の上、y=1に壁を作る
    // 内側空間: x=24,25 z=24,25 （2x2）
    // 壁は外周: x=23〜26, z=23〜26 の枠
    int cx = MAP_WIDTH / 2;   // 25
    int cz = MAP_DEPTH / 2;   // 25
    int wallY = 1;             // 底面の1つ上

    // 前壁 (z = cz-2 = 23): x = cx-2 〜 cx+1 (23〜26)
    for (int x = cx - 2; x <= cx + 1; x++) {
        mapData[wallY][cz - 2][x] = 1;
    }
    // 後壁 (z = cz+1 = 26)
    for (int x = cx - 2; x <= cx + 1; x++) {
        mapData[wallY][cz + 1][x] = 1;
    }
    // 左壁 (x = cx-2 = 23): z = cz-1 〜 cz (24〜25) ※角は前後壁で配置済み
    for (int z = cz - 1; z <= cz; z++) {
        mapData[wallY][z][cx - 2] = 1;
    }
    // 右壁 (x = cx+1 = 26)
    for (int z = cz - 1; z <= cz; z++) {
        mapData[wallY][z][cx + 1] = 1;
    }
}


//==========================================================
// 地面の高さを取得
//==========================================================
float Map::GetGroundHeight(float worldX, float worldZ) const
{
    float offsetX = -(MAP_WIDTH - 1) * BOX_SIZE * 0.5f;
    float offsetZ = -(MAP_DEPTH - 1) * BOX_SIZE * 0.5f;

    int mapX = static_cast<int>((worldX - offsetX) / BOX_SIZE);
    int mapZ = static_cast<int>((worldZ - offsetZ) / BOX_SIZE);

#if __cplusplus >= 201703L
    mapX = std::clamp(mapX, 0, MAP_WIDTH - 1);
    mapZ = std::clamp(mapZ, 0, MAP_DEPTH - 1);
#else
    mapX = Clamp(mapX, 0, MAP_WIDTH - 1);
    mapZ = Clamp(mapZ, 0, MAP_DEPTH - 1);
#endif

    for (int y = MAP_HEIGHT - 1; y >= 0; --y) {
        if (mapData[y][mapZ][mapX] == 1) {
            float offsetY = -(MAP_HEIGHT - 1) * BOX_SIZE * 0.5f;
            return y * BOX_SIZE + offsetY + BOX_SIZE * 0.5f;
        }
    }

    return 0.0f;
}

//==========================================================
// ブロックGameObject生成
//==========================================================
void Map::GenerateBlockObjects(ID3D11ShaderResourceView* texture)
{
    blockObjects.clear();
    float offsetX = -(MAP_WIDTH - 1) * BOX_SIZE * 0.5f;
    float offsetY = -(MAP_HEIGHT - 1) * BOX_SIZE * 0.5f;
    float offsetZ = -(MAP_DEPTH - 1) * BOX_SIZE * 0.5f;
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int z = 0; z < MAP_DEPTH; z++) {
            for (int x = 0; x < MAP_WIDTH; x++) {
                if (mapData[y][z][x] == 1) {
                    auto obj = std::make_shared<GameObject>();
                    obj->position = XMFLOAT3(x * BOX_SIZE + offsetX, y * BOX_SIZE + offsetY, z * BOX_SIZE + offsetZ);
                    obj->scale = XMFLOAT3(BOX_SIZE, BOX_SIZE, BOX_SIZE);
                    obj->rotation = XMFLOAT3(0, 0, 0);
                    obj->setMesh(Box, 36, texture);
                    obj->setBoxCollider(obj->scale);
                    blockObjects.push_back(std::move(obj));
                }
            }
        }
    }
}

} // namespace Game
