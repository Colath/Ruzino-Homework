import stdlib.genslang.lib.mx_transform_uv;

void mx_image_float(
    SamplerState texture_sampler,
    Texture2D texture,
    int layer,
    float defaultval,
    float2 texcoord,
    int uaddressmode,
    int vaddressmode,
    int filtertype,
    int framerange,
    int frameoffset,
    int frameendaction,
    float2 uv_scale,
    float2 uv_offset,
    out float result)
{
    float2 uv = mx_transform_uv(texcoord, uv_scale, uv_offset);
    result = texture.Sample(texture_sampler, uv).r;
}
