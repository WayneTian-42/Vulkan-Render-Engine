#version 450

// 使用include指令
#extension GL_GOOGLE_include_directive : require
// 使用buffer_reference指令
#extension GL_EXT_buffer_reference : require

#include "init_structre.glsl"

layout (location = 0) out vec3 fragNormal;
layout (location = 1) out vec3 fragColor;
layout (location = 2) out vec2 fragUV;

struct Vertex {
    vec3 position;
    float uv_x;
    vec3 normal;
    float uv_y;
    vec4 color;
};

layout (buffer_reference, std430) buffer VertexBuffer {
    Vertex vertices[];
};

layout (push_constant) uniform constants {
    mat4 renderMatrix;
    VertexBuffer vertexBuffer;
} PushConstants;

void main() {
    Vertex vertex = PushConstants.vertexBuffer.vertices[gl_VertexIndex];
    
    vec4 position = vec4(vertex.position, 1.0);

    gl_Position = sceneData.viewProj * PushConstants.renderMatrix * position;

    fragNormal = (PushConstants.renderMatrix * vec4(vertex.normal, 0.0)).xyz;

    fragColor = vertex.color.rgb * materialData.colorFactors.rgb;

    fragUV = vec2(vertex.uv_x, vertex.uv_y);
}