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

// �o�́iCommon.hlsli ���� VertexOut ��z��FPosition, Color �����j
[NumThreads(1, 1, 1)]
[OutputTopology("triangle")]
void main(
    uint3 tid : SV_DispatchThreadID,
    out indices uint3 tris[1],
    out vertices VertexOut verts[3]
)
{
    // ����́u�擪�� 3 �C���f�b�N�X�ŎO�p�` 1 ���v��f���ŏ���
    // ���σ��b�V���ɂ���Ȃ�ʓr�C���f�b�N�X��/�v���~�e�B�u����������萔�œn���Ă�������
    SetMeshOutputCounts(3, 1);

    // lane 0 �����Ŋm��I�ɏ����i����������Ȃ� Wave ������ł��ǂ��j
    if (tid.x == 0)
    {
        // �ǂݏo��
        const uint i0 = gIndices[0];
        const uint i1 = gIndices[1];
        const uint i2 = gIndices[2];

        float4 p0 = float4(gVertices[i0].pos, 1.0);
        float4 p1 = float4(gVertices[i1].pos, 1.0);
        float4 p2 = float4(gVertices[i2].pos, 1.0);

        // MVP
        const float4x4 MVP = mul(gCB.Proj, mul(gCB.View, gCB.World));

        // ���_�i���Œ�B�K�v�Ȃ� SRV ����F��ǂލ\���Ɋg�����ĉ������j
         // ���[���h�ϊ��A�r���[�ϊ��A�v���W�F�N�V�����ϊ���K�p
        
        verts[0].Color = float4(1, 1, 1, 1);
        verts[1].Color = float4(1, 1, 1, 1);
        verts[2].Color = float4(1, 1, 1, 1);

        // (2) �x�N�g���͍��ł͂Ȃ��E����|����
        verts[0].Position = p0;
        verts[1].Position = p1;
        verts[2].Position = p2;
        // �O�p�` 1 ��
        tris[0] = uint3(0, 1, 2);
    }
}
