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

// 出力（Common.hlsli 側の VertexOut を想定：Position, Color を持つ）
[NumThreads(1, 1, 1)]
[OutputTopology("triangle")]
void main(
    uint3 tid : SV_DispatchThreadID,
    out indices uint3 tris[1],
    out vertices VertexOut verts[3]
)
{
    // 今回は「先頭の 3 インデックスで三角形 1 枚」を吐く最小例
    // ※可変メッシュにするなら別途インデックス数/プリミティブ数を引数や定数で渡してください
    SetMeshOutputCounts(3, 1);

    // lane 0 だけで確定的に処理（分岐を避けるなら Wave 内判定でも良い）
    if (tid.x == 0)
    {
        // 読み出し
        const uint i0 = gIndices[0];
        const uint i1 = gIndices[1];
        const uint i2 = gIndices[2];

        float4 p0 = float4(gVertices[i0].pos, 1.0);
        float4 p1 = float4(gVertices[i1].pos, 1.0);
        float4 p2 = float4(gVertices[i2].pos, 1.0);

        // MVP
        const float4x4 MVP = mul(gCB.Proj, mul(gCB.View, gCB.World));

        // 頂点（白固定。必要なら SRV から色を読む構造に拡張して下さい）
         // ワールド変換、ビュー変換、プロジェクション変換を適用
        
        verts[0].Color = float4(1, 1, 1, 1);
        verts[1].Color = float4(1, 1, 1, 1);
        verts[2].Color = float4(1, 1, 1, 1);

        // (2) ベクトルは左ではなく右から掛ける
        verts[0].Position = p0;
        verts[1].Position = p1;
        verts[2].Position = p2;
        // 三角形 1 枚
        tris[0] = uint3(0, 1, 2);
    }
}
