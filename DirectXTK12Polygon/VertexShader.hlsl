
struct VS_INPUT
{
    float4 pos : POSITION;   // 頂点位置
   
   
};

struct VS_OUTPUT
{
    float4 pos : SV_POSITION; // 出力のスクリーン座標
   
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;

    
	output.pos = input.pos;
	

  
    return output;
}
