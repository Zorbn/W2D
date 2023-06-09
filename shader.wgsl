@group(0) @binding(0) var<uniform> projectionMatrix: mat4x4<f32>;
@group(0) @binding(1) var texture: texture_2d<f32>;
@group(0) @binding(2) var textureSampler: sampler;

struct VertexInput {
    @location(0) position: vec3<f32>,
    @location(1) color: vec4<f32>,
    @location(2) blend: f32,
    @location(3) textureCoords: vec2<f32>,
};

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) color: vec4<f32>,
    @location(1) blend: f32,
    @location(2) textureCoords: vec2<f32>,
}

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    out.position = projectionMatrix * vec4<f32>(in.position.x,
        in.position.y, in.position.z, 1.0);
    out.color = in.color;
    out.blend = in.blend;
    out.textureCoords = in.textureCoords;
    return out;

}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    let textureColor = textureSample(texture, textureSampler, in.textureCoords);

    if (textureColor.a == 0.0) {
        discard;
    }

    return mix(textureColor, in.color, in.blend);
}