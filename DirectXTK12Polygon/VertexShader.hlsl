
struct VS_INPUT
{
    float4 pos : POSITION;   // ���_�ʒu
   
   
};

struct VS_OUTPUT
{
    float4 pos : SV_POSITION; // �o�͂̃X�N���[�����W
   
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;

    
	output.pos = input.pos;
	

  
    return output;
}
