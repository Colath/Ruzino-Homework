#pragma once

import Scene.VertexInfo;
import Scene.MaterialParams;
import Scene.StandardSurface;
import Scene.UsdPreviewSurface;
import utils.Math.ShadingFrame;
import Utils.Math.MathHelpers;
#include "utils/random.slangh"

// Import callable data definition
import callable_data;

// Declare MaterialParams struct locally to avoid importing MaterialParamsBuffer
// which would create a local root signature binding conflict
#ifdef HOST_CODE
#include "utils/Math/PackedFormats.h"
using uint = uint32_t;
#endif

struct MaterialParams {
    uint data[72];
};

// Access materialParamsBuffer from global root signature
// This is bound in the main shader, not in the callable's local root signature
RWStructuredBuffer<MaterialParams> materialParamsBuffer;

// Fallback evaluation for unsupported materials
[shader("callable")]
void eval_fallback(inout EvalCallableData data)
{
    float3 color3 = float3(0.8);  // Default color for fallback
    data.color =
        float4(color3 * max(0.0, dot(data.L, data.vertexInfo.normalW)), 1.0);

    data.sampledDir =
        sample_cosine_hemisphere_concentric(random_float2(data.seed), data.pdf);
    bool valid;
    ShadingFrame sf = ShadingFrame.createSafe(
        data.vertexInfo.normalW, float4(1, 0, 0, 1), valid);
    data.sampledDir = sf.fromLocal(data.sampledDir);

    data.throughput = float3(0.8, 0.8, 0.8) / 3.14159 *
                      max(0.0, dot(data.vertexInfo.normalW, data.sampledDir));
}

// Standard Surface evaluation
[shader("callable")]
void eval_standard_surface(inout EvalCallableData data)
{
    // Read parameters from buffer
    MaterialParams params_blob =
        materialParamsBuffer[data.material_params_index];
    StandardSurfaceMaterialParams params;

    // Copy data from buffer (assuming MaterialParams can be reinterpreted)
    // In practice, you might need proper unpacking
    params = reinterpret<StandardSurfaceMaterialParams>(params_blob);

    VertexData vd;
    vd.normalWorld = data.vertexInfo.normalW;
    vd.tangentWorld = data.vertexInfo.tangentW.xyz;
    vd.positionWorld = data.vertexInfo.posW;

    // Evaluate the BSDF with light direction L
    surfaceshader bsdf_result = surfaceshader(float3(0.0), float3(0.0));
    NG_standard_surface_surfaceshader_100(
        vd,
        data.L,
        data.V,
        data.eta_flipped,
        params.base,
        params.base_color,
        params.diffuse_roughness,
        params.metalness,
        params.specular,
        params.specular_color,
        params.specular_roughness,
        params.specular_IOR,
        params.specular_anisotropy,
        params.specular_rotation,
        params.transmission,
        params.transmission_color,
        params.transmission_depth,
        params.transmission_scatter,
        params.transmission_scatter_anisotropy,
        params.transmission_dispersion,
        params.transmission_extra_roughness,
        params.subsurface,
        params.subsurface_color,
        params.subsurface_radius,
        params.subsurface_scale,
        params.subsurface_anisotropy,
        params.sheen,
        params.sheen_color,
        params.sheen_roughness,
        params.coat,
        params.coat_color,
        params.coat_roughness,
        params.coat_anisotropy,
        params.coat_rotation,
        params.coat_IOR,
        params.coat_normal,
        params.coat_affect_color,
        params.coat_affect_roughness,
        params.thin_film_thickness,
        params.thin_film_IOR,
        params.emission,
        params.emission_color,
        params.opacity,
        params.thin_walled != 0,
        params.normal,
        params.tangent,
        bsdf_result);

    data.color = float4(bsdf_result.color, 1.0);

    // Sample the BSDF for next direction
    data.sampledDir = sample_standard_surface(
        vd,
        data.V,
        params.base,
        params.base_color,
        params.diffuse_roughness,
        params.metalness,
        params.specular,
        params.specular_color,
        params.specular_roughness,
        params.specular_IOR,
        params.specular_anisotropy,
        params.specular_rotation,
        params.transmission,
        params.transmission_color,
        params.transmission_depth,
        params.transmission_scatter,
        params.transmission_scatter_anisotropy,
        params.transmission_dispersion,
        params.transmission_extra_roughness,
        params.subsurface,
        params.subsurface_color,
        params.subsurface_radius,
        params.subsurface_scale,
        params.subsurface_anisotropy,
        params.sheen,
        params.sheen_color,
        params.sheen_roughness,
        params.coat,
        params.coat_color,
        params.coat_roughness,
        params.coat_anisotropy,
        params.coat_rotation,
        params.coat_IOR,
        params.coat_normal,
        params.coat_affect_color,
        params.coat_affect_roughness,
        params.thin_film_thickness,
        params.thin_film_IOR,
        params.emission,
        params.emission_color,
        params.opacity,
        params.thin_walled != 0,
        params.normal,
        params.tangent,
        data.eta_flipped,
        data.seed,
        data.pdf);

    // Evaluate BSDF again with sampled direction to get throughput
    surfaceshader weight_result = surfaceshader(float3(0.0), float3(0.0));
    NG_standard_surface_surfaceshader_100(
        vd,
        data.sampledDir,
        data.V,
        data.eta_flipped,
        params.base,
        params.base_color,
        params.diffuse_roughness,
        params.metalness,
        params.specular,
        params.specular_color,
        params.specular_roughness,
        params.specular_IOR,
        params.specular_anisotropy,
        params.specular_rotation,
        params.transmission,
        params.transmission_color,
        params.transmission_depth,
        params.transmission_scatter,
        params.transmission_scatter_anisotropy,
        params.transmission_dispersion,
        params.transmission_extra_roughness,
        params.subsurface,
        params.subsurface_color,
        params.subsurface_radius,
        params.subsurface_scale,
        params.subsurface_anisotropy,
        params.sheen,
        params.sheen_color,
        params.sheen_roughness,
        params.coat,
        params.coat_color,
        params.coat_roughness,
        params.coat_anisotropy,
        params.coat_rotation,
        params.coat_IOR,
        params.coat_normal,
        params.coat_affect_color,
        params.coat_affect_roughness,
        params.thin_film_thickness,
        params.thin_film_IOR,
        params.emission,
        params.emission_color,
        params.opacity,
        params.thin_walled != 0,
        params.normal,
        params.tangent,
        weight_result);

    data.throughput = weight_result.color;

}

