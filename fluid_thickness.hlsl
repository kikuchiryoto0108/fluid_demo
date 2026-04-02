// ===== fluif_thickness.hlsl =====

cbuffer CBCamera : register(b0)
{
    float4x4 View;
    float4x4 Proj;
    float    PointRadius;
    float2   ScreenSize;
    float    padding0;
};

struct VS_INPUT
{
    float2 QuadPos : POSITION;
    float3 InstPos : INST_POS;
};

struct VS_OUTPUT
{
    float4 Position     : SV_POSITION;
    float2 TexCoord     : TEXCOORD0;
    float3 EyeSpacePos  : TEXCOORD1;
    float  SphereRadius : TEXCOORD2;
};

// Thickness uses the same vertex shader as depth
VS_OUTPUT VS_Depth(VS_INPUT input)
{
    VS_OUTPUT output;
    float3 eyePos = mul(float4(input.InstPos, 1.0), View).xyz;
    float3 viewPos = eyePos + float3(input.QuadPos * PointRadius, 0.0);
    output.Position     = mul(float4(viewPos, 1.0), Proj);
    output.TexCoord     = input.QuadPos;
    output.EyeSpacePos  = eyePos;
    output.SphereRadius = PointRadius;
    return output;
}

struct PS_THICKNESS_OUT
{
    float Thickness : SV_Target0;
};

PS_THICKNESS_OUT PS_Thickness(VS_OUTPUT input)
{
    PS_THICKNESS_OUT output;

    float3 N;
    N.xy = input.TexCoord;
    float r2 = dot(N.xy, N.xy);
    if (r2 > 1.0) discard;
    N.z = sqrt(1.0 - r2);

    // 厚みは加算ブレンドされるので、ある程度の値を出力
    output.Thickness = N.z * 0.4;
    return output;
}