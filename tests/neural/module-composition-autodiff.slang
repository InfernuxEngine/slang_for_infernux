//TEST:SIMPLE: -target spirv -entry main -stage compute -experimental-feature
//TEST:SIMPLE: -target wgsl -entry main -stage compute -experimental-feature

import slang.neural;
import slang.functional;
import slang.numerics.differentiable;

[[vk::binding(0, 0)]] StructuredBuffer<float> input;
[[vk::binding(1, 0)]] RWStructuredBuffer<float4> output;

// The array constructor, activation, and generic vector must all retain the
// derivative of x. At x = 0 the primal is 1 and both derivatives are 0.75.
[Differentiable]
float activate(float x)
{
    float[3] components = {x, 2.0 * x, -x};
    let value = InlineVector<float, 3>(components);
    let result = Sigmoid<float>().eval<InlineVector<float, 3>>(value);
    return result[0] + result[1];
}

[shader("compute")]
[numthreads(1, 1, 1)]
void main()
{
    let mapped = float3(1.0, 2.0, 3.0).map((float x) => x + 1.0);
    let forward = fwd_diff(activate)(diffPair(input[0], 1.0));
    DifferentialPair<float> reverse = diffPair(input[0], 0.0);
    bwd_diff(activate)(reverse, 1.0);
    output[0] = float4(forward.p, forward.d, reverse.d, mapped[0] + mapped[1] + mapped[2]);
}
