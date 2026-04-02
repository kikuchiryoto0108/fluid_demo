cbuffer CBBlur : register(b0)
{
    float2 BlurDir;
    float  BlurScale;
    float  BlurDepthFalloff;
    int    FilterRadius;
    float3 _pad1;
};

Texture2D<float> DepthTex : register(t0);
SamplerState PointSamp : register(s0);

struct VS_OUT
{
    float4 Pos : SV_POSITION;
    float2 UV  : TEXCOORD0;
};

VS_OUT VS_Quad(uint id : SV_VertexID)
{
    VS_OUT o;
    o.UV = float2((id << 1) & 2, id & 2);
    o.Pos = float4(o.UV * float2(2, -2) + float2(-1, 1), 0, 1);
    return o;
}

float PS_BilateralBlur(VS_OUT i) : SV_Target0
{
    float center = DepthTex.SampleLevel(PointSamp, i.UV, 0);
    
    // 深度がない場所は0を返す（補間処理を省略）
    if (center < 0.0001)
        return 0;
    
    float sum = center;
    float wsum = 1.0;
    
    // ループを固定回数に（-7～7 = 15サンプル）
    [unroll]
    for (int x = -7; x <= 7; x++)
    {
        if (x == 0) continue;
        
        float2 uv = i.UV + BlurDir * float(x) * 2.0;  // 2ピクセル飛ばし
        float s = DepthTex.SampleLevel(PointSamp, uv, 0);
        
        if (s < 0.0001) continue;
        
        // シンプルなガウシアン重み
        float dist = float(x) * BlurScale;
        float w = exp(-dist * dist * 0.1);
        
        sum += s * w;
        wsum += w;
    }
    
    return sum / wsum;
}

float PS_GaussianBlur(VS_OUT i) : SV_Target0
{
    float sum = 0;
    float wsum = 0;
    
    [unroll]
    for (int x = -5; x <= 5; x++)
    {
        float2 uv = i.UV + BlurDir * float(x);
        float s = DepthTex.SampleLevel(PointSamp, uv, 0);
        
        float w = exp(-float(x * x) * 0.1);
        
        sum += s * w;
        wsum += w;
    }
    
    return sum / wsum;
}