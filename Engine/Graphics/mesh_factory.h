//==============================================================================
//  File   : mesh_factory.h
//  Brief  : メッシュファクトリ - プリミティブ生成とモデルファイル読み込み
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/12
//------------------------------------------------------------------------------
//
//==============================================================================
#pragma once

#include "mesh.h"
#include "model_data.h"
#include <memory>
#include <vector>

namespace Engine {

    //==========================================================
    // メッシュファクトリクラス
    //==========================================================
    class MeshFactory {
    public:
        // --- プリミティブ生成 ---
        static std::shared_ptr<Mesh> CreateBox(float sizeX = 1.0f, float sizeY = 1.0f, float sizeZ = 1.0f);
        static std::shared_ptr<Mesh> CreatePlane(float width = 1.0f, float height = 1.0f);
        static std::shared_ptr<Mesh> CreateSphere(float radius = 0.5f, uint32_t slices = 16, uint32_t stacks = 16);

        // --- ファイルから読み込み（単一メッシュ） ---
        static std::shared_ptr<Mesh> CreateFromFile(const std::string& filePath);

        // --- 複数メッシュ対応版 ---
        static std::vector<std::shared_ptr<Mesh>> CreateAllFromFile(const std::string& filePath);

        // --- モデルデータ読み込み（メッシュ + テクスチャパス情報） ---
        static ModelData LoadModelData(const std::string& filePath);

        // --- モデルデータを読み込み、テクスチャも一緒に読み込む ---
        static ModelData LoadModelWithTextures(const std::string& filePath, ID3D11Device* pDevice);

    private:
        MeshFactory() = delete;
    };
}
