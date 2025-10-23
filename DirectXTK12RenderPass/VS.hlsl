
// Triangle.hlsl
struct VSIn { float2 pos : POSITION; };
struct VSOut { float4 pos : SV_Position; };

VSOut VSMain(VSIn vin) {
    VSOut v;
    v.pos = float4(vin.pos, 0.0f, 1.0f);
    return v;
}

cbuffer ColorCB : register(b0) { float4 gColor; }

float4 PSMain(VSOut v) : SV_Target{ return gColor; }


