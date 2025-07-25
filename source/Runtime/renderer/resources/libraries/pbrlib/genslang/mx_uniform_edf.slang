import pbrlib.genslang.lib.mx_closure_type;
#define EDF float3

void mx_uniform_edf(ClosureData closureData, float3 color, out EDF result)
{
    if (closureData.closureType == CLOSURE_TYPE_EMISSION)
    {
        result = color;
    }
}
