//TEST:SIMPLE(filecheck=VERT): -target wgsl -stage vertex -entry vertexMain
//TEST:SIMPLE(filecheck=FRAG): -target wgsl -stage fragment -entry fragmentMain
//TEST:SIMPLE(filecheck=AGGREGATE_ROW): -target wgsl -stage vertex -entry aggregateVertex -matrix-layout-row-major
//TEST:SIMPLE(filecheck=AGGREGATE_COL): -target wgsl -stage vertex -entry aggregateVertex
//TEST:SIMPLE(filecheck=AGGREGATE_ROW): -target wgsl -stage fragment -entry aggregateFragment -matrix-layout-row-major
//TEST:SIMPLE(filecheck=AGGREGATE_COL): -target wgsl -stage fragment -entry aggregateFragment
//TEST:SIMPLE(filecheck=DIRECT): -target wgsl -stage fragment -entry directTarget
//TEST:SIMPLE(filecheck=SPIRV_ROW): -target spirv-asm -stage vertex -entry aggregateVertex -matrix-layout-row-major
//TEST:SIMPLE(filecheck=SPIRV_COL): -target spirv-asm -stage vertex -entry aggregateVertex -matrix-layout-column-major

// Tests three aspects:
//  1. Flatten the nested struct for the return type of the entry-point
//  2. For fragment shader, SV_TARGET index must be emitted as @location(index)
//  3. For non-fragment shader, the user defined semantics should be emitted as @location(index)
//  4. Explicit locations and automatically assigned locations agree with reflection.

struct VertexInput
{
    //VERT-DAG: @location(0) mesh_position
    [[vk::location(0)]] float3 mesh_position : POSITION;
    //VERT-DAG: @location(4) mesh_uv
    [[vk::location(4)]] float2 mesh_uv : TEXCOORD0;
};

struct FragmentOutput
{
    //FRAG: @location(1) color1
    float4 color1 : SV_TARGET1;

    //FRAG: @location(0) color0
    float4 color0 : SV_TARGET0;
};

struct NestedVertexOutput
{
    float4 color : COLOR0;
};

struct VertexOutput
{
    //VERT-DAG: @location(0) uv
    //FRAG-DAG: @location(0) uv
    float2 uv : TEXCOORD0;

    //VERT-DAG: @location(1) color
    //FRAG-DAG: @location(1) color
    NestedVertexOutput nested;

    //VERT-DAG: @builtin(position) position
    //FRAG-DAG: @builtin(position) position
    float4 position : SV_Position;
};

VertexOutput vertexMain(VertexInput input)
{
    VertexOutput out;
    out.position = float4(input.mesh_position, 1.0);
    out.uv = input.mesh_uv;
    out.nested.color = float4(0.0, 0.0, 0.0, 0.0);
    return out;
}

//FRAG-DAG: @location(2) color3
//FRAG-DAG: @location(3) color6
FragmentOutput fragmentMain(VertexOutput input, float4 color3: COLOR3, float4 color6: COLOR6)
{
    FragmentOutput out;
    out.color0 = input.nested.color + color3;
    out.color1 = input.nested.color + color6;
    return out;
}

struct Piece
{
    float2x3 axes : AXES;
    float2 samples[2] : SAMPLES;
};

struct AggregateOutput
{
    float4 position : SV_Position;
    Piece data[2];
    nointerpolation uint2 ids[2] : IDS;
    float2x3 columns : COLUMNS;
};

