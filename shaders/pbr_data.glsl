layout (set = 0, binding = 0) uniform SceneData {
    mat4 view;
    mat4 proj;
    mat4 viewProj;
    vec4 ambientColor;
    vec4 lightDirection;
    vec4 lightColor;
    vec4 viewPosition;
} sceneData;

layout (set = 1, binding = 0) uniform GLTFMaterialData {
    vec4 baseColorFactor;      // 基础颜色因子
    vec4 emissiveFactor;       // 自发光因子
    vec4 metallicRoughnessFactor;  // x: 金属度, y: 粗糙度
    vec4 normalScale;          // x: 法线强度
    vec4 occlusionStrength;    // x: 环境光遮蔽强度
} materialData;

layout (set = 1, binding = 1) uniform sampler2D colorTexture;
layout (set = 1, binding = 2) uniform sampler2D metallicRoughnessTexture;
layout (set = 1, binding = 3) uniform sampler2D normalTexture;
layout (set = 1, binding = 4) uniform sampler2D emissiveTexture;
layout (set = 1, binding = 5) uniform sampler2D occlusionTexture;
