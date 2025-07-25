import pbrlib.genslang.lib.mx_closure_type;
import stdlib.genslang.mx_bsdf;

void mx_add_bsdf(ClosureData closureData, BSDF in1, BSDF in2, out BSDF result)
{
    result.response = in1.response + in2.response;
    result.throughput = in1.throughput + in2.throughput;
}
