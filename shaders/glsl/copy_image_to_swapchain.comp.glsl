#version 460

#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_descriptor_heap : require
#extension GL_EXT_shader_image_load_formatted : require

// layout(set = 0, binding = 0, rgba32f) uniform readonly image2D src_image;
// layout(set = 0, binding = 1, rgba8) uniform writeonly image2D dst_image;
layout(descriptor_heap) uniform image2D images[];

layout(push_constant) uniform PushConstant {
    uint image_width;
    uint image_height;
} pc;

layout (local_size_x = 32, local_size_y = 32, local_size_z = 1) in;
void main()
{
    ivec2 uv_coord = ivec2(gl_GlobalInvocationID.xy);
    if (uv_coord.x >= pc.image_width || uv_coord.y >= pc.image_height)
        return;

    imageStore(images[1], uv_coord, imageLoad(images[0], uv_coord));    
}