// Preview Surface evaluation
[shader("callable")]
void eval_preview_surface(inout EvalCallableData data)
{
    // Read parameters from buffer
    MaterialParams params_blob =
        materialParamsBuffer[data.material_params_index];
    PreviewSurfaceMaterialParams params;

    // Reinterpret buffer data as PreviewSurfaceMaterialParams
    params = reinterpret<PreviewSurfaceMaterialParams>(params_blob);

    VertexData vd;
    vd.normalWorld = data.vertexInfo.normalW;
    vd.tangentWorld = data.vertexInfo.tangentW.xyz;
    vd.bitangentWorld =
        cross(data.vertexInfo.normalW, data.vertexInfo.tangentW.xyz) *
        data.vertexInfo.tangentW.w;
    vd.positionWorld = data.vertexInfo.posW;

    // Evaluate the BSDF with light direction L
    surfaceshader bsdf_result = surfaceshader(float3(0.0), float3(0.0));
    IMP_UsdPreviewSurface_surfaceshader(
        vd,
        data.L,
        data.V,
        data.eta_flipped,
        params.diffuseColor,
        params.emissiveColor,
        params.useSpecularWorkflow,
        params.specularColor,
        params.metallic,
        params.roughness,
        params.clearcoat,
        params.clearcoatRoughness,
        params.opacity,
        params.opacityMode,
        params.opacityThreshold,
        params.ior,
        params.normal,
        params.displacement,
        params.occlusion,
        bsdf_result);

    data.color = float4(bsdf_result.color, 1.0);

    // Sample the BSDF for next direction
    data.sampledDir = sample_preview_surface(
        vd,
        data.V,
        params.diffuseColor,
        params.emissiveColor,
        params.useSpecularWorkflow,
        params.specularColor,
        params.metallic,
        params.roughness,
        params.clearcoat,
        params.clearcoatRoughness,
        params.opacity,
        params.opacityMode,
        params.opacityThreshold,
        params.ior,
        params.normal,
        params.displacement,
        params.occlusion,
        data.eta_flipped,
        data.seed,
        data.pdf);

    // Evaluate BSDF again with sampled direction to get throughput
    surfaceshader weight_result = surfaceshader(float3(0.0), float3(0.0));
    IMP_UsdPreviewSurface_surfaceshader(
        vd,
        data.sampledDir,
        data.V,
        data.eta_flipped,
        params.diffuseColor,
        params.emissiveColor,
        params.useSpecularWorkflow,
        params.specularColor,
        params.metallic,
        params.roughness,
        params.clearcoat,
        params.clearcoatRoughness,
        params.opacity,
        params.opacityMode,
        params.opacityThreshold,
        params.ior,
        params.normal,
        params.displacement,
        params.occlusion,
        weight_result);

    data.throughput = weight_result.color;
}
