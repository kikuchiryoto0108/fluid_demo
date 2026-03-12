//==============================================================================
//  File   : game.h
//  Brief  : ゲームシーン管理 - シーンの基底クラスと各種シーン定義
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/11
//------------------------------------------------------------------------------
//
//==============================================================================
#pragma once

#include "main.h"
#include "Game/Objects/destructible_wall.h" 
#include <vector>
#include <memory>

namespace Game {

    class GameObject;
    class Map;
    class MapRenderer;

//==========================================================
// シーン識別用列挙型
//==========================================================
enum class SceneType {
    NONE,       // 未設定
    TITLE,      // タイトル画面
    GAME,       // ゲームプレイ
    RESULT,     // リザルト画面
    MAX         // 最大値
};

//==========================================================
// シーン基底クラス
//==========================================================
class Scene {
public:
    Scene() : m_sceneType(SceneType::NONE) {}
    virtual ~Scene() = default;

    // --- ライフサイクル関数（純粋仮想） ---
    virtual HRESULT Initialize() = 0;
    virtual void    Finalize() = 0;
    virtual void    Update() = 0;
    virtual void    Draw() = 0;

    // --- アクセサ ---
    SceneType GetSceneType() const { return m_sceneType; }

protected:
    SceneType m_sceneType;
};

//==========================================================
// ゲームプレイシーン
//==========================================================
class SceneGame : public Scene {
public:
    SceneGame();
    ~SceneGame() override;

    // --- ライフサイクル関数 ---
    HRESULT Initialize() override;
    void    Finalize()   override;
    void    Update()     override;
    void    Draw()       override;

    // --- ゲーム固有のアクセサ ---
    Map* GetMap()         const { return m_pMap; }
    MapRenderer* GetMapRenderer() const { return m_pMapRenderer; }
    std::vector<std::shared_ptr<GameObject>>& GetWorldObjects() { return m_worldObjects; }
    const std::vector<std::shared_ptr<GameObject>>& GetWorldObjects() const { return m_worldObjects; }

    // 破壊可能な壁へのアクセス
    DestructibleWall* GetDestructibleWall() { return &m_destructibleWall; }
private:
    Map* m_pMap = nullptr;                  // マップデータ
    MapRenderer* m_pMapRenderer = nullptr;  // マップ描画システム
    std::vector<std::shared_ptr<GameObject>> m_worldObjects;    // ワールドオブジェクト

    // 破壊可能な壁
    DestructibleWall m_destructibleWall;

    // --- HP表示UIの描画（画面左上にHPバー＋テキスト） ---
    void DrawHPDisplay();
};

//==========================================================
// タイトルシーン（将来の拡張用スタブ）
//==========================================================
class SceneTitle : public Scene {
public:
    SceneTitle();
    ~SceneTitle() override;

    HRESULT Initialize() override;
    void    Finalize()   override;
    void    Update()     override;
    void    Draw()       override;
};

//==========================================================
// リザルトシーン（将来の拡張用スタブ）
//==========================================================
class SceneResult : public Scene {
public:
    SceneResult();
    ~SceneResult() override;

    HRESULT Initialize() override;
    void    Finalize()   override;
    void    Update()     override;
    void    Draw()       override;
};

//==========================================================
// シーン遷移管理クラス
//==========================================================
class Game {
public:
    Game();
    ~Game();

    // --- ライフサイクル関数 ---
    HRESULT Initialize(SceneType firstScene);
    void    Finalize();
    void    Update();
    void    Draw();

    // --- シーン遷移 ---
    void RequestSceneChange(SceneType nextScene);

    // --- アクセサ ---
    SceneType  GetCurrentSceneType() const { return m_currentSceneType; }
    Scene* GetCurrentScene()     const { return m_pCurrentScene.get(); }
    SceneGame* GetSceneGame()        const;

private:
    // --- シーン遷移処理 ---
    void ExecuteSceneChange();
    std::unique_ptr<Scene> CreateScene(SceneType type);

    // --- メンバ変数 ---
    std::unique_ptr<Scene> m_pCurrentScene;         // 現在のシーン
    SceneType m_currentSceneType = SceneType::NONE; // 現在のシーンタイプ
    SceneType m_nextSceneType = SceneType::NONE;    // 遷移先シーンタイプ
    bool      m_sceneChangeRequested = false;       // 遷移リクエストフラグ
};

} // namespace Game
