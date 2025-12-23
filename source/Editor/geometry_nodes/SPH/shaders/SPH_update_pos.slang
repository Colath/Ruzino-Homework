#include "donut/shaders/bindless.h"
#include "donut/shaders/packing.hlsli"
#include "donut/shaders/utils.hlsli"
#include "utils.slangh"

ConstantBuffer<SPHConstants> sph_constants : register(b0);

RWBuffer<float3> positions : register(u0);
RWBuffer<float3> velocities : register(u1);
RWBuffer<uint> rho : register(u2);

RWBuffer<int> pressure : register(u3);
RWBuffer<int> viscosity : register(u4);

RaytracingAccelerationStructure BoundaryBVH : register(t0);
StructuredBuffer<InstanceData> t_InstanceData : register(t1);
StructuredBuffer<GeometryData> t_GeometryData : register(t2);
StructuredBuffer<MaterialConstants> t_MaterialConstants : register(t3);

ByteAddressBuffer t_BindlessBuffers[] : register(t0, space1);
Texture2D t_BindlessTextures[] : register(t0, space2);

struct RayPayload
{
    float committedRayT;
    uint instanceID;
    uint primitiveIndex;
    uint geometryIndex;
    float2 barycentrics;
};


struct GeometrySample
{
    InstanceData instance;
    GeometryData geometry;
    MaterialConstants material;

    float3 vertexPositions[3];
    float2 vertexTexcoords[3];

    float3 objectSpacePosition;
    float2 texcoord;
    float3 flatNormal;
    float3 geometryNormal;
    float4 tangent;
};


GeometrySample getGeometryFromHit(
    uint instanceIndex,
    uint triangleIndex,
    uint geometryIndex,
    float2 rayBarycentrics,
    StructuredBuffer<InstanceData> instanceBuffer,
    StructuredBuffer<GeometryData> geometryBuffer,
    StructuredBuffer<MaterialConstants> materialBuffer)
{
    GeometrySample gs = (GeometrySample)0;

    gs.instance = instanceBuffer[instanceIndex];
    gs.geometry = geometryBuffer[gs.instance.firstGeometryIndex + geometryIndex];
    gs.material = materialBuffer[gs.geometry.materialIndex];

    ByteAddressBuffer indexBuffer =
        t_BindlessBuffers[NonUniformResourceIndex(gs.geometry.indexBufferIndex)];
    ByteAddressBuffer vertexBuffer =
        t_BindlessBuffers[NonUniformResourceIndex(gs.geometry.vertexBufferIndex)];

    float3 barycentrics;
    barycentrics.yz = rayBarycentrics;
    barycentrics.x = 1.0 - (barycentrics.y + barycentrics.z);

    uint3 indices =
        indexBuffer.Load3(gs.geometry.indexOffset + triangleIndex * c_SizeOfTriangleIndices);

    {
        gs.vertexPositions[0] =
            asfloat(vertexBuffer.Load3(gs.geometry.positionOffset + indices[0] * c_SizeOfPosition));
        gs.vertexPositions[1] =
            asfloat(vertexBuffer.Load3(gs.geometry.positionOffset + indices[1] * c_SizeOfPosition));
        gs.vertexPositions[2] =
            asfloat(vertexBuffer.Load3(gs.geometry.positionOffset + indices[2] * c_SizeOfPosition));
        gs.objectSpacePosition = interpolate(gs.vertexPositions, barycentrics);
    }

    {
        gs.vertexTexcoords[0] = asfloat(
            vertexBuffer.Load2(gs.geometry.texCoord1Offset + indices[0] * c_SizeOfTexcoord));
        gs.vertexTexcoords[1] = asfloat(
            vertexBuffer.Load2(gs.geometry.texCoord1Offset + indices[1] * c_SizeOfTexcoord));
        gs.vertexTexcoords[2] = asfloat(
            vertexBuffer.Load2(gs.geometry.texCoord1Offset + indices[2] * c_SizeOfTexcoord));
        gs.texcoord = interpolate(gs.vertexTexcoords, barycentrics);
    }

    {
        float3 normals[3];
        normals[0] = Unpack_RGB8_SNORM(
            vertexBuffer.Load(gs.geometry.normalOffset + indices[0] * c_SizeOfNormal));
        normals[1] = Unpack_RGB8_SNORM(
            vertexBuffer.Load(gs.geometry.normalOffset + indices[1] * c_SizeOfNormal));
        normals[2] = Unpack_RGB8_SNORM(
            vertexBuffer.Load(gs.geometry.normalOffset + indices[2] * c_SizeOfNormal));
        gs.geometryNormal = interpolate(normals, barycentrics);
        gs.geometryNormal = mul(gs.instance.transform, float4(gs.geometryNormal, 0.0)).xyz;
        gs.geometryNormal = normalize(gs.geometryNormal);
    }

    {
        float4 tangents[3];
        tangents[0] = Unpack_RGBA8_SNORM(
            vertexBuffer.Load(gs.geometry.tangentOffset + indices[0] * c_SizeOfNormal));
        tangents[1] = Unpack_RGBA8_SNORM(
            vertexBuffer.Load(gs.geometry.tangentOffset + indices[1] * c_SizeOfNormal));
        tangents[2] = Unpack_RGBA8_SNORM(
            vertexBuffer.Load(gs.geometry.tangentOffset + indices[2] * c_SizeOfNormal));
        gs.tangent.xyz = interpolate(tangents, barycentrics).xyz;
        gs.tangent.xyz = mul(gs.instance.transform, float4(gs.tangent.xyz, 0.0)).xyz;
        gs.tangent.xyz = normalize(gs.tangent.xyz);
        gs.tangent.w = tangents[0].w;
    }

    float3 objectSpaceFlatNormal = normalize(cross(
        gs.vertexPositions[1] - gs.vertexPositions[0],
        gs.vertexPositions[2] - gs.vertexPositions[0]));

    gs.flatNormal = normalize(mul(gs.instance.transform, float4(objectSpaceFlatNormal, 0.0)).xyz);

    return gs;
}

