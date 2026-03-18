//==============================================================================
// ブラーパス：深度を滑らかにする
//==============================================================================

Texture2D DepthTexture : register(t0);
SamplerState PointSampler : register(s0);

cbuffer CBBlur : register(b0) {
    float2 TexelSize;    // 1.0 / 画面サイズ
    float BlurScale;     // ブラーの強さ
    float BlurDepthFalloff;  // 深度差による減衰
};

struct VS_OUTPUT {
    float4 Position : SV_Position;
    float2 TexCoord : TEXCOORD;
};

// 頂点シェーダー（パススルー）
VS_OUTPUT VS_Blur(float3 pos : POSITION, float2 uv : TEXCOORD) {
    VS_OUTPUT output;
    output.Position = float4(pos, 1.0);
    output.TexCoord = uv;
    return output;
}

// バイラテラルブラー（深度を考慮したブラー）
float4 PS_BlurH(VS_OUTPUT input) : SV_Target {
    float centerDepth = DepthTexture.Sample(PointSampler, input.TexCoord).r;
    
    if (centerDepth <= 0) return float4(0, 0, 0, 0);
    
    float sum = 0;
    float wsum = 0;
    
    // 水平方向にサンプリング
    for (int x = -5; x <= 5; x++) {
        float2 offset = float2(x * TexelSize.x * BlurScale, 0);
        float sampleDepth = DepthTexture.Sample(PointSampler, input.TexCoord + offset).r;
        
        if (sampleDepth <= 0) continue;
        
        // 深度差による重み減衰
        float depthDiff = abs(centerDepth - sampleDepth);
        float w = exp(-depthDiff * BlurDepthFalloff) * exp(-x * x / 10.0);
        
        sum += sampleDepth * w;
        wsum += w;
    }
    
    return float4(sum / max(wsum, 0.0001), 0, 0, 1);
}

float4 PS_BlurV(VS_OUTPUT input) : SV_Target {
    float centerDepth = DepthTexture.Sample(PointSampler, input.TexCoord).r;
    
    if (centerDepth <= 0) return float4(0, 0, 0, 0);
    
    float sum = 0;
    float wsum = 0;
    
    // 垂直方向にサンプリング
    for (int y = -5; y <= 5; y++) {
        float2 offset = float2(0, y * TexelSize.y * BlurScale);
        float sampleDepth = DepthTexture.Sample(PointSampler, input.TexCoord + offset).r;
        
        if (sampleDepth <= 0) continue;
        
        float depthDiff = abs(centerDepth - sampleDepth);
        float w = exp(-depthDiff * BlurDepthFalloff) * exp(-y * y / 10.0);
        
        sum += sampleDepth * w;
        wsum += w;
    }
    
    return float4(sum / max(wsum, 0.0001), 0, 0, 1);
}