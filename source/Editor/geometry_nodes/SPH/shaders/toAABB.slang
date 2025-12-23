
Buffer<float3> positions : register(t0);
RWBuffer<float3> AABBs : register(u0);

[numthreads(32, 1, 1)]
void expandAABB(int2 i_globalIdx : SV_DispatchThreadID)
{
    int i = i_globalIdx.x;
    float3 pos = positions[i].xyz;
    float3 aabb_min, aabb_max;
    float radius = 0.02;
    aabb_min = pos - float3(radius, radius, radius/2);
    aabb_max = pos + float3(radius, radius, radius/2);
    AABBs[2 * i] = aabb_min;
    AABBs[2 * i + 1] = aabb_max;
}
