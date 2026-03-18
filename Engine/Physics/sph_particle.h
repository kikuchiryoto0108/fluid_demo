//==============================================================================
//  File   : sph_particle.h
//  Brief  : SPH流体パーティクル構造体
// 
//  Author : Ryoto Kikuchi
//  Date   : 2026/3/17
//==============================================================================
#pragma once

#include <DirectXMath.h>
#include <cstdint>

namespace Engine {
    using namespace DirectX;

    //==========================================================
    // SPHパーティクル構造体
    //==========================================================
    struct SPHParticle {
        XMFLOAT3 position;
        XMFLOAT3 velocity;
        XMFLOAT3 acceleration;
        float    density;
        float    pressure;
        float    mass;

        SPHParticle()
            : position(0.0f, 0.0f, 0.0f)
            , velocity(0.0f, 0.0f, 0.0f)
            , acceleration(0.0f, 0.0f, 0.0f)
            , density(1000.0f)
            , pressure(0.0f)
            , mass(1.0f) {
        }
    };

    //==========================================================
    // SPHパラメータ
    //==========================================================
    struct SPHParams {
        float smoothingRadius;
        float restDensity;
        float gasConstant;
        float viscosity;
        XMFLOAT3 gravity;
        float deltaTime;
        XMFLOAT3 boundaryMin;
        XMFLOAT3 boundaryMax;
        uint32_t particleCount;
    };
}
