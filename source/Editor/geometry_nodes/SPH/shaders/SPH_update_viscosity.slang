#include "utils.slangh"

ConstantBuffer<SPHConstants> sph_constants : register(b0);
StructuredBuffer<uint2> ContactPairs : register(t0);
Buffer<float3> positions : register(t1);
Buffer<float3> velocities : register(t2);
Buffer<uint> rho : register(t3);

RWStructuredBuffer<int> viscosity : register(u0);
RWBuffer<float> test : register(u1);


//pair_count
[numthreads(32, 1, 1)]
void update_viscosity(int2 i_globalIdx : SV_DispatchThreadID)
{
    if (i_globalIdx.x >= sph_constants.numPairs)
        return;
    float mV = sph_constants.mV;
    float h = sph_constants.smoothingRadius;
    float visc = sph_constants.constVisc;
    float rho_0 = sph_constants.restDensity;
    float surf = sph_constants.constSurf;
    float mass = mV * rho_0;
    
    uint u1 = ContactPairs[i_globalIdx.x].x;
    uint u2 = ContactPairs[i_globalIdx.x].y;
    float3 r = positions[u1] - positions[u2];
    float r2 = dot(r, r);
    float3 dv = velocities[u1] - velocities[u2];
    float rho_i = float(rho[u1]) / INT_SCALE;
    float rho_j = float(rho[u2]) / INT_SCALE;
    
    float3 DDv = 10 * visc * dot(r, dv) / (r2 + 0.01 * h * h) * cubic_kernel_derivative(r, h);
    float3 F_visc = mass * DDv;
    float3 f_surf;
    
    if (r2 > sph_constants.particleDiameter * sph_constants.particleDiameter)
        f_surf = surf *r * cubic_kernel(r, h);
    else
        f_surf =surf *r * cubic_kernel(float3(sph_constants.particleDiameter, 0, 0), h);
    
    int3 iF_visci = int3((F_visc / rho_j - f_surf) * INT_SCALE);
    int3 iF_viscj = int3(-(F_visc / rho_i - f_surf) * INT_SCALE);
    
        
    
    InterlockedAdd(viscosity[3 * u1 + 0], iF_visci.x);
    InterlockedAdd(viscosity[3 * u1 + 1], iF_visci.y);
    InterlockedAdd(viscosity[3 * u1 + 2], iF_visci.z);
    InterlockedAdd(viscosity[3 * u2 + 0], iF_viscj.x);
    InterlockedAdd(viscosity[3 * u2 + 1], iF_viscj.y);
    InterlockedAdd(viscosity[3 * u2 + 2], iF_viscj.z);
}
