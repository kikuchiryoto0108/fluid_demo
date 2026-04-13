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
    
    // ★ 深度が1.0に近い = 何もない場所
    if (center > 0.9999)
        return 1.0;
    
    float sum = center;
    float wsum = 1.0;
    
    // ループを固定回数に（-7?7 = 15サンプル）
    [unroll]
    for (int x = -7; x <= 7; x++)
    {
        if (x == 0) continue;
        
        float2 uv = i.UV + BlurDir * float(x) * 2.0;  // 2ピクセル飛ばし
        float s = DepthTex.SampleLevel(PointSamp, uv, 0);
        
        // ★ 空ピクセルはスキップ
        if (s > 0.9999) continue;
        
        // ★ 深度差によるバイラテラル重み
        float depthDiff = abs(s - center);
        float bilateralW = exp(-depthDiff * depthDiff * BlurDepthFalloff);
        
        // シンプルなガウシアン重み
        float dist = float(x) * BlurScale;
        float spatialW = exp(-dist * dist * 0.1);
        
        // ★ 空間×深度の重み
        float w = spatialW * bilateralW;
        
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
        
        // ★ 空ピクセルはスキップ
        if (s > 0.9999) continue;
        
        float w = exp(-float(x * x) * 0.1);
        
        sum += s * w;
        wsum += w;
    }
    
    // ★ 有効サンプルがなければ空を返す
    if (wsum < 0.001) return 1.0;
    return sum / wsum;
}