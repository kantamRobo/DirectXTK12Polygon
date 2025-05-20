struct VS_INPUT
{
    float4 pos : POSITION; // 頂点位置
    float3 normal : NORMAL; // 法線
};

cbuffer ConstantBuffer:register(b0)
{
    float4x4 World; // ワールド変換行列
    float4x4 View; // ビュー変換行列
    float4x4 Projection; // 透視射影変換行列
};

struct VS_OUTPUT
{
    
    
    float4 pos : SV_POSITION; // 出力のスクリーン座標
    float3 normal : NORMAL; // 出力の法線
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    /*
    // ワールド変換、ビュー変換、プロジェクション変換を適用
    float4 worldPosition = mul(input.pos, World);
    float4 viewPosition = mul(worldPosition, View);
    output.pos = mul(viewPosition, Projection);
    
    output.normal = mul(float4(input.normal, 0.0f), World).xyz; // 法線もワールド座標系に変換
 */
    output.pos = input.pos;
   // output.normal = input.normal;
    
   
    return output;
}
