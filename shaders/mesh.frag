#version 450

#extension GL_GOOGLE_include_directive : require
#include "init_structre.glsl"

layout (location = 0) in vec3 fragNormal;
layout (location = 1) in vec3 fragColor;
layout (location = 2) in vec2 fragUV;

layout (location = 0) out vec4 outColor;

void main() {
    float lightValue = max(dot(sceneData.lightDirection.xyz, fragNormal), 0.1);

    vec3 color = fragColor * texture(colorTexture, fragUV).rgb;
    vec3 ambient = color * sceneData.ambientColor.rgb;

    outColor = vec4(ambient + color * lightValue * sceneData.lightColor.w, 1.0);
}