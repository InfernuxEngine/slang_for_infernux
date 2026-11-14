//TEST:SIMPLE: -target spirv -entry computeMain -stage compute -experimental-feature
//TEST:SIMPLE: -target wgsl -entry computeMain -stage compute -experimental-feature

import slang.neural;

StructuredBuffer<float> input;
RWStructuredBuffer<float4> output;

// Construction inside the differentiated function must preserve dependence
// on the input, including array, scalar-broadcast, and explicit copy forms.
[Differentiable]
float construct(float x)
{
    float[3] values = {x, 2.0 * x, -x};
    let fromArray = InlineVector<float, 3>(values);
    let broadcast = InlineVector<float, 3>(x);
    let copy = InlineVector<float, 3>(fromArray);
    let zero = InlineVector<float, 3>();
    return fromArray[0] + copy[1] + broadcast[2] + zero[0];
}

[shader("compute")]
[numthreads(1, 1, 1)]
void computeMain()
{
    let forward = fwd_diff(construct)(diffPair(input[0], 1.0));
    DifferentialPair<float> reverse = diffPair(input[0], 0.0);
    bwd_diff(construct)(reverse, 1.0);
    output[0] = float4(forward.p, forward.d, reverse.d, 1.0);
}
