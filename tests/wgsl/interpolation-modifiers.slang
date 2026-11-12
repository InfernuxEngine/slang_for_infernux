// HLSL interpolation modifiers on WGSL user varyings:
//   linear (perspective-correct)  -> WGSL default (no attribute)
//   noperspective                 -> @interpolate(linear)
//   centroid / sample             -> @interpolate(perspective, centroid / sample)
//   nointerpolation               -> @interpolate(flat)
//   integer varyings              -> @interpolate(flat), which WGSL requires
//TEST:SIMPLE(filecheck=VS): -target wgsl -stage vertex -entry vsMain
//TEST:SIMPLE(filecheck=FS): -target wgsl -stage fragment -entry fsMain
// Validate the emitted WGSL through Tint (ignored where the downstream compiler is absent).
//TEST:SIMPLE(filecheck=SPV): -target wgsl-spirv-asm -stage vertex -entry vsMain
//TEST:SIMPLE(filecheck=SPV): -target wgsl-spirv-asm -stage fragment -entry fsMain

struct Varyings
{
    float4 position : SV_Position;
    linear float a : A;
    noperspective float b : B;
    centroid float c : C;
    sample float d : D;
    nointerpolation float f : F;
    uint id : ID;
    int2 cell : CELL;
};

// Author semantics on a helper value are not physical stage locations.
struct ComputedValues
{
    float a : TEXCOORD0;
    nointerpolation uint id : TEXCOORD1;
};

[noinline]
ComputedValues make_values(uint id)
{
    ComputedValues result;
    result.a = float(id) + 1.0;
    result.id = id;
    return result;
}

// SPV: OpEntryPoint

// VS-DAG: {{^}}    @location(0) a_{{[0-9]+}} : f32
// VS-DAG: @interpolate(linear) @location(1) b_{{[0-9]+}} : f32
// VS-DAG: @interpolate(perspective, centroid) @location(2) c_{{[0-9]+}} : f32
// VS-DAG: @interpolate(perspective, sample) @location(3) d_{{[0-9]+}} : f32
// VS-DAG: @interpolate(flat) @location(4) f_{{[0-9]+}} : f32
// VS-DAG: @interpolate(flat) @location(5) id_{{[0-9]+}} : u32
// VS-DAG: @interpolate(flat) @location(6) cell_{{[0-9]+}} : vec2<i32>
Varyings vsMain(uint vid: SV_VertexID)
{
    ComputedValues values = make_values(vid);
    Varyings o;
    o.position = float4(float(vid), 0, 0, 1);
    o.a = values.a;
    o.b = 2;
    o.c = 3;
    o.d = 4;
    o.f = 6;
    o.id = values.id;
    o.cell = int2(vid, 2);
    return o;
}

// FS-DAG: @interpolate(flat) @location({{[0-9]+}}) id_{{[0-9]+}} : u32
// FS-DAG: @interpolate(perspective, sample) @location({{[0-9]+}}) d_{{[0-9]+}} : f32
[noinline]
float4 read_varyings(Varyings v)
{
    return float4(v.a + v.b + v.c + v.d, v.f, float(v.id), float(v.cell.x));
}

float4 fsMain(Varyings v) : SV_Target
{
    return read_varyings(v);
}
