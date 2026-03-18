//==============================================================================
// 深度パス：パーティクルを深度（カメラからの距離）として描画
//==============================================================================

cbuffer CBMatrix : register(b0) {
    float4x4 WorldViewProjection;
};

cbuffer CBFluid : register(b2) {
    float4x4 View;
    float4x4 Projection;
    float PointRadius;
    float3 Padding;
};

struct VS_INPUT {
    float3 Position : POSITION;
    float3 Normal   : NORMAL;
    float4 Color    : COLOR;
    float2 TexCoord : TEXCOORD;
};

struct VS_OUTPUT {
    float4 Position : SV_Position;
    float2 TexCoord : TEXCOORD0;
    float Depth     : TEXCOORD1;
};

VS_OUTPUT VS_Depth(VS_INPUT input) {
    VS_OUTPUT output;
    output.Position = mul(float4(input.Position, 1.0), WorldViewProjection);
    output.TexCoord = input.TexCoord;
    
    // ビュー空間での深度
    float4 viewPos = mul(float4(input.Position, 1.0), View);
    output.Depth = -viewPos.z;  // カメラからの距離
    
    return output;
}

float4 PS_Depth(VS_OUTPUT input) : SV_Target {
    // UV座標を中心基準に変換
    float2 uv = input.TexCoord * 2.0 - 1.0;
    float dist = length(uv);
    
    // 円の外側は描画しない
    if (dist > 1.0) discard;
    
    // 球面の深度オフセット
    float z = sqrt(1.0 - dist * dist);
    float depth = input.Depth - z * PointRadius;
    
    return float4(depth, 0, 0, 1);
}