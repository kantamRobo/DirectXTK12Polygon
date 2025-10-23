struct VSOut { float4 pos : SV_Position; };
cbuffer ColorCB : register(b0) { float4 gColor; }

float4 PSMain(VSOut v) : SV_Target{ return gColor; }