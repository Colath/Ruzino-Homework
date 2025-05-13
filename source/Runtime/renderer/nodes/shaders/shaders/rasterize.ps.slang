// Pixel shader for rasterization
#include "utils/view_cb.h"
import Scene.SceneTypes;

// Input structure from vertex shader
struct VSOutput {
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float3 worldPos : POSITION;

#if defined(ENABLE_TEXCOORD)
    float2 texcoord : TEXCOORD0;
#endif
};

// Main pixel shader function
void main(
    in VSOutput input,
    out float4 o_pos: SV_Target0,
    out float2 o_texcoord: SV_Target1,
    out float4 o_diffuse_color: SV_Target2,
    out float4 o_metallic_roughness: SV_Target3,
    out float4 o_normal: SV_Target4,
    out float depth: SV_Depth)
{
    // Output world position in the position buffer
    o_pos = float4(input.worldPos, 1.0);

    // Output normalized normal vector
    o_normal = float4(normalize(input.normal), 1.0);

    // Output depth value
    depth = input.position.z;

    // Output default material properties
    o_diffuse_color = float4(1, 0, 1, 1);  // Magenta as default color
    o_metallic_roughness =
        float4(0.5, 0.5, 0, 1);  // Medium metallic and roughness

#if defined(ENABLE_TEXCOORD)
    // Output texture coordinates if enabled
    o_texcoord = input.texcoord;
#else
    o_texcoord = float2(0, 0);
#endif
}
