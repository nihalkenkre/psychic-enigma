#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_buffer_reference : require

#include "utils.glsl"

layout(buffer_reference, buffer_reference_align=8) writeonly buffer MeshData {
    vec3 positions[];
    vec3 normals[];
    vec3 tangents[];
    vec2 uv0s[];
    
    uint indices[];
};

layout(push_constant) uniform PushConstant {
    MeshData mesh_data;   
    uint selected_shape; // 0 - Cube, 1 - Sphere
} pc;

void main()
{
    pc.mesh_data.positions[0] = vec3(0, 0, 0);
}