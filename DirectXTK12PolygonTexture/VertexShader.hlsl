// „Ÿ„Ÿ VertexShader.hlsl „Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ
#include "Shader.hlsli"

VSOutput main(float3 inPos : POSITION,
    float4 inColor : COLOR, float2 texcoord : TEXCOORD0)
{
    VSOutput outV;

    float4 pos = float4(inPos, 1.0f);
    outV.position = pos + offset; // © SV_POSITION ‚ğ•K‚¸‘‚­
    outV.color = inColor;
    outV.texcoord = texcoord;
    return outV;
}
