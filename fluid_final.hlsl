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
    
    // 水がない場所はスキップ
    if (depth <= 0) {
        return SceneTexture.Sample(LinearSampler, input.TexCoord);
    }
    
    // 法線計算
    float3 normal = ComputeNormal(input.TexCoord);
    
    // ビュー方向（簡易）
    float3 viewDir = float3(0, 0, 1);
    
    // フレネル効果
    float fresnel = pow(1.0 - saturate(dot(normal, viewDir)), FresnelPower);
    
    // ライティング
    float3 lightDir = normalize(LightDir);
    float diffuse = max(dot(normal, lightDir), 0.0) * 0.5 + 0.5;
    
    // スペキュラ
    float3 halfVec = normalize(lightDir + viewDir);
    float specular = pow(max(dot(normal, halfVec), 0.0), 64.0);
    
    // 屈折（簡易：法線でUVをずらす）
    float2 refractUV = input.TexCoord + normal.xy * 0.02;
    float3 refractColor = SceneTexture.Sample(LinearSampler, refractUV).rgb;
    
    // 反射色（環境マップがないので空色）
    float3 reflectColor = float3(0.6, 0.8, 1.0);
    
    // 水の色
    float3 waterBase = WaterColor * diffuse;
    
    // 最終合成
    float3 finalColor = lerp(refractColor * waterBase, reflectColor, fresnel * 0.3);
    finalColor += specular * 0.5;
    
    return float4(finalColor, WaterAlpha);
}