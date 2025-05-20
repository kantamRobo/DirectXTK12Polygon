
// DS: バリセンタリック補間
struct DSOut
{
    float4 Pos : SV_POSITION;
};
// HS: 3 control-point patch を受け取り、出力制御点とパッチ定数を生成
struct HSOut
{
    float3 Pos : POSITION;
};

struct HSConst
{
    float EdgeTess[3] : SV_TessFactor; // 外周
    float InsideTess : SV_InsideTessFactor; // 内部
};

[domain("tri")]
DSOut main(HSConst hc, const OutputPatch<HSOut, 3> patch, float3 bary : SV_DomainLocation)
{
    DSOut o;
    float3 p = patch[0].Pos * bary.x
        + patch[1].Pos * bary.y
        + patch[2].Pos * bary.z;
    // そのままクリップ空間と仮定（射影行列などは省略）
    o.Pos = float4(p, 1.0f);
    return o;
}