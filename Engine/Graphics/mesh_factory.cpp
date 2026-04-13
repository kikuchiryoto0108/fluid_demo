//==============================================================================
//  File   : mesh_factory.cpp
//  Brief  : メッシュファクトリ - プリミティブ生成とモデルファイル読み込み実装
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/12
//------------------------------------------------------------------------------
//
//==============================================================================
#include "pch.h"
#include "mesh_factory.h"
#include "texture_loader.h"
#include "Engine/Core/debug_log.h"
#include <cmath>
#include <fstream>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <unordered_map>

#pragma comment(lib, "assimp-vc143-mt.lib")

// Engine名前空間のDebugLogを使用
using Engine::DebugLog;

//==========================================================
// ヘルパー関数
//==========================================================

// ベースパスを取得するヘルパー関数
static std::string GetBasePath(const std::string& filePath) {
    size_t lastSlash = filePath.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        return filePath.substr(0, lastSlash + 1);
    }
    return "";
}

// ファイル名部分を取得（拡張子除く）
static std::string GetFileNameWithoutExtension(const std::string& filePath) {
    size_t lastSlash = filePath.find_last_of("/\\");
    size_t start = (lastSlash != std::string::npos) ? lastSlash + 1 : 0;
    size_t lastDot = filePath.find_last_of('.');
    if (lastDot != std::string::npos && lastDot > start) {
        return filePath.substr(start, lastDot - start);
    }
    return filePath.substr(start);
}

// stringをwstringに変換するヘルパー関数
static std::wstring StringToWString(const std::string& str) {
    if (str.empty()) return std::wstring();
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), nullptr, 0);
    std::wstring wstr(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wstr[0], size);
    return wstr;
}

// ファイルが存在するかチェック
static bool FileExists(const std::string& path) {
    std::ifstream f(path);
    return f.good();
}

namespace Engine {

    //==========================================================
    // CreateBox - 立方体メッシュ生成
    //==========================================================
    std::shared_ptr<Mesh> MeshFactory::CreateBox(float sizeX, float sizeY, float sizeZ) {
        auto pMesh = std::make_shared<Mesh>();

        float hx = sizeX * 0.5f;
        float hy = sizeY * 0.5f;
        float hz = sizeZ * 0.5f;

        std::vector<Vertex3D> vertices = {
            // 前面 (Z = -hz)
            {{ -hx,  hy, -hz }, { 0, 0, -1 }, { 1, 1, 1, 1 }, { 0, 0 }},
            {{  hx,  hy, -hz }, { 0, 0, -1 }, { 1, 1, 1, 1 }, { 1, 0 }},
            {{ -hx, -hy, -hz }, { 0, 0, -1 }, { 1, 1, 1, 1 }, { 0, 1 }},
            {{  hx, -hy, -hz }, { 0, 0, -1 }, { 1, 1, 1, 1 }, { 1, 1 }},
            // 背面 (Z = +hz)
            {{  hx,  hy,  hz }, { 0, 0, 1 }, { 1, 1, 1, 1 }, { 0, 0 }},
            {{ -hx,  hy,  hz }, { 0, 0, 1 }, { 1, 1, 1, 1 }, { 1, 0 }},
            {{  hx, -hy,  hz }, { 0, 0, 1 }, { 1, 1, 1, 1 }, { 0, 1 }},
            {{ -hx, -hy,  hz }, { 0, 0, 1 }, { 1, 1, 1, 1 }, { 1, 1 }},
            // 左面 (X = -hx)
            {{ -hx,  hy,  hz }, { -1, 0, 0 }, { 1, 1, 1, 1 }, { 0, 0 }},
            {{ -hx,  hy, -hz }, { -1, 0, 0 }, { 1, 1, 1, 1 }, { 1, 0 }},
            {{ -hx, -hy,  hz }, { -1, 0, 0 }, { 1, 1, 1, 1 }, { 0, 1 }},
            {{ -hx, -hy, -hz }, { -1, 0, 0 }, { 1, 1, 1, 1 }, { 1, 1 }},
            // 右面 (X = +hx)
            {{  hx,  hy, -hz }, { 1, 0, 0 }, { 1, 1, 1, 1 }, { 0, 0 }},
            {{  hx,  hy,  hz }, { 1, 0, 0 }, { 1, 1, 1, 1 }, { 1, 0 }},
            {{  hx, -hy, -hz }, { 1, 0, 0 }, { 1, 1, 1, 1 }, { 0, 1 }},
            {{  hx, -hy,  hz }, { 1, 0, 0 }, { 1, 1, 1, 1 }, { 1, 1 }},
            // 上面 (Y = +hy)
            {{ -hx,  hy,  hz }, { 0, 1, 0 }, { 1, 1, 1, 1 }, { 0, 0 }},
            {{  hx,  hy,  hz }, { 0, 1, 0 }, { 1, 1, 1, 1 }, { 1, 0 }},
            {{ -hx,  hy, -hz }, { 0, 1, 0 }, { 1, 1, 1, 1 }, { 0, 1 }},
            {{  hx,  hy, -hz }, { 0, 1, 0 }, { 1, 1, 1, 1 }, { 1, 1 }},
            // 下面 (Y = -hy)
            {{ -hx, -hy, -hz }, { 0, -1, 0 }, { 1, 1, 1, 1 }, { 0, 0 }},
            {{  hx, -hy, -hz }, { 0, -1, 0 }, { 1, 1, 1, 1 }, { 1, 0 }},
            {{ -hx, -hy,  hz }, { 0, -1, 0 }, { 1, 1, 1, 1 }, { 0, 1 }},
            {{  hx, -hy,  hz }, { 0, -1, 0 }, { 1, 1, 1, 1 }, { 1, 1 }},
        };

        std::vector<uint32_t> indices = {
            // 前面
            0, 1, 2, 2, 1, 3,
            // 背面
            4, 5, 6, 6, 5, 7,
            // 左面
            8, 9, 10, 10, 9, 11,
            // 右面
            12, 13, 14, 14, 13, 15,
            // 上面
            16, 17, 18, 18, 17, 19,
            // 下面
            20, 21, 22, 22, 21, 23,
        };

        pMesh->SetVertices(std::move(vertices));
        pMesh->SetIndices(std::move(indices));

        return pMesh;
    }

