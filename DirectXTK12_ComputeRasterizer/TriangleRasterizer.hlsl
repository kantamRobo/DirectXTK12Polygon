// TriangleRasterizer.hlsl
// Compute shader-based software rasterizer.
// Performs pixel-driven triangle rasterization with perspective-correct interpolation.

RWTexture2D<float4> OutputTexture : register(u0);

struct Vertex {
    float3 pos;
    float4 color;
    float2 uv;
};
StructuredBuffer<Vertex> VertexBuffer : register(t0);
Texture2D<float4> BaseTexture : register(t1);
SamplerState BaseSampler : register(s0);

cbuffer ConstantBuffer : register(b0)
{
    matrix WorldViewProj;
    float2 ScreenSize;
    uint TriangleCount;
    float Padding;
}

float EdgeFunction(float2 a, float2 b, float2 c) {
    return (c.x - a.x) * (b.y - a.y) - (c.y - a.y) * (b.x - a.x);
}

[numthreads(16, 16, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    float2 p = float2(dispatchThreadID.x, dispatchThreadID.y) + 0.5f;
    if (p.x >= ScreenSize.x || p.y >= ScreenSize.y) return;
    float bestDepth = 1.0f;
    float4 bestColor = float4(0.1f, 0.1f, 0.15f, 1.0f);
    for (uint i = 0; i < TriangleCount; ++i)
    {
        uint idx = i * 3;
        Vertex v0_raw = VertexBuffer[idx];
        Vertex v1_raw = VertexBuffer[idx + 1];
        Vertex v2_raw = VertexBuffer[idx + 2];
        float4 c0 = mul(float4(v0_raw.pos, 1.0f), WorldViewProj);
        float4 c1 = mul(float4(v1_raw.pos, 1.0f), WorldViewProj);
        float4 c2 = mul(float4(v2_raw.pos, 1.0f), WorldViewProj);
        float invW0 = 1.0f / c0.w;
        float invW1 = 1.0f / c1.w;
        float invW2 = 1.0f / c2.w;
        float2 s0, s1, s2;
        s0.x = (c0.x * invW0 + 1.0f) * 0.5f * ScreenSize.x;
        s0.y = (1.0f - c0.y * invW0) * 0.5f * ScreenSize.y;
        s1.x = (c1.x * invW1 + 1.0f) * 0.5f * ScreenSize.x;
        s1.y = (1.0f - c1.y * invW1) * 0.5f * ScreenSize.y;
        s2.x = (c2.x * invW2 + 1.0f) * 0.5f * ScreenSize.x;
        s2.y = (1.0f - c2.y * invW2) * 0.5f * ScreenSize.y;
        float area = EdgeFunction(s0, s1, s2);
        if (area <= 0) continue;
        float w0 = EdgeFunction(s1, s2, p);
        float w1 = EdgeFunction(s2, s0, p);
        float w2 = EdgeFunction(s0, s1, p);
        if (w0 >= 0 && w1 >= 0 && w2 >= 0) {
            w0 /= area; w1 /= area; w2 /= area;
            float interpolatedInvW = w0 * invW0 + w1 * invW1 + w2 * invW2;
            float currentW = 1.0f / interpolatedInvW;
            float currentDepth = c0.z * invW0 * w0 + c1.z * invW1 * w1 + c2.z * invW2 * w2;
            if (currentDepth < bestDepth) {
                bestDepth = currentDepth;
                float2 uv0_p = v0_raw.uv * invW0;
                float2 uv1_p = v1_raw.uv * invW1;
                float2 uv2_p = v2_raw.uv * invW2;
                float2 finalUV = (w0 * uv0_p + w1 * uv1_p + w2 * uv2_p) * currentW;
                float4 col0_p = v0_raw.color * invW0;
                float4 col1_p = v1_raw.color * invW1;
                float4 col2_p = v2_raw.color * invW2;
                float4 finalVertexColor = (w0 * col0_p + w1 * col1_p + w2 * col2_p) * currentW;
                float4 texColor = BaseTexture.SampleLevel(BaseSampler, finalUV, 0);
                bestColor = finalVertexColor * texColor;
            }
        }
    }
    OutputTexture[dispatchThreadID.xy] = bestColor;
}