//SPIRV_ROW-DAG: OpDecorate %entryPointParam_aggregateVertex_data_0_samples_0 Location 2
//SPIRV_ROW-DAG: OpDecorate %entryPointParam_aggregateVertex_columns_1 Location 11
//SPIRV_ROW-DAG: %entryPointParam_aggregateVertex_data_0_axes_0 = OpVariable %_ptr_Output_v3float
//SPIRV_COL-DAG: OpDecorate %entryPointParam_aggregateVertex_data_0_samples_0 Location 3
//SPIRV_COL-DAG: OpDecorate %entryPointParam_aggregateVertex_columns_2 Location 14
//SPIRV_COL-DAG: %entryPointParam_aggregateVertex_data_0_axes_0 = OpVariable %_ptr_Output_v2float
//AGGREGATE_ROW-DAG: @location(0) {{[^:]+}} : vec3<f32>
//AGGREGATE_ROW-DAG: @location(1) {{[^:]+}} : vec3<f32>
//AGGREGATE_ROW-DAG: @location(2) {{[^:]+}} : vec2<f32>
//AGGREGATE_ROW-DAG: @location(3) {{[^:]+}} : vec2<f32>
//AGGREGATE_ROW-DAG: @location(4) {{[^:]+}} : vec3<f32>
//AGGREGATE_ROW-DAG: @location(5) {{[^:]+}} : vec3<f32>
//AGGREGATE_ROW-DAG: @location(6) {{[^:]+}} : vec2<f32>
//AGGREGATE_ROW-DAG: @location(7) {{[^:]+}} : vec2<f32>
//AGGREGATE_ROW-DAG: @interpolate(flat) @location(8) {{[^:]+}} : vec2<u32>
//AGGREGATE_ROW-DAG: @interpolate(flat) @location(9) {{[^:]+}} : vec2<u32>
//AGGREGATE_ROW-DAG: @location(10) {{[^:]+}} : vec3<f32>
//AGGREGATE_ROW-DAG: @location(11) {{[^:]+}} : vec3<f32>
//AGGREGATE_COL-DAG: @location(0) {{[^:]+}} : vec2<f32>
//AGGREGATE_COL-DAG: @location(1) {{[^:]+}} : vec2<f32>
//AGGREGATE_COL-DAG: @location(2) {{[^:]+}} : vec2<f32>
//AGGREGATE_COL-DAG: @location(3) {{[^:]+}} : vec2<f32>
//AGGREGATE_COL-DAG: @location(4) {{[^:]+}} : vec2<f32>
//AGGREGATE_COL-DAG: @location(5) {{[^:]+}} : vec2<f32>
//AGGREGATE_COL-DAG: @location(6) {{[^:]+}} : vec2<f32>
//AGGREGATE_COL-DAG: @location(7) {{[^:]+}} : vec2<f32>
//AGGREGATE_COL-DAG: @location(8) {{[^:]+}} : vec2<f32>
//AGGREGATE_COL-DAG: @location(9) {{[^:]+}} : vec2<f32>
//AGGREGATE_COL-DAG: @interpolate(flat) @location(10) {{[^:]+}} : vec2<u32>
//AGGREGATE_COL-DAG: @interpolate(flat) @location(11) {{[^:]+}} : vec2<u32>
//AGGREGATE_COL-DAG: @location(12) {{[^:]+}} : vec2<f32>
//AGGREGATE_COL-DAG: @location(13) {{[^:]+}} : vec2<f32>
//AGGREGATE_COL-DAG: @location(14) {{[^:]+}} : vec2<f32>
AggregateOutput aggregateVertex(VertexInput input)
{
    AggregateOutput result;
    result.position = float4(input.mesh_position, 1);
    result.columns = float2x3(1, 2, 3, 4, 5, 6);
    for (int i = 0; i < 2; ++i)
    {
        result.data[i].axes = float2x3(1, 2, 3, 4, 5, 6);
        result.ids[i] = uint2(i, i + 1);
        for (int j = 0; j < 2; ++j)
            result.data[i].samples[j] = input.mesh_uv * (i + j + 1);
    }
    return result;
}

float4 aggregateFragment(AggregateOutput input) : SV_Target
{
    uint i = input.ids[1].x;
    return float4(input.data[i].samples[0], input.data[0].axes[1].xy + input.columns[1].yz);
}

//DIRECT: @location(3) output
float4 directTarget() : SV_Target3
{
    return float4(1, 0, 1, 1);
}
