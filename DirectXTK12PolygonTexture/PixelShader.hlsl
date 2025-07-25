// ── PixelShader.hlsl ──────────────────────
#include "Shader.hlsli"

// テクスチャおよびサンプラーステートを定義
Texture2D g_Texture : register(t0);
SamplerState g_Sampler : register(s0);

// 入力構造体（UV座標を受け取れるようにする）
struct PSInput
{
    float4 position : SV_POSITION;
    float3 color : COLOR0;
    float2 uv : TEXCOORD0;
    
};

// ピクセルシェーダー
float4 main(PSInput input) : SV_TARGET
{
    // テクスチャをサンプリングして色を返す
    return g_Texture.Sample(g_Sampler, input.uv);
}
