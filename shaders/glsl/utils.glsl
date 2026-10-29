vec4 srgb_to_linear(vec4 srgb)
{
    return vec4(
        srgb.x <= 0.04045 ? srgb.x / 12.92 : pow(((srgb.x + 0.055) / 1.055), 2.4),
        srgb.y <= 0.04045 ? srgb.y / 12.92 : pow(((srgb.y + 0.055) / 1.055), 2.4),
        srgb.z <= 0.04045 ? srgb.z / 12.92 : pow(((srgb.z + 0.055) / 1.055), 2.4),
        srgb.w);
}

vec4 linear_to_srgb(vec4 linear)
{
    return vec4(
        linear.x <= 0.0031308 ? linear.x * 12.92 : 1.055 * pow(linear.x, 0.4166666) - 0.055,
        linear.y <= 0.0031308 ? linear.y * 12.92 : 1.055 * pow(linear.y, 0.4166666) - 0.055,
        linear.z <= 0.0031308 ? linear.z * 12.92 : 1.055 * pow(linear.z, 0.4166666) - 0.055,
        linear.w);
}