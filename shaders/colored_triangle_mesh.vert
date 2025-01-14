#version 450
// 启用buffer reference扩展，允许在着色器中使用buffer reference
// 使用buffer reference，可以减少内存的拷贝，提高性能
#extension GL_EXT_buffer_reference : require

layout (location = 0) out vec3 fragColor;
layout (location = 1) out vec2 fragUV;

struct Vertex {
    vec3 position;
    float uv_x;
    vec3 normal;
    float uv_y;
    vec4 color;
};

layout(buffer_reference, std430) readonly buffer VertexBuffer {
    Vertex vertices[];
};

layout (push_constant) uniform constants {
    mat4 render_matrix;
    VertexBuffer vertex_buffer;
} PushConstants;

void main() 
{
    // 获取顶点数据
    Vertex vertex = PushConstants.vertex_buffer.vertices[gl_VertexIndex];

    // 计算顶点位置
    gl_Position = PushConstants.render_matrix * vec4(vertex.position, 1.0);
    fragColor = vertex.color.rgb;
    fragUV = vec2(vertex.uv_x, vertex.uv_y);
}
