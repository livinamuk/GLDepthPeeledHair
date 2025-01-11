#version 460 core

layout (location = 0) out vec4 FragOut;
layout (binding = 0) uniform sampler2D baseColorTexture;

in vec2 TexCoord;
in vec4 WorldPos;
uniform mat4 view;
uniform float nearPlane;
uniform float farPlane;

void main() {
    vec4 baseColor = texture2D(baseColorTexture, TexCoord);
    float threshold = 0.3;
    if (baseColor.a < threshold) {
       discard;
    }
    float viewspaceDepth = (view * WorldPos).z;
    float normalizedDepth = (viewspaceDepth - (-farPlane)) / ((-nearPlane) - (-farPlane));    
    FragOut.r = normalizedDepth;
}
