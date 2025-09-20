//--------------------------------------------------------------------------------------
// SimpleTriangleMS.hlsl (SRV �o�C���h�� / MS)
//--------------------------------------------------------------------------------------
#include "Common.hlsli"

// C++ ���̃��[�g�V�O�l�`���O��: b0=CBV, t0=���_SRV, t1=�C���f�b�N�XSRV
// �iC++ �� SetGraphicsRootConstantBufferView(b0), SetGraphicsRootDescriptorTable(t0-t1) ����j

// �Œ���̒萔�o�b�t�@�i�s��͕K�v�ɉ����Ē����j
struct SceneCB
{
    float4x4 World;
    float4x4 View;
    float4x4 Proj;
};
ConstantBuffer<SceneCB> gCB : register(b0);

// ���_/�C���f�b�N�X�� SRV ����ǂ�
struct VertexPosition
{
    float3 pos;
}; // C++���� VertexPosition �ƈ�v������
StructuredBuffer<VertexPosition> gVertices : register(t0);
StructuredBuffer<uint> gIndices : register(t1);
// SimpleTriangleMS.hlsl
[NumThreads(1, 1, 1)]
[OutputTopology("triangle")]
void main(
    uint3 gtid : SV_GroupThreadID, // �� SV_DispatchThreadID �ł͂Ȃ�������
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

        // �E�|���iC++ ���� World/View/Proj ��]�u���Ă��邽�߁j
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
