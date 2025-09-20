//--------------------------------------------------------------------------------------
// SimpleTrianglePS.hlsl (PS)
//--------------------------------------------------------------------------------------
#include "Common.hlsli"

[RootSignature("")]
float4 main(VertexOut input) : SV_Target
{
    return input.Color; // MS 側でセットした色をそのまま出力
}
