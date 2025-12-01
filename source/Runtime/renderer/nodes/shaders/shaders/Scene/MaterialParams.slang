#pragma once
import Scene.VertexInfo;
import utils.Math.ShadingFrame;
import Utils.Math.MathHelpers;

#include "utils/Math/MathConstants.slangh"
#include "utils/random.slangh"

import pbrlib.genslang.mx_roughness_anisotropy;
import pbrlib.genslang.lib.mx_microfacet;
import pbrlib.genslang.lib.mx_microfacet_specular;
import pbrlib.genslang.lib.mx_microfacet_diffuse;

// Aligned material parameter structures for standard_surface
// Each parameter is 16-byte aligned for GPU efficiency
struct StandardSurfaceMaterialParams {
    // Base parameters (48 bytes = 3 * float4)
    float base;
    float diffuse_roughness;
    float metalness;
    float specular;

    float3 base_color;
    float specular_roughness;

    float3 specular_color;
    float specular_IOR;

    // Specular detail (32 bytes = 2 * float4)
    float specular_anisotropy;
    float specular_rotation;
    float transmission;
    float transmission_depth;

    float3 transmission_color;
    float transmission_scatter_anisotropy;

    // Transmission detail (32 bytes = 2 * float4)
    float3 transmission_scatter;
    float transmission_dispersion;

    float transmission_extra_roughness;
    float subsurface;
    float subsurface_scale;
    float subsurface_anisotropy;

    // Subsurface (32 bytes = 2 * float4)
    float3 subsurface_color;
    float sheen;

    float3 subsurface_radius;
    float sheen_roughness;

    // Sheen (32 bytes = 2 * float4)
    float3 sheen_color;
    float coat;

    float3 coat_color;
    float coat_roughness;

    // Coat (48 bytes = 3 * float4)
    float coat_anisotropy;
    float coat_rotation;
    float coat_IOR;
    float coat_affect_color;

    float3 coat_normal;
    float coat_affect_roughness;

    float thin_film_thickness;
    float thin_film_IOR;
    float emission;
    uint thin_walled;

    // Emission and opacity (32 bytes = 2 * float4)
    float3 emission_color;
    float padding0;

    float3 opacity;
    float padding1;

    // Normals and tangents (32 bytes = 2 * float4)
    float3 normal;
    float padding2;

    float3 tangent;
    float padding3;

    // Total: 288 bytes = 18 * float4
};

// Aligned material parameter structure for preview_surface
// Each parameter is 16-byte aligned for GPU efficiency
struct PreviewSurfaceMaterialParams {
    // Colors (48 bytes = 3 * float4)
    float3 diffuseColor;
    float metallic;

    float3 emissiveColor;
    float roughness;

    float3 specularColor;
    float clearcoat;

    // Properties (64 bytes = 4 * float4)
    float clearcoatRoughness;
    float opacity;
    int opacityMode;
    float opacityThreshold;

    float ior;
    int useSpecularWorkflow;
    float padding0;
    float padding1;

    float3 normal;
    float displacement;

    float occlusion;
    float padding2;
    float padding3;
    float padding4;

    // Total: 112 bytes = 7 * float4
};