    //==========================================================
    // CreatePlane - 平面メッシュ生成
    //==========================================================
    std::shared_ptr<Mesh> MeshFactory::CreatePlane(float width, float height) {
        auto pMesh = std::make_shared<Mesh>();

        float hw = width * 0.5f;
        float hh = height * 0.5f;

        std::vector<Vertex3D> vertices = {
            {{ -hw, 0,  hh }, { 0, 1, 0 }, { 1, 1, 1, 1 }, { 0, 0 }},
            {{  hw, 0,  hh }, { 0, 1, 0 }, { 1, 1, 1, 1 }, { 1, 0 }},
            {{ -hw, 0, -hh }, { 0, 1, 0 }, { 1, 1, 1, 1 }, { 0, 1 }},
            {{  hw, 0, -hh }, { 0, 1, 0 }, { 1, 1, 1, 1 }, { 1, 1 }},
        };

        std::vector<uint32_t> indices = { 0, 1, 2, 2, 1, 3 };

        pMesh->SetVertices(std::move(vertices));
        pMesh->SetIndices(std::move(indices));

        return pMesh;
    }

    //==========================================================
    // CreateSphere - 球体メッシュ生成
    //==========================================================
    std::shared_ptr<Mesh> MeshFactory::CreateSphere(float radius, uint32_t slices, uint32_t stacks) {
        auto pMesh = std::make_shared<Mesh>();

        std::vector<Vertex3D> vertices;
        std::vector<uint32_t> indices;

        constexpr float PI = 3.14159265358979323846f;

        // 頂点生成
        for (uint32_t stack = 0; stack <= stacks; ++stack) {
            float phi = PI * static_cast<float>(stack) / static_cast<float>(stacks);
            float sinPhi = std::sin(phi);
            float cosPhi = std::cos(phi);

            for (uint32_t slice = 0; slice <= slices; ++slice) {
                float theta = 2.0f * PI * static_cast<float>(slice) / static_cast<float>(slices);
                float sinTheta = std::sin(theta);
                float cosTheta = std::cos(theta);

                Vertex3D v;
                v.normal = { cosTheta * sinPhi, cosPhi, sinTheta * sinPhi };
                v.position = { v.normal.x * radius, v.normal.y * radius, v.normal.z * radius };
                v.texCoord = { static_cast<float>(slice) / slices, static_cast<float>(stack) / stacks };
                v.color = { 1, 1, 1, 1 };

                vertices.push_back(v);
            }
        }

        // インデックス生成
        for (uint32_t stack = 0; stack < stacks; ++stack) {
            for (uint32_t slice = 0; slice < slices; ++slice) {
                uint32_t first = stack * (slices + 1) + slice;
                uint32_t second = first + slices + 1;

                indices.push_back(first);
                indices.push_back(second);
                indices.push_back(first + 1);

                indices.push_back(second);
                indices.push_back(second + 1);
                indices.push_back(first + 1);
            }
        }

        pMesh->SetVertices(std::move(vertices));
        pMesh->SetIndices(std::move(indices));

        return pMesh;
    }

