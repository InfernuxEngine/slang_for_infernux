//TEST:SIMPLE(filecheck=CHECK,PADDED): -target spirv-asm -emit-spirv-directly -entry computeMain -stage compute -DPADDED
//TEST:SIMPLE(filecheck=CHECK,PACKED): -target spirv-asm -emit-spirv-directly -entry computeMain -stage compute -DPACKED
//TEST:SIMPLE(filecheck=CHECK,DEFAULT): -target spirv-asm -emit-spirv-directly -entry computeMain -stage compute
//TEST:SIMPLE(filecheck=RAW): -target spirv-asm -emit-spirv-directly -entry rawMain -stage compute

// GetDimensions reports the runtime array stride, including explicit buffer layout padding.
#if defined(PADDED)
StructuredBuffer<float, Std140DataLayout> values;
#elif defined(PACKED)
StructuredBuffer<float3, ScalarDataLayout> values;
#else
StructuredBuffer<float3> values;
#endif

RWStructuredBuffer<uint> output;
ByteAddressBuffer rawValues;

// PADDED: OpDecorate [[ARRAY:%[^ ]+]] ArrayStride 16
// PACKED: OpDecorate [[ARRAY:%[^ ]+]] ArrayStride 12
// DEFAULT: OpDecorate [[ARRAY:%[^ ]+]] ArrayStride 16
// CHECK: [[ARRAY]] = OpTypeRuntimeArray
// PADDED: [[STRIDE:%[^ ]+]] = OpConstant %uint 16
// PACKED: [[STRIDE:%[^ ]+]] = OpConstant %uint 12
// DEFAULT: [[STRIDE:%[^ ]+]] = OpConstant %uint 16
// CHECK: [[COUNT:%[^ ]+]] = OpArrayLength %uint %values 0
// CHECK: OpStore {{%[^ ]+}} [[COUNT]]
// CHECK: OpStore {{%[^ ]+}} [[STRIDE]]

[numthreads(1, 1, 1)]
void computeMain()
{
    uint count, stride;
    values.GetDimensions(count, stride);
    output[0] = count;
    output[1] = stride;
}

// ByteAddressBuffer.GetDimensions still returns the byte count of its 32-bit words.
// RAW: OpDecorate [[WORDS_ARRAY:%[^ ]+]] ArrayStride 4
// RAW: [[WORDS_ARRAY]] = OpTypeRuntimeArray %uint
// RAW: [[WORD_STRIDE:%[^ ]+]] = OpConstant %uint 4
// RAW: [[WORDS:%[^ ]+]] = OpArrayLength %uint %rawValues 0
// RAW: [[BYTES:%[^ ]+]] = OpIMul %uint [[WORDS]] [[WORD_STRIDE]]
// RAW: OpStore {{%[^ ]+}} [[BYTES]]

[numthreads(1, 1, 1)]
void rawMain()
{
    uint bytes;
    rawValues.GetDimensions(bytes);
    output[0] = bytes;
}
