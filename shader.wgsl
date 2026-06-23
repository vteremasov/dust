struct Uniforms {
    pan: vec2<f32>,
    zoom: f32,
    screen_size: vec2<f32>,
};

@group(0) @binding(0) var<uniform> uniforms: Uniforms;
@group(0) @binding(1) var font_texture: texture_2d<f32>;
@group(0) @binding(2) var font_sampler: sampler;

struct VertexInput {
    @location(0) position: vec2<f32>,
    @location(1) uv: vec2<f32>,
    @location(2) color: vec4<f32>,
};

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) uv: vec2<f32>,
    @location(1) color: vec4<f32>,
};

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    // Map screen/world coordinate to NDC (Normalized Device Coordinates)
    let screen_pos = (in.position * uniforms.zoom) + uniforms.pan;
    let ndc_x = (screen_pos.x / (uniforms.screen_size.x / 2.0)) - 1.0;
    // Invert Y for WebGPU (top-left is +1 Y in NDC space screen mapping logic)
    let ndc_y = 1.0 - (screen_pos.y / (uniforms.screen_size.y / 2.0));
    out.position = vec4<f32>(ndc_x, ndc_y, 0.0, 1.0);
    out.uv = in.uv;
    out.color = in.color;
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    let tex_color = textureSample(font_texture, font_sampler, in.uv);
    // Draw text (using red channel as alpha) or solid shapes (where tex_color.r is 1.0)
    return vec4<f32>(in.color.rgb, in.color.a * tex_color.r);
}

@fragment
fn fs_image(in: VertexOutput) -> @location(0) vec4<f32> {
    // Draw image sampling full RGBA colors
    return textureSample(font_texture, font_sampler, in.uv) * in.color;
}
