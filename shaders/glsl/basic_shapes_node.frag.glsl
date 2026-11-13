#version 460

#extension GL_GOOGLE_include_directive : require

#include "utils.glsl"

layout(location=0) in vec2 in_uv;

layout(location=0) out vec4 out_color;

void main()
{
   out_color = vec4(0.1, 0.1, 0.1, 1); 
}