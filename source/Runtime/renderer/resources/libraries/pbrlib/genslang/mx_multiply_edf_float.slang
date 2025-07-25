import pbrlib.genslang.lib.mx_closure_type;
#define EDF float3
void mx_multiply_edf_float(ClosureData closureData, EDF in1, float in2, out EDF result)
{
    result = in1 * in2;
}
