RaytracingAccelerationStructure g_scene : register(t0);
RWTexture2D<float4> g_output : register(u0);

struct RayPayload
{
    float4 color;
};

[shader("raygeneration")]
void RayGen()
{
    uint2 launchIndex = DispatchRaysIndex().xy;
    float2 dims = float2(DispatchRaysDimensions().xy);

    // 画面座標を -1 ~ 1 に正規化
    float2 d = (((launchIndex.xy + 0.5f) / dims.xy) * 2.0f - 1.0f);

    RayDesc ray;
    ray.Origin = float3(0, 0, -2);
    ray.Direction = normalize(float3(d.x, -d.y, 1)); // Z奥へ
    ray.TMin = 0.001;
    ray.TMax = 1000.0;

    RayPayload payload;
    payload.color = float4(0, 0, 0, 0);

    TraceRay(g_scene, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);

    g_output[launchIndex] = payload.color;
}

[shader("miss")]
void Miss(inout RayPayload payload)
{
    payload.color = float4(0.0f, 0.2f, 0.4f, 1.0f); // 背景色（青）
}

[shader("closesthit")]
void ClosestHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    float3 barycentrics = float3(1.0 - attr.barycentrics.x - attr.barycentrics.y, attr.barycentrics.x, attr.barycentrics.y);
    payload.color = float4(barycentrics, 1.0f); // 重心座標を色にする
}