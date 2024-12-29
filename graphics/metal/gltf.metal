#include <metal_stdlib>

using namespace metal;

// float3 / float2 have special alignment rules that break the memory layout

struct Vec2 {
    float x, y;
};

struct Vec3 {
    float x, y, z;
};

struct Vertex {
    Vec3 p;
    Vec3 n;
    Vec2 t;
};
static_assert(sizeof(Vertex) == sizeof(float) * 8, "Incompatible vertex memory layout");

struct Uniforms {
    float4x4 model;
    float4x4 view;
};

struct Fragment {
    float4 position [[position]];
    float2 texcoord;
};

vertex Fragment main_vs(const device Vertex* vertices [[buffer(0)]], const device Uniforms* u [[buffer(1)]], uint vid [[vertex_id]]) {
    Vertex in = vertices[vid];
    return (Fragment) {
        .position = u->view * u->model * float4(in.p.x, in.p.y, in.p.z, 1.0f),
        .texcoord = float2(in.t.x, in.t.y),
    };
}

fragment float4 main_fs(Fragment frag [[stage_in]], texture2d<float> tex [[texture(0)]]) {
    constexpr sampler tex_sampler = sampler(mag_filter::linear, min_filter::linear);
    return tex.sample(tex_sampler, frag.texcoord);
}
