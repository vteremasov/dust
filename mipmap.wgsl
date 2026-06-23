// Mipmap generation shader
// Renders a fullscreen triangle sampling from the previous mip level

struct VOut {
    @builtin(position) position: vec4f,
    @location(0) uv: vec2f,
};

@vertex fn vs(@builtin(vertex_index) vi : u32) -> VOut {
    var pos = array<vec2f, 3>(
        vec2f(-1.0, -1.0),
        vec2f( 3.0, -1.0),
        vec2f(-1.0,  3.0)
    );
    var out: VOut;
    out.position = vec4f(pos[vi], 0.0, 1.0);
    out.uv = pos[vi] * vec2f(0.5, -0.5) + 0.5;
    return out;
}

@group(0) @binding(0) var src: texture_2d<f32>;
@group(0) @binding(1) var smp: sampler;

@fragment fn fs(in: VOut) -> @location(0) vec4f {
    return textureSample(src, smp, in.uv);
}
