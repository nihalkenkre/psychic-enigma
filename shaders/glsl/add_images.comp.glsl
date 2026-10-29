#version 460

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_descriptor_heap : require

#include "utils.glsl"

layout(descriptor_heap, rgba8) uniform image2D images[];

layout(push_constant) uniform PushConstant {
    uint image_width;
    uint image_height; 
    uint color_target_count;
} pc;

layout (local_size_x = 32, local_size_y = 32, local_size_z = 1) in;
void main()
{
    ivec2 uv_coord = ivec2(gl_GlobalInvocationID.xy);
    if (uv_coord.x >= pc.image_width || uv_coord.y >= pc.image_height)
        return;

    vec4 gui_color = srgb_to_linear(imageLoad(images[pc.color_target_count], uv_coord));
    vec4 color_target_color = vec4(0);

    for (uint i = 0; i < pc.color_target_count; ++i)
    {
        color_target_color += imageLoad(images[i], uv_coord);
    }

    imageStore(images[pc.color_target_count + 1], uv_coord, color_target_color + gui_color);
}