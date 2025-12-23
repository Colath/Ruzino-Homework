//pressure_hlsl
#include "utils.slangh"

ConstantBuffer<SPHConstants> sph_constants : register(b0);

StructuredBuffer<uint2> ContactPairs : register(t0);
Buffer<float3> positions : register(t1);

RWBuffer<int> pressure : register(u0);
RWBuffer<uint> rho : register(u1);
RWBuffer<float> test : register(u2);



//pair_count
[numthreads(32, 1, 1)]
void update_pressure(int2 i_globalIdx : SV_DispatchThreadID)
{
    if (i_globalIdx.x >= sph_constants.numPairs)
        return;
    float mV = sph_constants.mV;
    float h = sph_constants.smoothingRadius;
    float rho_0 = sph_constants.restDensity;
    float press_k = sph_constants.constPress;
    
    uint u1 = ContactPairs[i_globalIdx.x].x;
    uint u2 = ContactPairs[i_globalIdx.x].y;
    float3 r = positions[u1] - positions[u2];
    float rho_i = max(float(rho[u1]) / INT_SCALE, rho_0);
    float rho_j = max(float(rho[u2]) / INT_SCALE, rho_0);
    float p_i = press_k * (pow(max(rho_i / rho_0, 1), 2) - 1);
    float p_j = press_k * (pow(max(rho_j / rho_0, 1), 2) - 1);
    float3 Dp = rho_0 * mV * (p_i / rho_i / rho_i + p_j / rho_j / rho_j) * cubic_kernel_derivative(r, h);
    float3 F_pres_1 = -Dp;    
    float3 F_pres_2 =  Dp;
    int3 iF_pres_1 = int3(F_pres_1 * INT_SCALE);
    int3 iF_pres_2 = int3(F_pres_2 * INT_SCALE);

    InterlockedAdd(pressure[3 * u1 + 0], iF_pres_1.x);
    InterlockedAdd(pressure[3 * u1 + 1], iF_pres_1.y);
    InterlockedAdd(pressure[3 * u1 + 2], iF_pres_1.z);
    InterlockedAdd(pressure[3 * u2 + 0], iF_pres_2.x);
    InterlockedAdd(pressure[3 * u2 + 1], iF_pres_2.y);
    InterlockedAdd(pressure[3 * u2 + 2], iF_pres_2.z);
}
