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
    @location(2) @interpolate(flat) cell_bounds: vec4<f32>,
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

    // Calculate cell boundaries based on GRID_SIZE = 32
    let cell_size = 1.0 / 32.0;
    let col = floor(in.uv.x * 32.0);
    let row = floor(in.uv.y * 32.0);
    
    let u0 = col * cell_size;
    let v0 = row * cell_size;
    let u1 = (col + 1.0) * cell_size;
    let v1 = (row + 1.0) * cell_size;
    
    // Clamp uv to cell bounds with a 1-texel safety margin to prevent neighbor bleeding
    let inset = 1.0 / 2048.0;
    out.cell_bounds = vec4<f32>(u0 + inset, v0 + inset, u1 - inset, v1 - inset);
    
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    let clamped_uv = clamp(in.uv, in.cell_bounds.xy, in.cell_bounds.zw);

    // Force sampling at mip level 0 to avoid derivative-based filtering leaks
    let msd = textureSampleLevel(font_texture, font_sampler, clamped_uv, 0.0).rgb;
    let median = max(min(msd.r, msd.g), min(max(msd.r, msd.g), msd.b));
    
    // Isotropic anti-aliasing: convert MSDF distance to screen pixels using continuous UV derivatives
    let uv_width = fwidth(in.uv);
    let texel_width = uv_width * 2048.0;
    let screen_px_width = max(length(texel_width), 0.0001);
    
    // Threshold controls font weight (boldness): 0.50 is standard.
    let threshold = 0.50;
    
    // Clamp the transition width (in distance field units) to 0.9 to prevent ghost/background boxes
    // when zoomed out (large screen_px_width).
    let w = clamp(screen_px_width / 3.0, 0.0001, 0.9);
    let opacity = clamp((median - threshold) / w + 0.5, 0.0, 1.0);
    return vec4<f32>(in.color.rgb, in.color.a * opacity);
}

@fragment
fn fs_image(in: VertexOutput) -> @location(0) vec4<f32> {
    // Draw image sampling full RGBA colors
    return textureSample(font_texture, font_sampler, in.uv) * in.color;
}
