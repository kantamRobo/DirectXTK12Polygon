// „Ÿ„Ÿ Shader.hlsli „Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ„Ÿ
cbuffer SceneConstantBuffer : register(b0)
{
    float4 offset;
};

struct VSOutput        // © gPSInputh ‚Å‚Í‚È‚­—p“r‚ª•ª‚©‚é–¼‘O‚É
{
    float4 position : SV_POSITION; // š•K{
    float4 color : COLOR;
    float2 texcoord : TEXCOORD0;
};
