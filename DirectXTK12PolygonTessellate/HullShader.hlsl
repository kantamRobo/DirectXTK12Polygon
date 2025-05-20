
cbuffer TessFactorBuffer : register(b1)
{
    float Inner; // SV_InsideTessFactor に対応
    float Outer; // SV_TessFactor に対応（3 要素すべてに同じ値を流す想定）
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

// VS: 単に入力頂点を次へ渡す
struct VSOut
{
    float4 Pos : SV_POSITION; // VS が出力する SV_POSITION に合わせる
};

[domain("tri")]
[partitioning("fractional_odd")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
[patchconstantfunc("HSConstFunc")]
HSOut main(InputPatch<VSOut, 3> patch, uint cid : SV_OutputControlPointID)
{
    HSOut o;
    o.Pos = patch[cid].Pos;
    return o;
}

HSConst HSConstFunc(InputPatch<VSOut, 3> patch)
{
    HSConst o;
    o.EdgeTess[0] = Outer;
    o.EdgeTess[1] = Outer;
    o.EdgeTess[2] = Outer;
    o.InsideTess = Inner;
    return o;
}