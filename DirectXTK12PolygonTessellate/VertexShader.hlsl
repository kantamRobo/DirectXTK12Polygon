struct VS_INPUT
{
    float4 pos : POSITION; // ���_�ʒu
    float3 normal : NORMAL; // �@��
};

cbuffer ConstantBuffer:register(b0)
{
    float4x4 World; // ���[���h�ϊ��s��
    float4x4 View; // �r���[�ϊ��s��
    float4x4 Projection; // �����ˉe�ϊ��s��
};

struct VS_OUTPUT
{
    
    
    float4 pos : SV_POSITION; // �o�͂̃X�N���[�����W
    float3 normal : NORMAL; // �o�̖͂@��
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    /*
    // ���[���h�ϊ��A�r���[�ϊ��A�v���W�F�N�V�����ϊ���K�p
    float4 worldPosition = mul(input.pos, World);
    float4 viewPosition = mul(worldPosition, View);
    output.pos = mul(viewPosition, Projection);
    
    output.normal = mul(float4(input.normal, 0.0f), World).xyz; // �@�������[���h���W�n�ɕϊ�
 */
    output.pos = input.pos;
   // output.normal = input.normal;
    
   
    return output;
}