    //==========================================================
    // aiMeshからEngine::Meshへ変換するヘルパー関数
    //==========================================================
    static std::shared_ptr<Mesh> ConvertAiMesh(aiMesh* aiMesh) {
        if (!aiMesh) return nullptr;

        auto pMesh = std::make_shared<Mesh>();

        // 頂点データの変換
        std::vector<Vertex3D> vertices;
        vertices.reserve(aiMesh->mNumVertices);

        for (unsigned int i = 0; i < aiMesh->mNumVertices; i++) {
            Vertex3D vertex;

            // 位置
            vertex.position = {
                aiMesh->mVertices[i].x,
                aiMesh->mVertices[i].y,
                aiMesh->mVertices[i].z
            };

            // 法線
            if (aiMesh->HasNormals()) {
                vertex.normal = {
                    aiMesh->mNormals[i].x,
                    aiMesh->mNormals[i].y,
                    aiMesh->mNormals[i].z
                };
            } else {
                vertex.normal = { 0.0f, 1.0f, 0.0f };
            }

            // テクスチャ座標（最初のUVセット）
            if (aiMesh->mTextureCoords[0]) {
                vertex.texCoord = {
                    aiMesh->mTextureCoords[0][i].x,
                    aiMesh->mTextureCoords[0][i].y
                };
            } else {
                vertex.texCoord = { 0.0f, 0.0f };
            }

            // 頂点カラー
            if (aiMesh->HasVertexColors(0)) {
                vertex.color = {
                    aiMesh->mColors[0][i].r,
                    aiMesh->mColors[0][i].g,
                    aiMesh->mColors[0][i].b,
                    aiMesh->mColors[0][i].a
                };
            } else {
                vertex.color = { 1.0f, 1.0f, 1.0f, 1.0f };
            }

            vertices.push_back(vertex);
        }

        // インデックスデータの変換
        std::vector<uint32_t> indices;
        for (unsigned int i = 0; i < aiMesh->mNumFaces; i++) {
            aiFace& face = aiMesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; j++) {
                indices.push_back(face.mIndices[j]);
            }
        }

        pMesh->SetVertices(std::move(vertices));
        pMesh->SetIndices(std::move(indices));

