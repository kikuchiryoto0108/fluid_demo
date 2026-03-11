/*********************************************************************
 * \file   shader.hlsl
 * \brief  3D/2D共用シェーダー
 *         - VertexShaderPolygon : 頂点シェーダー
 *         - PixelShaderPolygon  : ピクセルシェーダー
 * 
 * \author Ryoto Kikuchi
 * \date   2025
 *********************************************************************/

// ==========================================================
// 定数バッファ
// ==========================================================

// --- b0: ワールドビュープロジェクション行列 ---
cbuffer ConstantBuffer : register(b0)
{
    matrix WorldViewProjection;
}

// --- b1: マテリアル情報 ---
cbuffer MaterialBuffer : register(b1)
{
    float4 Diffuse;    // offset 0
    float4 Ambient;    // offset 16
    float4 Specular;   // offset 32
    float4 Emission;   // offset 48
    float  Shininess;
    float3 Padding;
}

// ==========================================================
// テクスチャ・サンプラー
// ==========================================================
Texture2D    g_Texture      : register(t0);
SamplerState g_SamplerState : register(s0);

// ==========================================================
// VertexShaderPolygon - 頂点シェーダー
// ==========================================================
void VertexShaderPolygon(
    in  float4 inPosition  : POSITION0,
    in  float4 inNormal    : NORMAL0,
    in  float4 inDiffuse   : COLOR0,
    in  float2 inTexCoord  : TEXCOORD0,
    out float4 outPosition : SV_POSITION,
    out float4 outNormal   : NORMAL0,
    out float2 outTexCoord : TEXCOORD0,
    out float4 outDiffuse  : COLOR0)
{
    outPosition = mul(inPosition, WorldViewProjection);
    outNormal   = inNormal;
    outTexCoord = inTexCoord;
    outDiffuse  = inDiffuse * Diffuse;
}

// ==========================================================
// PixelShaderPolygon - ピクセルシェーダー
// ==========================================================
void PixelShaderPolygon(
    in  float4 inPosition : SV_POSITION,
    in  float4 inNormal   : NORMAL0,
    in  float2 inTexCoord : TEXCOORD0,
    in  float4 inDiffuse  : COLOR0,
    out float4 outDiffuse : SV_Target)
{
    outDiffuse = g_Texture.Sample(g_SamplerState, inTexCoord) * inDiffuse;
}