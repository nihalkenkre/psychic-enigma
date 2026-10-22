#version 460

#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require

#include "utils.glsl"

layout (local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout (binding=0) writeonly uniform image2D final_image;
layout (rgba32f, binding=1) readonly uniform image2D gui_target;
layout (rgba32f, binding=0) readonly uniform image2D color_targets[];

layout(push_constant) uniform PushConstant {
    uint color_target_count;
} pc;

void main()
{
    ivec2 uv_coord = ivec2(gl_GlobalInvocationID.xy);
    vec4 gui_color = imageLoad(gui_target, uv_coord);
    vec4 color_target_color = vec4(0);

    for (uint i = 0; i < pc.color_target_count; ++i)
    {
        color_target_color += imageLoad(color_targets[i], uv_coord);
    }

    imageStore(final_image, uv_coord, color_target_color + gui_color);
}