//TEST:SIMPLE(filecheck=WGSL): -target wgsl -stage compute -entry computeMain
//TEST:SIMPLE(filecheck=WGSL): -target wgsl -stage compute -entry computeMain -fvk-use-dx-layout
//TEST:SIMPLE(filecheck=WGSL): -target wgsl -stage compute -entry computeMain -fvk-use-c-layout
//TEST:SIMPLE(filecheck=WGSL): -target wgsl -stage compute -entry computeMain -force-glsl-scalar-layout
//TEST:SIMPLE(filecheck=TINT): -target wgsl-spirv-asm -stage compute -entry computeMain

// ScalarDataLayout packs float3 elements to 12 bytes for SPIR-V. WGSL uses a native storage
// array of vec3<f32>, whose element stride is 16 bytes. GetDimensions reports that same stride.
StructuredBuffer<float3, ScalarDataLayout> values;
RWStructuredBuffer<uint> output;

// WGSL: var<storage, read> [[VALUES:values_[0-9]+]] : array<vec3<f32>>;
// WGSL: arrayLength(&[[VALUES]]), 16);

// TINT: OpDecorate [[ARRAY:%[^ ]+]] ArrayStride 16
// TINT: [[ARRAY]] = OpTypeRuntimeArray %v3float
// TINT: [[STRIDE:%[^ ]+]] = OpConstant %uint 16
// TINT: [[COUNT:%[^ ]+]] = OpArrayLength %uint
// TINT: OpCompositeConstruct %v2uint [[COUNT]] [[STRIDE]]

[numthreads(1, 1, 1)]
void computeMain(uint3 tid : SV_DispatchThreadID)
{
    uint count, stride;
    values.GetDimensions(count, stride);
    if (tid.x < count)
        output[tid.x] = uint(values[tid.x].x) + stride;
}
