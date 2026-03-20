//==============================================================================
// 最終パス：深度から法線を計算し、水面をシェーディング
//==============================================================================

Texture2D DepthTexture : register(t0);
Texture2D SceneTexture : register(t1);  // 元のシーン
SamplerState LinearSampler : register(s0);

cbuffer CBFinal : register(b0) {
    float4x4 InvProjection;
    float2 TexelSize;
    float2 Padding;
    float3 WaterColor;
    float WaterAlpha;
    float3 LightDir;
    float FresnelPower;
};

struct VS_OUTPUT {
    float4 Position : SV_Position;
    float2 TexCoord : TEXCOORD;
};

VS_OUTPUT VS_Final(float3 pos : POSITION, float2 uv : TEXCOORD) {
    VS_OUTPUT output;
    output.Position = float4(pos, 1.0);
    output.TexCoord = uv;
    return output;
}

// 深度から法線を計算
float3 ComputeNormal(float2 uv) {
    float depth = DepthTexture.Sample(LinearSampler, uv).r;
    float depthL = DepthTexture.Sample(LinearSampler, uv - float2(TexelSize.x, 0)).r;
    float depthR = DepthTexture.Sample(LinearSampler, uv + float2(TexelSize.x, 0)).r;
    float depthT = DepthTexture.Sample(LinearSampler, uv - float2(0, TexelSize.y)).r;
    float depthB = DepthTexture.Sample(LinearSampler, uv + float2(0, TexelSize.y)).r;
    
    float3 normal;
    normal.x = (depthL - depthR) * 0.5;
    normal.y = (depthT - depthB) * 0.5;
    normal.z = 1.0;
    
    return normalize(normal);
}

float4 PS_Final(VS_OUTPUT input) : SV_Target {
    float depth = DepthTexture.Sample(LinearSampler, input.TexCoord).r;
    float thickness = ThicknessTexture.Sample(LinearSampler, input.TexCoord).r;
    float4 sceneColor = SceneTexture.Sample(LinearSampler, input.TexCoord);
    
    if (depth <= 0.001 || depth >= 0.999) {
        return sceneColor;
    }
    
    // 法線計算（より滑らかに）
    float3 normal = ComputeNormal(input.TexCoord);
    float3 viewDir = float3(0, 0, 1);
    
    // フレネル効果（強め）
    float fresnel = pow(1.0 - saturate(dot(normal, viewDir)), FresnelPower);
    
    // ライティング
    float3 lightDir = normalize(LightDir);
    float diffuse = max(dot(normal, lightDir), 0.0) * 0.3 + 0.7;
    
    // スペキュラ（強いハイライト）
    float3 halfVec = normalize(lightDir + viewDir);
    float specular = pow(max(dot(normal, halfVec), 0.0), 128.0);
    
    // 屈折（強め）
    float refractionStrength = 0.04;
    float2 refractOffset = normal.xy * refractionStrength;
    float2 refractUV = clamp(input.TexCoord + refractOffset, 0.01, 0.99);
    float3 refractColor = SceneTexture.Sample(LinearSampler, refractUV).rgb;
    
    // 厚みによる色の変化（より強調）
    float thicknessFactor = saturate(thickness * 20.0);
    
    // 水の色（深い青）
    float3 deepWater = float3(0.02, 0.15, 0.4);
    float3 shallowWater = float3(0.3, 0.6, 0.9);
    float3 waterBase = lerp(shallowWater, deepWater, thicknessFactor) * diffuse;
    
    // 背景との合成
    float alpha = lerp(0.4, 0.95, thicknessFactor);
    float3 blendedColor = lerp(refractColor, waterBase, alpha);
    
    // 反射（空の色っぽく）
    float3 reflectColor = float3(0.7, 0.85, 1.0);
    blendedColor = lerp(blendedColor, reflectColor, fresnel * 0.5);
    
    // スペキュラ追加（キラキラ）
    blendedColor += specular * float3(1.0, 1.0, 1.0) * 0.8;
    
    return float4(blendedColor, 1.0);
}
