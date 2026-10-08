#version 450

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(push_constant) uniform PC {
    int ox;
    int oy;
    int oz;
    int cols;
    int rows;
} pc;

layout(location = 0) in uint inData0;
layout(location = 1) in uint inData1;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragUV;

void main() {
    uint x = inData0 & 63u;
    uint y = (inData0 >> 6) & 63u;
    uint z = (inData0 >> 12) & 63u;
    uint face = (inData0 >> 18) & 7u;
    uint tile = inData1 & 16383u;
    uint corner = (inData1 >> 14) & 3u;

    vec3 local = vec3(float(x), float(y), float(z));
    vec3 world = local + vec3(float(pc.ox), float(pc.oy), float(pc.oz));

    float shades[6] = float[6](0.8, 0.8, 1.0, 0.5, 0.7, 0.7);
    fragColor = vec3(shades[face]);

    uint col = tile % uint(pc.cols);
    uint row = tile / uint(pc.cols);
    vec2 c = vec2((corner == 1u || corner == 2u) ? 1.0 : 0.0,
                  corner < 2u ? 1.0 : 0.0);
    fragUV = (vec2(float(col), float(row)) + c) / vec2(float(pc.cols), float(pc.rows));

    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(world, 1.0);
}
