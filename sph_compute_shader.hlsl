//==============================================================================
//  File   : sph_fluid.hlsl
//  Brief  : SPH流体パーティクル描画シェーダー
//==============================================================================

// 定数バッファ
cbuffer CBCamera : register(b0) {
    float4x4 View;
    float4x4 Projection;
    float3 CameraPosition;
    float padding;
};

// 頂点入力
struct VS_INPUT {
    float3 Position : POSITION;
    float4 Color : COLOR;
    float Size : SIZE;
};

// ジオメトリシェーダー出力
struct GS_OUTPUT {
    float4 Position : SV_Position;
    float4 Color : COLOR;
    float2 TexCoord : TEXCOORD;
};

//==========================================================
// 頂点シェーダー（パススルー）
//==========================================================
VS_INPUT VS_Main(VS_INPUT input) {
    return input;
}

//==========================================================
// ジオメトリシェーダー（ビルボード生成）
//==========================================================
[maxvertexcount(4)]
void GS_Main(point VS_INPUT input[1], inout TriangleStream<GS_OUTPUT> stream) {
    float3 pos = input[0].Position;
    float size = input[0].Size;
    float4 color = input[0].Color;
    
    // カメラ向きのビルボード軸を計算
    float3 up = float3(0, 1, 0);
    float3 look = normalize(CameraPosition - pos);
    float3 right = normalize(cross(up, look));
    up = cross(look, right);
    
    // 4頂点生成
    float3 offsets[4] = {
        float3(-1, -1, 0),
        float3(-1,  1, 0),
        float3( 1, -1, 0),
        float3( 1,  1, 0)
    };
    float2 uvs[4] = {
        float2(0, 1),
        float2(0, 0),
        float2(1, 1),
        float2(1, 0)
    };
    
    [unroll]
    for (int i = 0; i < 4; i++) {
        GS_OUTPUT output;
        float3 worldPos = pos + (right * offsets[i].x + up * offsets[i].y) * size;
        float4 viewPos = mul(float4(worldPos, 1.0), View);
        output.Position = mul(viewPos, Projection);
        output.Color = color;
        output.TexCoord = uvs[i];
        stream.Append(output);
    }
}

//==========================================================
// ピクセルシェーダー（球状グラデーション）
//==========================================================
float4 PS_Main(GS_OUTPUT input) : SV_Target {
    // 中心からの距離で球状に見せる
    float2 center = input.TexCoord - 0.5;
    float dist = length(center) * 2.0;
    
    // 円の外側は透明
    if (dist > 1.0) discard;
    
    // ソフトエッジ
    float alpha = 1.0 - smoothstep(0.5, 1.0, dist);
    
    float4 color = input.Color;
    color.a *= alpha;
    
    return color;
}