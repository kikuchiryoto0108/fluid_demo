cbuffer CBCamera : register(b0)
{
    float4x4 View;
    float4x4 Proj;
    float PointRadius;
    float2 ScreenSize;
    float padding0;
};

struct VS_INPUT
{
    float2 QuadPos  : POSITION;
    float3 InstPos  : INST_POS;
};

struct VS_OUTPUT
{
    float4 Position     : SV_POSITION;
    float2 TexCoord     : TEXCOORD0;
    float3 EyeSpacePos  : TEXCOORD1;
    float  SphereRadius : TEXCOORD2;
};

VS_OUTPUT VS_Depth(VS_INPUT input)
{
    VS_OUTPUT output;
    
    // ワールド座標をビュー空間へ
    float3 eyePos = mul(float4(input.InstPos, 1.0), View).xyz;
    
    // ビルボード展開
    float3 viewPos = eyePos;
    viewPos.x += input.QuadPos.x * PointRadius;
    viewPos.y += input.QuadPos.y * PointRadius;
    
    output.Position = mul(float4(viewPos, 1.0), Proj);
    output.TexCoord = input.QuadPos;
    output.EyeSpacePos = eyePos;
    output.SphereRadius = PointRadius;
    
    return output;
}

float PS_Depth(VS_OUTPUT input) : SV_Target0
{
    // 円の中心からの距離
    float2 coord = input.TexCoord;
    float r2 = dot(coord, coord);
    
    // 円の外は破棄
    if (r2 > 1.0)
        discard;
    
    // 球の表面のZ（手前に膨らむ）
    float sphereZ = sqrt(1.0 - r2);
    
    // ビュー空間での位置（球の表面）
    float3 spherePos = input.EyeSpacePos;
    spherePos.z -= sphereZ * input.SphereRadius;
    
    // クリップ空間に変換して深度を計算
    float4 clipPos = mul(float4(spherePos, 1.0), Proj);
    float depth = clipPos.z / clipPos.w;
    
    return depth;
}