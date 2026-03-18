//==============================================================================
//  File   : game_manager.h
//==============================================================================
#pragma once

#include "main.h"
#include "Game/game.h"
#include <vector>
#include <memory>

namespace Engine {
    class SPHFluid;
}

namespace Game {

    class GameObject;
    class Map;
    class MapRenderer;
    class Player;

    class GameManager {
    public:
        static GameManager& Instance();

        HRESULT Initialize();
        void Finalize();
        void Update();
        void Draw();

        Game& GetGame() { return m_game; }
        const Game& GetGame() const { return m_game; }

        std::vector<std::shared_ptr<GameObject>>& GetWorldObjects();
        const std::vector<std::shared_ptr<GameObject>>& GetWorldObjects() const;

        Map* GetMap() const;
        MapRenderer* GetMapRenderer() const;
        GameObject* GetLocalPlayerGameObject() const;

        double GetFPS() const { return m_fps; }

        Engine::SPHFluid* GetFluid() { return m_fluid.get(); }
        void SpawnFluidParticles(const DirectX::XMFLOAT3& position, uint32_t count, float radius);

    private:
        GameManager();
        ~GameManager();
        GameManager(const GameManager&) = delete;
        GameManager& operator=(const GameManager&) = delete;

        Game m_game;
        double m_fps = 0.0;
        uint32_t m_inputSeq = 0;

        std::unique_ptr<Engine::SPHFluid> m_fluid;

        static std::vector<std::shared_ptr<GameObject>> s_emptyWorldObjects;
    };

} // namespace Game
