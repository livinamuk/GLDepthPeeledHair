#version 460 core

layout (location = 0) in vec3 vPosition;
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec2 vUV;
layout (location = 3) in vec3 vTangent;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

out vec2 TexCoord;
out vec4 WorldPos;

void main() {
	TexCoord = vUV;
    WorldPos = model * vec4(vPosition, 1.0);
	gl_Position = projection * view * WorldPos;
}