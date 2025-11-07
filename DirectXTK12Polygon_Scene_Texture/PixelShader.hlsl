// ── PixelShader.hlsl ──────────────────────
#include "Shader.hlsli"

// テクスチャおよびサンプラーステートを定義
Texture2D g_Texture : register(t0);
SamplerState g_Sampler : register(s0);



// ピクセルシェーダー
float4 main(VS_OUTPUT input) : SV_TARGET
{
    // テクスチャをサンプリングして色を返す
    return g_Texture.Sample(g_Sampler, input.texcoord);
}
