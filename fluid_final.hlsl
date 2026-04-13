cbuffer CBFinal : register(b0)
{
    float4x4 InvProj;
    float2   TexelSize;
    float    WaterAlpha;
    float    FresnelPower;
    float3   WaterColor;
    float    padding2;
    float3   WaterDeepColor;
    float    padding3;
    float3   LightDir;
    float    SpecPower;
    float3   AbsorptionCoeff;
    float    RefractScale;
};

Texture2D<float> BlurredDepth : register(t0);
Texture2D<float> ThicknessTex : register(t1);
SamplerState     LinearSamp   : register(s0);

struct VS_QUAD_OUT
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

float3 UVToEye(float2 uv, float depth)
{
    float4 clipPos = float4(uv * 2.0 - 1.0, depth, 1.0);
    clipPos.y = -clipPos.y;
    float4 viewPos = mul(clipPos, InvProj);
    return viewPos.xyz / viewPos.w;
}

float3 GetEyePos(float2 uv)
{
    float d = BlurredDepth.SampleLevel(LinearSamp, uv, 0);
    return UVToEye(uv, d);
}

float4 PS_Final(VS_QUAD_OUT input) : SV_Target0
{
    float depth = BlurredDepth.SampleLevel(LinearSamp, input.TexCoord, 0);
    float thickness = ThicknessTex.SampleLevel(LinearSamp, input.TexCoord, 0);

    // ★ 空判定を反転（1.0が空）
    if (depth > 0.9999 && thickness < 0.001)
        return float4(0, 0, 0, 0);

    if (depth > 0.9999)
    {
        return float4(0.15, 0.35, 0.6, thickness * 0.3);
    }

    float3 posEye = UVToEye(input.TexCoord, depth);

    // 法線計算
    float3 ddxPos = GetEyePos(input.TexCoord + float2(TexelSize.x, 0)) - posEye;
    float3 ddx2   = posEye - GetEyePos(input.TexCoord - float2(TexelSize.x, 0));
    if (abs(ddxPos.z) > abs(ddx2.z)) ddxPos = ddx2;

    float3 ddyPos = GetEyePos(input.TexCoord + float2(0, TexelSize.y)) - posEye;
    float3 ddy2   = posEye - GetEyePos(input.TexCoord - float2(0, TexelSize.y));
    if (abs(ddyPos.z) > abs(ddy2.z)) ddyPos = ddy2;

    float3 N = normalize(cross(ddxPos, ddyPos));
    
    if (any(isnan(N)))
        N = float3(0, 0, 1);

    float3 V = normalize(-posEye);
    float3 L = normalize(float3(0.3, 0.8, -0.5));
    float3 H = normalize(L + V);

    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float NdotH = max(dot(N, H), 0.0);

    // フレネル（控えめに）
    float fresnel = 0.02 + 0.15 * pow(1.0 - NdotV, 3.0);

    // スペキュラ
    float spec = pow(NdotH, 128.0) * 2.0;
    float specSoft = pow(NdotH, 24.0) * 0.3;

    // リムライト（控えめに、透明度に影響させる）
    float rim = pow(1.0 - NdotV, 4.0);

    // 水の色
    float3 shallow = float3(0.25, 0.55, 0.9);
    float3 deep = float3(0.08, 0.2, 0.45);
    float depthFactor = saturate(thickness * 2.5);
    float3 waterCol = lerp(shallow, deep, depthFactor);

    // ディフューズ
    float diffuse = NdotL * 0.4 + 0.6;

    // 最終色（透明度は色ではなく透明度で表現）
    float3 color = waterCol * diffuse * 0.6
                 + float3(1, 1, 1) * spec
                 + float3(0.8, 0.9, 1.0) * specSoft
                 + waterCol * fresnel * 0.3
                 + float3(0.05, 0.08, 0.12);

    // アルファ（水は透明に）
    float alpha = saturate(depthFactor * 0.5 + 0.3);
    alpha *= (1.0 - rim * 0.5);  // 端を透明に
    alpha = clamp(alpha, 0.2, 0.8);

    return float4(color, alpha);
}