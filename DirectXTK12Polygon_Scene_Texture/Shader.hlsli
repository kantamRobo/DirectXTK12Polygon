struct VS_INPUT
{
    float4 pos : POSITION; // 頂点位置
    float3 normal : NORMAL; // 法線
    float2 texcoord : TEXCOORD0;
};

cbuffer ConstantBuffer
{
    float4x4 World; // ワールド変換行列
    float4x4 View; // ビュー変換行列
    float4x4 Projection; // 透視射影変換行列
};

struct VS_OUTPUT
{
    float4 pos : SV_POSITION; // 出力のスクリーン座標
    float3 normal : NORMAL; // 出力の法線
    float2 texcoord : TEXCOORD0;
};