        return pMesh;
    }

    //==========================================================
    // CreateFromFile - ファイルからメッシュ読み込み（単一）
    //==========================================================
    std::shared_ptr<Mesh> MeshFactory::CreateFromFile(const std::string& filePath) {
        DebugLog("[MeshFactory::CreateFromFile] Loading: %s\n", filePath.c_str());
        
        Assimp::Importer importer;

        const aiScene* scene = importer.ReadFile(filePath,
            aiProcess_Triangulate |
            aiProcess_GenNormals |
            aiProcess_FlipUVs |
            aiProcess_CalcTangentSpace |
            aiProcess_JoinIdenticalVertices
        );

        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
            DebugLog("[MeshFactory::CreateFromFile] FAILED to load scene: %s\n", importer.GetErrorString());
            return nullptr;
        }

        if (scene->mNumMeshes == 0) {
            DebugLog("[MeshFactory::CreateFromFile] FAILED: No meshes found in file\n");
            return nullptr;
        }

        DebugLog("[MeshFactory::CreateFromFile] SUCCESS: Loaded %d mesh(es)\n", scene->mNumMeshes);
        return ConvertAiMesh(scene->mMeshes[0]);
    }

    //==========================================================
    // CreateAllFromFile - ファイルから全メッシュ読み込み（複数対応）
    //==========================================================
    std::vector<std::shared_ptr<Mesh>> MeshFactory::CreateAllFromFile(const std::string& filePath) {
        // OBJ,FBX読み込みをAssimpに任せる。複数メッシュ対応版。
        DebugLog("[MeshFactory::CreateAllFromFile] Loading: %s\n", filePath.c_str());
        
        std::vector<std::shared_ptr<Mesh>> meshes;

        Assimp::Importer importer;

        const aiScene* scene = importer.ReadFile(filePath,
            aiProcess_Triangulate |
            aiProcess_GenNormals |
            aiProcess_FlipUVs |
            aiProcess_CalcTangentSpace |
            aiProcess_JoinIdenticalVertices
        );

        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
            DebugLog("[MeshFactory::CreateAllFromFile] FAILED to load scene: %s\n", importer.GetErrorString());
            return meshes;
        }

        DebugLog("[MeshFactory::CreateAllFromFile] Scene loaded: %d meshes, %d materials\n", 
                 scene->mNumMeshes, scene->mNumMaterials);

        meshes.reserve(scene->mNumMeshes);

        for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
            auto pMesh = ConvertAiMesh(scene->mMeshes[i]);
            if (pMesh) {
                DebugLog("[MeshFactory::CreateAllFromFile] Mesh[%d]: %d vertices, %d indices\n",
                         i, (int)pMesh->GetVertexCount(), (int)pMesh->GetIndexCount());
                meshes.push_back(pMesh);
            }
        }

        DebugLog("[MeshFactory::CreateAllFromFile] SUCCESS: Loaded %d mesh(es)\n", (int)meshes.size());
        return meshes;
    }    // テクスチャパスのキャッシュ（同じマテリアルの重複検索を防ぐ）
    static std::unordered_map<unsigned int, std::string> s_texturePathCache;

    //==========================================================
    // マテリアルからディフューズテクスチャパスを取得（キャッシュ対応版）
    //==========================================================
    static std::string GetDiffuseTexturePath(const aiScene* scene, unsigned int materialIndex,
        const std::string& basePath, const std::string& modelFileName) {
        // キャッシュチェック
        auto it = s_texturePathCache.find(materialIndex);
        if (it != s_texturePathCache.end()) {
            return it->second;
        }

        DebugLog("[GetDiffuseTexturePath] materialIndex=%d (first lookup)\n", materialIndex);

        if (materialIndex >= scene->mNumMaterials) {
            s_texturePathCache[materialIndex] = "";
            return "";
        }

        aiMaterial* material = scene->mMaterials[materialIndex];
        std::string texPathStr;

        // ディフューズテクスチャを試行
        if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
            aiString texPath;
            if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS) {
                texPathStr = texPath.C_Str();
            }
        }

        // BASE_COLORテクスチャを試行（PBR対応）
        if (texPathStr.empty() && material->GetTextureCount(aiTextureType_BASE_COLOR) > 0) {
            aiString texPath;
            if (material->GetTexture(aiTextureType_BASE_COLOR, 0, &texPath) == AI_SUCCESS) {
                texPathStr = texPath.C_Str();
            }
        }

        std::string result;

        // テクスチャパスが見つかった場合
        if (!texPathStr.empty()) {
            for (char& c : texPathStr) {
                if (c == '\\') c = '/';
            }

            // 絶対パスチェック
            if (!texPathStr.empty() && (texPathStr[0] == '/' || texPathStr.find(':') != std::string::npos)) {
                if (FileExists(texPathStr)) {
                    result = texPathStr;
                }
            }

            if (result.empty()) {
                size_t lastSlash = texPathStr.find_last_of('/');
                std::string texFileName = (lastSlash != std::string::npos) ? texPathStr.substr(lastSlash + 1) : texPathStr;

                // 検索パス（優先順位順）
                const char* searchFolders[] = { "", "textures/", "Textures/", "texture/", "Texture/" };

                for (const char* folder : searchFolders) {
                    std::string path = basePath + folder + texFileName;
                    if (FileExists(path)) {
                        result = path;
                        break;
                    }
                }
            }
        }

        // フォールバック検索（テクスチャが見つからない場合のみ）
        if (result.empty()) {
            static const char* suffixes[] = { "_Albedo", "_albedo", "_Diffuse", "_diffuse", "_BaseColor", "_basecolor", "" };
            static const char* extensions[] = { ".png", ".jpg", ".tga", ".bmp" };
            static const char* folders[] = { "", "textures/", "Textures/" };

            bool found = false;
            for (const char* folder : folders) {
                if (found) break;
                for (const char* suffix : suffixes) {
                    if (found) break;
                    for (const char* ext : extensions) {
                        std::string candidate = basePath + folder + modelFileName + suffix + ext;
                        if (FileExists(candidate)) {
                            result = candidate;
                            found = true;
                            break;
                        }
                    }
                }
            }
        }

        // キャッシュに保存
        s_texturePathCache[materialIndex] = result;

        if (result.empty()) {
            DebugLog("[GetDiffuseTexturePath] No texture found for material %d\n", materialIndex);
        } else {
            DebugLog("[GetDiffuseTexturePath] Found: %s\n", result.c_str());
        }

        return result;
    }

    //==========================================================
    // LoadModelData - キャッシュクリア付き
    //==========================================================
    ModelData MeshFactory::LoadModelData(const std::string& filePath) {
        DebugLog("[MeshFactory::LoadModelData] Loading: %s\n", filePath.c_str());

        // 新しいモデル読み込み時にキャッシュをクリア
        s_texturePathCache.clear();

        ModelData modelData;
        modelData.basePath = GetBasePath(filePath);
        std::string modelFileName = GetFileNameWithoutExtension(filePath);

        Assimp::Importer importer;

        const aiScene* scene = importer.ReadFile(filePath,
            aiProcess_Triangulate |
            aiProcess_GenNormals |
            aiProcess_FlipUVs |
            aiProcess_CalcTangentSpace |
            aiProcess_JoinIdenticalVertices
        );

        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
            DebugLog("[MeshFactory::LoadModelData] FAILED: %s\n", importer.GetErrorString());
            return modelData;
        }

        DebugLog("[MeshFactory::LoadModelData] Scene: %d meshes, %d materials\n",
            scene->mNumMeshes, scene->mNumMaterials);

        modelData.subMeshes.reserve(scene->mNumMeshes);

        for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
            SubMeshData subMesh;
            subMesh.mesh = ConvertAiMesh(scene->mMeshes[i]);

            if (subMesh.mesh) {
                unsigned int materialIndex = scene->mMeshes[i]->mMaterialIndex;
                subMesh.diffuseTexturePath = GetDiffuseTexturePath(scene, materialIndex, modelData.basePath, modelFileName);
                modelData.subMeshes.push_back(std::move(subMesh));
            }
        }

        DebugLog("[MeshFactory::LoadModelData] SUCCESS: %d subMeshes\n", (int)modelData.subMeshes.size());
        return modelData;
    }

    //==========================================================
    // LoadModelWithTextures - 修正版
    //==========================================================
    ModelData MeshFactory::LoadModelWithTextures(const std::string& filePath, ID3D11Device* pDevice) {
        DebugLog("[MeshFactory::LoadModelWithTextures] START: %s\n", filePath.c_str());

        ModelData modelData = LoadModelData(filePath);

        if (!pDevice) {
            DebugLog("[MeshFactory::LoadModelWithTextures] FAILED: pDevice is null\n");
            return modelData;
        }

        // デフォルトの白テクスチャを1回だけ作成
        ID3D11ShaderResourceView* defaultWhite = nullptr;

        for (size_t i = 0; i < modelData.subMeshes.size(); i++) {
            auto& subMesh = modelData.subMeshes[i];

            if (!subMesh.diffuseTexturePath.empty()) {
                // テクスチャパスがある場合は読み込み
                std::wstring wPath = StringToWString(subMesh.diffuseTexturePath);
                subMesh.texture = TextureLoader::Load(pDevice, wPath);
            } else {
                // テクスチャパスがない場合は白テクスチャを使用
                if (!defaultWhite) {
                    defaultWhite = TextureLoader::CreateWhite(pDevice);
                    DebugLog("[MeshFactory::LoadModelWithTextures] Created default white texture\n");
                }
                subMesh.texture = defaultWhite;
            }

            if (subMesh.texture && subMesh.mesh) {
                subMesh.mesh->SetTexture(subMesh.texture);
            }

            // メッシュをGPUにアップロード
            if (subMesh.mesh) {
                subMesh.mesh->Upload(pDevice);
            }
        }

        DebugLog("[MeshFactory::LoadModelWithTextures] COMPLETE: %d subMeshes\n", (int)modelData.subMeshes.size());
        return modelData;
    }
}