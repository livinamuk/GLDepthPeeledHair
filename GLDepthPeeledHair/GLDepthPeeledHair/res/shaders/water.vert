    #version 460 core

layout (location = 0) in vec3 vPosition;
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec2 vUV;
uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

out vec2 TexCoord;
out vec4 ClipSpace;
out vec3 WorldPos;


void main() {

    vec4 worldPos = model * vec4(vPosition, 1.0);

    ClipSpace = projection * view * model * vec4(vPosition, 1.0);
    gl_Position = ClipSpace;

    TexCoord = vUV;
    WorldPos = worldPos.xyz;


}