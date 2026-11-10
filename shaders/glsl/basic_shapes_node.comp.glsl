#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_buffer_reference : require

#include "utils.glsl"

layout(buffer_reference, buffer_reference_align=8) readonly buffer PositionData {
    vec3 position;
};

layout(buffer_reference, buffer_reference_align=8) readonly buffer VertexData {
    vec3 normal;
    vec3 tangent;
    vec2 uv0;
};

layout(buffer_reference, buffer_reference_align=8) readonly buffer TriangleData {
    uvec3 indices;
};

layout(push_constant) uniform PushConstant {
    PositionData positions;
    VertexData vertices;
    TriangleData triangles;
    
    uint triangle_count;
} pc;

void main()
{
    pc.positions.position = vec3(0, 0, 0);
}