#version 450

#extension GL_GOOGLE_include_directive : require
#include "pbr_data.glsl"

layout (location = 0) in vec3 fragNormal;
layout (location = 1) in vec3 fragColor;
layout (location = 2) in vec2 fragUV;
layout (location = 3) in vec3 fragPosition;
// todo: 需要使用TBN矩阵，但是顶点属性中没有切线
layout (location = 4) in mat3 TBNMatrix;

layout (location = 0) out vec4 outColor;

/**
* @brief 法线分布函数，基于Trowbridge-Reitz GGX模型估算 
* @param roughness 粗糙度
* @param normal 法线
* @param halfVector 半程向量
* @return 法线分布函数值
*/
float NormalDistributionFunction(float roughness, vec3 normal, vec3 halfVector) {
    float NdotH = max(dot(normal, halfVector), 0.0);
    float a2 = roughness * roughness;
    float NdotH2 = NdotH * NdotH;
    float denom = NdotH2 * (a2 - 1) + 1;
    float denom2 = denom * denom;
    return a2 / (3.14159265359 * denom2);
}

/**
* @brief 几何函数，基于Smith-Schlick-GGX模型估算
* @param roughness 粗糙度
* @param normal 法线
* @param view 视向量
* @return 几何函数值
*/
float GeometrySchlickGGX(float roughness, vec3 normal, vec3 view) {
    float kDirect = (roughness + 1) * (roughness + 1) / 8;
    float NdotV = max(dot(normal, view), 0.0);
    float denom = NdotV * (1 - kDirect) + kDirect;
    return NdotV / denom;
}

/**
* @brief 几何函数，基于Smith-GGX模型估算
* @param roughness 粗糙度
* @param normal 法线
* @param view 视向量
* @param light 光源方向
* @return 几何函数值
*/
float GeometrySmith(float roughness, vec3 normal, vec3 view, vec3 light) {
    float ggx1 = GeometrySchlickGGX(roughness, normal, view);
    float ggx2 = GeometrySchlickGGX(roughness, normal, light);
    return ggx1 * ggx2;
}

/**
* @brief 菲涅尔函数，基于Schlick近似模型估算
* @param F0 基础反射率
* @param view 视向量
* @param halfVector 半程向量
* @return 菲涅尔函数值
*/
vec3 FresnelFunction(vec3 F0, vec3 view, vec3 halfVector) {
    float HdotV = max(dot(halfVector, view), 0.0);
    return F0 + (1 - F0) * pow(1 - HdotV, 5);
}

void main() {
    // 使用顶点颜色和纹理颜色混合作为材质颜色
    vec3 color = fragColor * texture(colorTexture, fragUV).rgb;
    // vec3 color = texture(colorTexture, fragUV).rgb;
    // 基于全局环境光因子计算环境光
    vec3 ambient = color * sceneData.ambientColor.rgb;

    // pbr效果计算
    // float roughness = materialData.metallicRoughnessFactor.y;
    // 使用金属粗糙度纹理计算粗糙度，y通道存储粗糙度
    float roughness = texture(metallicRoughnessTexture, fragUV).g;
    // vec3 normal = normalize(fragNormal);
    // 使用法线纹理计算法线
    vec3 normal = texture(normalTexture, fragUV).rgb;
    normal = normalize(normal * 2.0 - 1.0);
    // 使用TBN矩阵计算法线
    // normal = normalize(TBNMatrix * normal);
    // 基于相机位置和顶点位置计算观察向量
    vec3 view = normalize(sceneData.viewPosition.xyz - fragPosition);
    // 计算半程向量
    vec3 halfVector = normalize(view + sceneData.lightDirection.xyz);

    // 计算法线分布函数
    float NDF = NormalDistributionFunction(roughness, normal, halfVector);
    // 计算几何函数
    float G = GeometrySmith(roughness, normal, view, sceneData.lightDirection.xyz);
    // 计算菲涅尔函数
    // vec3 F0 = vec3(0.04);
    // vec3 metallic = texture(metallicRoughnessTexture, fragUV).rgb;
    // F0 = mix(F0, metallic, materialData.metallicRoughnessFactor.x);
    // 使用金属粗糙度纹理计算菲涅尔函数，b通道存储菲涅尔函数
    float metallic = texture(metallicRoughnessTexture, fragUV).b;
    // vec3 F0 = vec3(metallic);
    vec3 F0 = mix(vec3(0.04f), color, metallic);
    vec3 F = FresnelFunction(F0, view, halfVector);

    // 计算镜面反射和漫反射系数
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    // 金属表面没有漫反射
    kD *= 1.0 - metallic;

    // 计算漫反射颜色
    vec3 diffuse = kD * color / 3.14159265359;

    // 计算镜面反射颜色
    float NdotL = max(dot(normal, sceneData.lightDirection.xyz), 0.0);
    float NdotV = max(dot(normal, view), 0.0);
    vec3 specular = (NDF * G * F) / (4 * NdotL * NdotV);

    // 计算最终颜色
    vec3 finalColor = (diffuse + specular) * sceneData.lightColor.rgb * sceneData.lightColor.w * NdotL;

    // 计算自发光颜色
    vec3 emissive = texture(emissiveTexture, fragUV).rgb * materialData.emissiveFactor.rgb;

    // 输出最终颜色
    outColor = vec4(ambient + finalColor + emissive, 1.0);
}