float4 linear_to_srgb(float4 linear)
{
    return float4(
        linear.x <= 0.0031308 ? linear.x * 12.92 : 1.055 * pow(linear.x, 0.4166666) - 0.055,
        linear.y <= 0.0031308 ? linear.y * 12.92 : 1.055 * pow(linear.y, 0.4166666) - 0.055,
        linear.z <= 0.0031308 ? linear.z * 12.92 : 1.055 * pow(linear.z, 0.4166666) - 0.055,
        linear.w);
}

float4 srgb_to_linear(float4 srgb)
{
    return float4(
        srgb.x <= 0.04045 ? srgb.x / 12.92 : pow(((srgb.x + 0.055) / 1.055), 2.4),
        srgb.y <= 0.04045 ? srgb.y / 12.92 : pow(((srgb.y + 0.055) / 1.055), 2.4),
        srgb.z <= 0.04045 ? srgb.z / 12.92 : pow(((srgb.z + 0.055) / 1.055), 2.4),
        srgb.w);
}

struct PushConstants
{
    uint image_width;
    uint image_height;
    uint image_count;
}

[[vk::push_constant]]
ConstantBuffer<PushConstants> pc;

[[vk::binding(0, 0)]]
RWTexture2D<float4> final_target;

[[vk::binding(1, 0)]]
RWTexture2D<float4> gui_target;

[[vk::binding(2, 0)]]
RWTexture2DArray<float4> color_targets;

[shader("compute")]
[numthreads(32, 32, 1)]
void main(uint3 thread_id: SV_DispatchThreadID)
{
    if (thread_id.x >= pc.image_width || thread_id.y >= pc.image_height)
        return;

    float4 final_color = float4(0);

    for (uint i = 0; i < pc.image_count; ++i)
    {
        final_color += color_targets[uint3(thread_id.xy, i)];
    }

    float4 gui_color = srgb_to_linear(gui_target[thread_id.xy]);
    final_color += final_color * (1 - gui_color.a) + gui_color * gui_color.a;
    final_target[thread_id.xy] = final_color;
}