//particle_count
[numthreads(32, 1, 1)] void update_pos(int2 i_globalIdx
                                       : SV_DispatchThreadID)
{
    if (i_globalIdx.x >= sph_constants.numParticles)
        return;

    float3 gravity = float3(0, sph_constants.gravity, 0);

    float dt = sph_constants.dt;
    int3 iF_pres = int3(
        pressure[3 * i_globalIdx.x],
        pressure[3 * i_globalIdx.x + 1],
        pressure[3 * i_globalIdx.x + 2]);
    int3 iF_visc = int3(
        viscosity[3 * i_globalIdx.x],
        viscosity[3 * i_globalIdx.x + 1],
        viscosity[3 * i_globalIdx.x + 2]);
    float3 F_pres = float3(iF_pres) / INT_SCALE;
    float3 F_visc = float3(iF_visc) / INT_SCALE;
    float3 pos_o = positions[i_globalIdx.x];

    RayDesc ray;

    ray.Origin = pos_o;
    ray.Direction = velocities[i_globalIdx.x];
    ray.TMin = 0;
    ray.TMax = 100;

    RayQuery<RAY_FLAG_NONE> rayQuery;
    rayQuery.TraceRayInline(BoundaryBVH, RAY_FLAG_NONE, 0xff, ray);

    rayQuery.Proceed();

    RayPayload payload;

    if (rayQuery.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
    {
        payload.instanceID = rayQuery.CommittedInstanceID();
        payload.primitiveIndex = rayQuery.CommittedPrimitiveIndex();
        payload.geometryIndex = rayQuery.CommittedGeometryIndex();
        payload.barycentrics = rayQuery.CommittedTriangleBarycentrics();
        payload.committedRayT = rayQuery.CommittedRayT();

        
        GeometrySample gs = getGeometryFromHit(
            payload.instanceID,
            payload.primitiveIndex,
            payload.geometryIndex,
            payload.barycentrics,
            t_InstanceData,
            t_GeometryData,
            t_MaterialConstants);

        float3 normal = gs.geometryNormal;
    }


    velocities[i_globalIdx.x] += dt * (F_pres + F_visc + gravity);
    positions[i_globalIdx.x] += dt * velocities[i_globalIdx.x];

    //positions[i_globalIdx.x] = max(positions[i_globalIdx.x], float3(0, 0, 0));
    //positions[i_globalIdx.x] = min(positions[i_globalIdx.x], boundary);





    float3 boundary = float3(1,1,1);

    int3 minflag = int3(positions[i_globalIdx.x] < float3(0, 0, 0));
    int3 maxflag = int3(positions[i_globalIdx.x] > boundary);
    positions[i_globalIdx.x] = -2 * positions[i_globalIdx.x] * minflag + positions[i_globalIdx.x];
    positions[i_globalIdx.x] = -2 * (positions[i_globalIdx.x] - boundary) * maxflag + positions[i_globalIdx.x];
    velocities[i_globalIdx.x] = (1 - 2 * (minflag + maxflag) * sqrt(0.7)) * velocities[i_globalIdx.x];
    
    velocities[i_globalIdx.x] = (positions[i_globalIdx.x] - pos_o) / dt;
    
    rho[i_globalIdx.x] = 0;
    pressure[3 * i_globalIdx.x] = 0;
    pressure[3 * i_globalIdx.x + 1] = 0;
    pressure[3 * i_globalIdx.x + 2] = 0;
    viscosity[3 * i_globalIdx.x] = 0;
    viscosity[3 * i_globalIdx.x + 1] = 0;
    viscosity[3 * i_globalIdx.x + 2] = 0;
    

}
