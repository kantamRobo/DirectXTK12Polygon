#include "Shader.hlsli"

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;

    // ワールド変換、ビュー変換、プロジェクション変換を適用
    float4 worldPosition = mul(input.pos, World);
    float4 viewPosition = mul(worldPosition, View);
    output.pos = mul(viewPosition, Projection);
	output.texcoord = input.texcoord;
    output.normal = mul(float4(input.normal, 0.0f), World).xyz; // 法線もワールド座標系に変換


    return output;
}
