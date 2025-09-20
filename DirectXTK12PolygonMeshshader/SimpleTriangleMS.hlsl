//--------------------------------------------------------------------------------------
// SimpleTriangleMS.hlsl (SRV バインド版 / MS)
//--------------------------------------------------------------------------------------
#include "Common.hlsli"

// C++ 側のルートシグネチャ前提: b0=CBV, t0=頂点SRV, t1=インデックスSRV
// （C++ で SetGraphicsRootConstantBufferView(b0), SetGraphicsRootDescriptorTable(t0-t1) する）

// 最低限の定数バッファ（行列は必要に応じて調整）
struct SceneCB
{
    float4x4 World;
    float4x4 View;
    float4x4 Proj;
};
ConstantBuffer<SceneCB> gCB : register(b0);

// 頂点/インデックスを SRV から読む
struct VertexPosition
{
    float3 pos;
}; // C++側の VertexPosition と一致させる
StructuredBuffer<VertexPosition> gVertices : register(t0);
StructuredBuffer<uint> gIndices : register(t1);
// SimpleTriangleMS.hlsl
[NumThreads(1, 1, 1)]
[OutputTopology("triangle")]
void main(
    uint3 gtid : SV_GroupThreadID, // ← SV_DispatchThreadID ではなくこちら
    out indices uint3 tris[1],
    out vertices VertexOut verts[3]
)
{
    SetMeshOutputCounts(3, 1);
    if (gtid.x == 0)
    {
        const uint i0 = gIndices[0];
        const uint i1 = gIndices[1];
        const uint i2 = gIndices[2];

        float4 p0 = float4(gVertices[i0].pos, 1.0);
        float4 p1 = float4(gVertices[i1].pos, 1.0);
        float4 p2 = float4(gVertices[i2].pos, 1.0);

        // 右掛け（C++ 側で World/View/Proj を転置しているため）
        float4x4 WVP = mul(gCB.World, mul(gCB.View, gCB.Proj));
        verts[0].Position = mul(p0, WVP);
        verts[1].Position = mul(p1, WVP);
        verts[2].Position = mul(p2, WVP);

        verts[0].Color = float4(1, 1, 1, 1);
        verts[1].Color = float4(1, 1, 1, 1);
        verts[2].Color = float4(1, 1, 1, 1);

        tris[0] = uint3(0, 1, 2);
    }
}
