//density_hlsl
#include "utils.slangh"
    

ConstantBuffer<SPHConstants> sph_constants : register(b0);
Buffer<float3> positions : register(t0);
StructuredBuffer<uint2> ContactPairs : register(t1);
RWBuffer<uint> rho : register(u0);




//pair_count
[numthreads(32, 1, 1)]
void update_density(int2 i_globalIdx : SV_DispatchThreadID)
{
    if (i_globalIdx.x >= sph_constants.numPairs) return;
    float mV = sph_constants.mV;
    float rho0 = sph_constants.restDensity;
    float h = sph_constants.smoothingRadius;
    
    uint u1 = ContactPairs[i_globalIdx.x].x;
    uint u2 = ContactPairs[i_globalIdx.x].y;
    float3 r = positions[u1] - positions[u2];

    float Drho = mV * cubic_kernel(r, h);
    uint iDrho = uint(Drho * INT_SCALE * rho0);
    InterlockedAdd(rho[u1], iDrho);
    InterlockedAdd(rho[u2], iDrho);

}


//particle_count
[numthreads(32, 1, 1)]
void update_density_once(int2 i_globalIdx : SV_DispatchThreadID)
{
    if (i_globalIdx.x >= sph_constants.numParticles)
        return;
    float mV = sph_constants.mV;
    float h = sph_constants.smoothingRadius;
    float rho0 = sph_constants.restDensity;
    float Drho = mV * cubic_kernel(float3(0, 0, 0), h);
    int iDrho = int(Drho * INT_SCALE * rho0);
    //InterlockedAdd(rho[i_globalIdx.x], iDrho);
    rho[i_globalIdx.x] += iDrho;

}
