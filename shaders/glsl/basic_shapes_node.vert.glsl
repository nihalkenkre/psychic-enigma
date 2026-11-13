#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

#include "utils.glsl"

layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec2 in_uv;

layout(location=0) out vec2 out_uv;

layout(buffer_reference, std430) readonly buffer UniformBlock {
    mat4 mvp;
};

layout(push_constant) uniform PushConstant {
    UniformBlock uniform_block_ptr;
} pc;

void main()
{
    gl_Position = pc.uniform_block_ptr.mvp * vec4(position, 1);
}