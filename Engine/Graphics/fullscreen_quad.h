//==============================================================================
//  File   : fullscreen_quad.h
//  Brief  : ポストプロセス用フルスクリーン四角形
//==============================================================================
#pragma once

#include <d3d11.h>
#include <wrl/client.h>

namespace Engine {
    using Microsoft::WRL::ComPtr;

    class FullscreenQuad {
    public:
        bool Initialize(ID3D11Device* device);
        void Draw(ID3D11DeviceContext* context);

    private:
        ComPtr<ID3D11Buffer> m_pVertexBuffer;
    };
}
