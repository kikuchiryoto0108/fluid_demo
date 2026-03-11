//==============================================================================
//  File   : model_data.h
//  Brief  : モデルデータ - 複数サブメッシュとテクスチャを管理
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/12
//------------------------------------------------------------------------------
//
//==============================================================================
#pragma once

#include "mesh.h"
#include <memory>
#include <vector>
#include <string>
#include <d3d11.h>

namespace Engine {

    //==========================================================
    // サブメッシュデータ（メッシュ + 対応するテクスチャパス）
    //==========================================================
    struct SubMeshData {
        std::shared_ptr<Mesh> mesh;              // メッシュ本体
        std::string diffuseTexturePath;          // ディフューズテクスチャのパス
        ID3D11ShaderResourceView* texture = nullptr;  // 読み込まれたテクスチャ
    };

    //==========================================================
    // モデル全体のデータ（複数サブメッシュ + テクスチャ）
    //==========================================================
    struct ModelData {
        std::vector<SubMeshData> subMeshes;      // サブメッシュ配列
        std::string basePath;                    // モデルファイルのベースパス（テクスチャ相対パス解決用）

        // --- テクスチャを解放 ---
        void ReleaseTextures() {
            for (auto& subMesh : subMeshes) {
                if (subMesh.texture) {
                    subMesh.texture->Release();
                    subMesh.texture = nullptr;
                }
            }
        }

        // --- 全メッシュをGPUにアップロード ---
        bool UploadAll(ID3D11Device* pDevice) {
            for (auto& subMesh : subMeshes) {
                if (subMesh.mesh && !subMesh.mesh->Upload(pDevice)) {
                    return false;
                }
            }
            return true;
        }

        // --- テクスチャが1つ以上あるか ---
        bool HasTextures() const {
            for (const auto& subMesh : subMeshes) {
                if (subMesh.texture != nullptr) {
                    return true;
                }
            }
            return false;
        }
    };

}
