#include "light.h"

#include <spdlog/spdlog.h>
#include "pxr/imaging/glf/simpleLight.h"
#include "pxr/imaging/hd/changeTracker.h"
#include "pxr/imaging/hd/rprimCollection.h"
#include "pxr/imaging/hd/sceneDelegate.h"
#include "pxr/imaging/hio/image.h"
#include "pxr/usd/sdr/shaderNode.h"
#include "pxr/usd/usd/tokens.h"
#include "pxr/usdImaging/usdImaging/tokens.h"
#include "renderParam.h"
#include "../nodes/shaders/shaders/Scene/Lights/LightData.slang"

USTC_CG_NAMESPACE_OPEN_SCOPE
using namespace pxr;
void Hd_USTC_CG_Light::Sync(
    HdSceneDelegate* sceneDelegate,
    HdRenderParam* renderParam,
    HdDirtyBits* dirtyBits)
{
    TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    TF_UNUSED(renderParam);

    if (!TF_VERIFY(sceneDelegate != nullptr)) {
        return;
    }

    const SdfPath& id = GetId();

    // HdStLight communicates to the scene graph and caches all interesting
    // values within this class. Later on Get() is called from
    // TaskState (RenderPass) to perform aggregation/pre-computation,
    // in order to make the shader execution efficient.

    // Change tracking
    HdDirtyBits bits = *dirtyBits;

    // Transform
    if (bits & DirtyTransform) {
        _params[HdTokens->transform] = VtValue(sceneDelegate->GetTransform(id));
    }

    // Lighting Params
    if (bits & DirtyParams) {
        HdChangeTracker& changeTracker =
            sceneDelegate->GetRenderIndex().GetChangeTracker();

        // Remove old dependencies
        VtValue val = Get(HdTokens->filters);
        if (val.IsHolding<SdfPathVector>()) {
            auto lightFilterPaths = val.UncheckedGet<SdfPathVector>();
            for (const SdfPath& filterPath : lightFilterPaths) {
                changeTracker.RemoveSprimSprimDependency(filterPath, id);
            }
        }

        if (_lightType == HdPrimTypeTokens->simpleLight) {
            _params[HdLightTokens->params] =
                sceneDelegate->Get(id, HdLightTokens->params);
        }
        // else if (_lightType == HdPrimTypeTokens->domeLight)
        //{
        //     _params[HdLightTokens->params] =
        //         _PrepareDomeLight(id, sceneDelegate);
        // }
        //// If it is an area light we will extract the parameters and convert
        //// them to a GlfSimpleLight that approximates the light source.
        // else
        //{
        //     _params[HdLightTokens->params] =
        //         _ApproximateAreaLight(id, sceneDelegate);
        // }

        // Add new dependencies
        val = Get(HdTokens->filters);
        if (val.IsHolding<SdfPathVector>()) {
            auto lightFilterPaths = val.UncheckedGet<SdfPathVector>();
            for (const SdfPath& filterPath : lightFilterPaths) {
                changeTracker.AddSprimSprimDependency(filterPath, id);
            }
        }
    }

    if (bits & (DirtyTransform | DirtyParams)) {
        auto transform = Get(HdTokens->transform).GetWithDefault<GfMatrix4d>();
        // Update cached light objects.  Note that simpleLight ignores
        // scene-delegate transform, in favor of the transform passed in by
        // params...
        if (_lightType == HdPrimTypeTokens->domeLight) {
            // Apply domeOffset if present
            VtValue domeOffset = sceneDelegate->GetLightParamValue(
                id, HdLightTokens->domeOffset);
            if (domeOffset.IsHolding<GfMatrix4d>()) {
                transform = domeOffset.UncheckedGet<GfMatrix4d>() * transform;
            }
            auto light =
                Get(HdLightTokens->params).GetWithDefault<GlfSimpleLight>();
            light.SetTransform(transform);
            _params[HdLightTokens->params] = VtValue(light);
        }
        else if (_lightType != HdPrimTypeTokens->simpleLight) {
            // e.g. area light
            auto light =
                Get(HdLightTokens->params).GetWithDefault<GlfSimpleLight>();
            GfVec3f p = GfVec3f(transform.ExtractTranslation());
            GfVec4f pos(p[0], p[1], p[2], 1.0f);
            // Convention is to emit light along -Z
            GfVec4d zDir = GfVec4f(transform.GetRow(2));
            if (_lightType == HdPrimTypeTokens->rectLight ||
                _lightType == HdPrimTypeTokens->diskLight) {
                light.SetSpotDirection(GfVec3f(-zDir[0], -zDir[1], -zDir[2]));
            }
            else if (_lightType == HdPrimTypeTokens->distantLight) {
                // For a distant light, translate to +Z homogeneous limit
                // See simpleLighting.glslfx : integrateLightsDefault.
                pos = GfVec4f(zDir[0], zDir[1], zDir[2], 0.0f);
            }
            else if (_lightType == HdPrimTypeTokens->sphereLight) {
                _params[HdLightTokens->radius] =
                    sceneDelegate->GetLightParamValue(
                        id, HdLightTokens->radius);
            }
            auto diffuse =
                sceneDelegate->GetLightParamValue(id, HdLightTokens->diffuse)
                    .Get<float>();
            auto color =
                sceneDelegate->GetLightParamValue(id, HdLightTokens->color)
                    .Get<GfVec3f>() *
                diffuse;
            light.SetDiffuse(GfVec4f(color[0], color[1], color[2], 0));
            light.SetPosition(pos);
            _params[HdLightTokens->params] = VtValue(light);
        }
    }

    // Shadow Params
    if (bits & DirtyShadowParams) {
        _params[HdLightTokens->shadowParams] =
            sceneDelegate->GetLightParamValue(id, HdLightTokens->shadowParams);
    }

    // Shadow Collection
    if (bits & DirtyCollection) {
        VtValue vtShadowCollection = sceneDelegate->GetLightParamValue(
            id, HdLightTokens->shadowCollection);

        // Optional
        if (vtShadowCollection.IsHolding<HdRprimCollection>()) {
            auto newCollection =
                vtShadowCollection.UncheckedGet<HdRprimCollection>();

            if (_params[HdLightTokens->shadowCollection] != newCollection) {
                _params[HdLightTokens->shadowCollection] =
                    VtValue(newCollection);

                HdChangeTracker& changeTracker =
                    sceneDelegate->GetRenderIndex().GetChangeTracker();

                changeTracker.MarkCollectionDirty(newCollection.GetName());
            }
        }
        else {
            _params[HdLightTokens->shadowCollection] =
                VtValue(HdRprimCollection());
        }    }

    // Don't clear dirty bits here - let derived classes handle it
}

HdDirtyBits Hd_USTC_CG_Light::GetInitialDirtyBitsMask() const
{
    // In the case of simple and distant lights we want to sync all dirty bits,
    // but for area lights coming from the scenegraph we just want to extract
    // the Transform and Params for now.
    if (_lightType == HdPrimTypeTokens->simpleLight ||
        _lightType == HdPrimTypeTokens->distantLight) {
        return AllDirty;
    }
    else {
        return (DirtyParams | DirtyTransform);
    }
}

VtValue Hd_USTC_CG_Light::Get(const TfToken& token) const
{
    VtValue val;
    TfMapLookup(_params, token, &val);
    return val;
}

void Hd_USTC_CG_Simple_Light::Sync(
    HdSceneDelegate* sceneDelegate,
    HdRenderParam* renderParam,
    HdDirtyBits* dirtyBits)
{
    Hd_USTC_CG_Light::Sync(sceneDelegate, renderParam, dirtyBits);
    
    // Allocate and populate light buffer for simple/point light
    if (*dirtyBits & (DirtyParams | DirtyTransform)) {
        auto* render_param = static_cast<Hd_USTC_CG_RenderParam*>(renderParam);
        if (!this->light_buffer) {
            this->light_buffer = render_param->InstanceCollection->light_pool.allocate(1);
        }
        
        const SdfPath& id = this->GetId();
        LightData lightData;
        lightData.type = (uint32_t)LightType::Point;
        
        // Get transform
        auto transform = this->Get(HdTokens->transform).GetWithDefault<GfMatrix4d>();
        GfVec3d pos = transform.ExtractTranslation();
        lightData.posW = float3(pos[0], pos[1], pos[2]);
        
        // Get color and intensity with standard USD Light API
        auto diffuse = sceneDelegate->GetLightParamValue(id, HdLightTokens->diffuse).GetWithDefault<float>(1.0f);
        auto color = sceneDelegate->GetLightParamValue(id, HdLightTokens->color).GetWithDefault<GfVec3f>(GfVec3f(1,1,1));
        auto intensity = sceneDelegate->GetLightParamValue(id, HdLightTokens->intensity).GetWithDefault<float>(1.0f);
        auto exposure = sceneDelegate->GetLightParamValue(id, HdLightTokens->exposure).GetWithDefault<float>(0.0f);
        
        float finalIntensity = intensity * pow(2.0f, exposure);
        lightData.intensity = float3(color[0], color[1], color[2]) * diffuse * finalIntensity;
        
        spdlog::info("PointLight {}: pos=({},{},{}), color=({},{},{}), intensity={}, exposure={}", 
                     id.GetText(), pos[0], pos[1], pos[2], color[0], color[1], color[2], intensity, exposure);
        
        this->light_buffer->write_data(&lightData);
    }
    
    // Clear dirty bits
    *dirtyBits = Clean;
}

void Hd_USTC_CG_Distant_Light::Sync(
    HdSceneDelegate* sceneDelegate,
    HdRenderParam* renderParam,
    HdDirtyBits* dirtyBits)
{
    Hd_USTC_CG_Light::Sync(sceneDelegate, renderParam, dirtyBits);
    
    // Get distant light specific parameters
    const SdfPath& id = this->GetId();
    HdDirtyBits bits = *dirtyBits;
    
    if (bits & (DirtyTransform | DirtyParams)) {
        auto transform = this->Get(HdTokens->transform).GetWithDefault<GfMatrix4d>();
        
        // Extract direction from transform
        GfVec4d zDir = GfVec4f(transform.GetRow(2));
        _direction = GfVec3f(zDir[0], zDir[1], zDir[2]);
        
        // Get angle parameter if available
        VtValue angleValue = sceneDelegate->GetLightParamValue(id, HdLightTokens->angle);
        if (!angleValue.IsEmpty()) {
            _angle = angleValue.Get<float>();
        }
        
        // Allocate and populate light buffer
        auto* render_param = static_cast<Hd_USTC_CG_RenderParam*>(renderParam);
        if (!this->light_buffer) {
            this->light_buffer = render_param->InstanceCollection->light_pool.allocate(1);
        }
        
        LightData lightData;
        lightData.type = (uint32_t)LightType::Distant;
        lightData.dirW = float3(_direction[0], _direction[1], _direction[2]);
        lightData.cosSubtendedAngle = cos(_angle);
        
        // Get color and intensity with exposure
        auto diffuse = sceneDelegate->GetLightParamValue(id, HdLightTokens->diffuse).GetWithDefault<float>(1.0f);
        auto color = sceneDelegate->GetLightParamValue(id, HdLightTokens->color).GetWithDefault<GfVec3f>(GfVec3f(1,1,1));
        auto intensity = sceneDelegate->GetLightParamValue(id, HdLightTokens->intensity).GetWithDefault<float>(1.0f);
        auto exposure = sceneDelegate->GetLightParamValue(id, HdLightTokens->exposure).GetWithDefault<float>(0.0f);
        
        float finalIntensity = intensity * pow(2.0f, exposure);
        lightData.intensity = float3(color[0], color[1], color[2]) * diffuse * finalIntensity;
        
        this->light_buffer->write_data(&lightData);
    }
    
    // Clear dirty bits
    *dirtyBits = Clean;
}

void Hd_USTC_CG_Sphere_Light::Sync(
    HdSceneDelegate* sceneDelegate,
    HdRenderParam* renderParam,
    HdDirtyBits* dirtyBits)
{
    Hd_USTC_CG_Light::Sync(sceneDelegate, renderParam, dirtyBits);
    
    // Get sphere light specific parameters
    const SdfPath& id = this->GetId();
    HdDirtyBits bits = *dirtyBits;
    
    if (bits & DirtyParams) {
        VtValue radiusValue = sceneDelegate->GetLightParamValue(id, HdLightTokens->radius);
        if (!radiusValue.IsEmpty()) {
            _radius = radiusValue.Get<float>();
        }
    }
    
    // Allocate and populate light buffer
    if (bits & (DirtyParams | DirtyTransform)) {
        auto* render_param = static_cast<Hd_USTC_CG_RenderParam*>(renderParam);
        if (!this->light_buffer) {
            this->light_buffer = render_param->InstanceCollection->light_pool.allocate(1);
        }
        
        LightData lightData;
        lightData.type = (uint32_t)LightType::Sphere;
        
        // Get transform
        auto transform = this->Get(HdTokens->transform).GetWithDefault<GfMatrix4d>();
        GfVec3d pos = transform.ExtractTranslation();
        lightData.posW = float3(pos[0], pos[1], pos[2]);
        
        // Get color and intensity with exposure
        auto diffuse = sceneDelegate->GetLightParamValue(id, HdLightTokens->diffuse).GetWithDefault<float>(1.0f);
        auto color = sceneDelegate->GetLightParamValue(id, HdLightTokens->color).GetWithDefault<GfVec3f>(GfVec3f(1,1,1));
        auto intensity = sceneDelegate->GetLightParamValue(id, HdLightTokens->intensity).GetWithDefault<float>(1.0f);
        auto exposure = sceneDelegate->GetLightParamValue(id, HdLightTokens->exposure).GetWithDefault<float>(0.0f);
        
        float finalIntensity = intensity * pow(2.0f, exposure);
        lightData.intensity = float3(color[0], color[1], color[2]) * diffuse * finalIntensity;
        
        // Sphere-specific parameters
        // Compute surface area for future use in importance sampling
        lightData.surfaceArea = 4.0f * 3.14159265359f * _radius * _radius;
        
        this->light_buffer->write_data(&lightData);
    }
    
    // Clear dirty bits
    *dirtyBits = Clean;
}

void Hd_USTC_CG_Rect_Light::Sync(
    HdSceneDelegate* sceneDelegate,
    HdRenderParam* renderParam,
    HdDirtyBits* dirtyBits)
{
    Hd_USTC_CG_Light::Sync(sceneDelegate, renderParam, dirtyBits);
    
    // Get rectangle light specific parameters
    const SdfPath& id = this->GetId();
    HdDirtyBits bits = *dirtyBits;
    
    if (bits & DirtyParams) {
        VtValue widthValue = sceneDelegate->GetLightParamValue(id, HdLightTokens->width);
        if (!widthValue.IsEmpty()) {
            _width = widthValue.Get<float>();
        }
        
        VtValue heightValue = sceneDelegate->GetLightParamValue(id, HdLightTokens->height);
        if (!heightValue.IsEmpty()) {
            _height = heightValue.Get<float>();
        }
    }
    
    // Allocate and populate light buffer
    if (bits & (DirtyParams | DirtyTransform)) {
        auto* render_param = static_cast<Hd_USTC_CG_RenderParam*>(renderParam);
        if (!this->light_buffer) {
            this->light_buffer = render_param->InstanceCollection->light_pool.allocate(1);
        }
        
        LightData lightData;
        lightData.type = (uint32_t)LightType::Rect;
        
        // Get transform
        auto transform = this->Get(HdTokens->transform).GetWithDefault<GfMatrix4d>();
        GfVec3d pos = transform.ExtractTranslation();
        lightData.posW = float3(pos[0], pos[1], pos[2]);
        
        // Get direction (rectangle emits along -Z in local space)
        GfVec4d zDir = transform.GetRow(2);
        float3 dir = float3(-zDir[0], -zDir[1], -zDir[2]);
        float dirLen = sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
        lightData.dirW = float3(dir.x / dirLen, dir.y / dirLen, dir.z / dirLen); // Normalized
        
        // Get tangent and bitangent for rectangle orientation
        // Extract normalized directions and compute scaled tangent/bitangent
        GfVec4d xDir = transform.GetRow(0);
        GfVec4d yDir = transform.GetRow(1);
        
        // Normalize and scale by width/height
        float3 xVec = float3(xDir[0], xDir[1], xDir[2]);
        float3 yVec = float3(yDir[0], yDir[1], yDir[2]);
        float xLen = sqrt(xVec.x * xVec.x + xVec.y * xVec.y + xVec.z * xVec.z);
        float yLen = sqrt(yVec.x * yVec.x + yVec.y * yVec.y + yVec.z * yVec.z);
        
        lightData.tangent = float3(xVec.x / xLen, xVec.y / xLen, xVec.z / xLen) * _width;
        lightData.bitangent = float3(yVec.x / yLen, yVec.y / yLen, yVec.z / yLen) * _height;
        
        // Get color and intensity with exposure
        auto diffuse = sceneDelegate->GetLightParamValue(id, HdLightTokens->diffuse).GetWithDefault<float>(1.0f);
        auto color = sceneDelegate->GetLightParamValue(id, HdLightTokens->color).GetWithDefault<GfVec3f>(GfVec3f(1,1,1));
        auto intensity = sceneDelegate->GetLightParamValue(id, HdLightTokens->intensity).GetWithDefault<float>(1.0f);
        auto exposure = sceneDelegate->GetLightParamValue(id, HdLightTokens->exposure).GetWithDefault<float>(0.0f);
        
        // Combine intensity with exposure: intensity * 2^exposure
        float finalIntensity = intensity * pow(2.0f, exposure);
        lightData.intensity = float3(color[0], color[1], color[2]) * diffuse * finalIntensity;
        
        // Debug output
        spdlog::info("RectLight {}: width={}, height={}, color=({},{},{}), intensity={}, exposure={}, finalIntensity={}", 
                     id.GetText(), _width, _height, color[0], color[1], color[2], intensity, exposure, finalIntensity);
        
        // Rectangle area
        lightData.surfaceArea = _width * _height;
        
        // Store transformation matrix for potential texture mapping
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                lightData.transMat[i][j] = transform[i][j];
            }
        }
        
        // Compute inverse transpose for normal transformation
        GfMatrix4d invTranspose = transform.GetInverse().GetTranspose();
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                lightData.transMatIT[i][j] = invTranspose[i][j];
            }
        }
        
        this->light_buffer->write_data(&lightData);
    }
    
    // Clear dirty bits
    *dirtyBits = Clean;
}

void Hd_USTC_CG_Disk_Light::Sync(
    HdSceneDelegate* sceneDelegate,
    HdRenderParam* renderParam,
    HdDirtyBits* dirtyBits)
{
    Hd_USTC_CG_Light::Sync(sceneDelegate, renderParam, dirtyBits);
    
    // Get disk light specific parameters
    const SdfPath& id = this->GetId();
    HdDirtyBits bits = *dirtyBits;
    
    if (bits & DirtyParams) {
        VtValue radiusValue = sceneDelegate->GetLightParamValue(id, HdLightTokens->radius);
        if (!radiusValue.IsEmpty()) {
            _radius = radiusValue.Get<float>();
        }
    }
    
    // Allocate and populate light buffer
    if (bits & (DirtyParams | DirtyTransform)) {
        auto* render_param = static_cast<Hd_USTC_CG_RenderParam*>(renderParam);
        if (!this->light_buffer) {
            this->light_buffer = render_param->InstanceCollection->light_pool.allocate(1);
        }
        
        LightData lightData;
        lightData.type = (uint32_t)LightType::Disc;
        
        // Get transform
        auto transform = this->Get(HdTokens->transform).GetWithDefault<GfMatrix4d>();
        GfVec3d pos = transform.ExtractTranslation();
        lightData.posW = float3(pos[0], pos[1], pos[2]);
        
        // Get direction (disk emits along -Z in local space)
        GfVec4d zDir = transform.GetRow(2);
        float3 dir = float3(-zDir[0], -zDir[1], -zDir[2]);
        float dirLen = sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
        lightData.dirW = float3(dir.x / dirLen, dir.y / dirLen, dir.z / dirLen); // Normalized
        
        // Get tangent and bitangent for disk orientation
        // Extract normalized directions and scale by radius
        GfVec4d xDir = transform.GetRow(0);
        GfVec4d yDir = transform.GetRow(1);
        
        float3 xVec = float3(xDir[0], xDir[1], xDir[2]);
        float3 yVec = float3(yDir[0], yDir[1], yDir[2]);
        float xLen = sqrt(xVec.x * xVec.x + xVec.y * xVec.y + xVec.z * xVec.z);
        float yLen = sqrt(yVec.x * yVec.x + yVec.y * yVec.y + yVec.z * yVec.z);
        
        lightData.tangent = float3(xVec.x / xLen, xVec.y / xLen, xVec.z / xLen) * _radius;
        lightData.bitangent = float3(yVec.x / yLen, yVec.y / yLen, yVec.z / yLen) * _radius;
        
        // Get color and intensity with exposure
        auto diffuse = sceneDelegate->GetLightParamValue(id, HdLightTokens->diffuse).GetWithDefault<float>(1.0f);
        auto color = sceneDelegate->GetLightParamValue(id, HdLightTokens->color).GetWithDefault<GfVec3f>(GfVec3f(1,1,1));
        auto intensity = sceneDelegate->GetLightParamValue(id, HdLightTokens->intensity).GetWithDefault<float>(1.0f);
        auto exposure = sceneDelegate->GetLightParamValue(id, HdLightTokens->exposure).GetWithDefault<float>(0.0f);
        
        float finalIntensity = intensity * pow(2.0f, exposure);
        lightData.intensity = float3(color[0], color[1], color[2]) * diffuse * finalIntensity;
        
        // Debug output
        spdlog::info("DiskLight {}: radius={}, color=({},{},{}), intensity={}, exposure={}", 
                     id.GetText(), _radius, color[0], color[1], color[2], intensity, exposure);
        
        // Disk area
        lightData.surfaceArea = 3.14159265359f * _radius * _radius;
        
        this->light_buffer->write_data(&lightData);
    }
    
    // Clear dirty bits
    *dirtyBits = Clean;
}

void Hd_USTC_CG_Cylinder_Light::Sync(
    HdSceneDelegate* sceneDelegate,
    HdRenderParam* renderParam,
    HdDirtyBits* dirtyBits)
{
    Hd_USTC_CG_Light::Sync(sceneDelegate, renderParam, dirtyBits);
    
    // Get cylinder light specific parameters
    const SdfPath& id = this->GetId();
    HdDirtyBits bits = *dirtyBits;
    
    if (bits & DirtyParams) {
        VtValue radiusValue = sceneDelegate->GetLightParamValue(id, HdLightTokens->radius);
        if (!radiusValue.IsEmpty()) {
            _radius = radiusValue.Get<float>();
        }
        
        VtValue lengthValue = sceneDelegate->GetLightParamValue(id, HdLightTokens->length);
        if (!lengthValue.IsEmpty()) {
            _length = lengthValue.Get<float>();
        }
    }
    
    // Allocate and populate light buffer  
    if (bits & (DirtyParams | DirtyTransform)) {
        auto* render_param = static_cast<Hd_USTC_CG_RenderParam*>(renderParam);
        if (!this->light_buffer) {
            this->light_buffer = render_param->InstanceCollection->light_pool.allocate(1);
        }
        
        LightData lightData;
        // Note: Cylinder light is not in the standard LightType enum
        // For now, treat it as a special case or extend the enum
        // Using Point as placeholder - you may want to add LightType::Cylinder
        lightData.type = (uint32_t)LightType::Point; // TODO: Add Cylinder type
        
        // Get transform
        auto transform = this->Get(HdTokens->transform).GetWithDefault<GfMatrix4d>();
        GfVec3d pos = transform.ExtractTranslation();
        lightData.posW = float3(pos[0], pos[1], pos[2]);
        
        // Cylinder axis direction (along local Z)
        GfVec4d zDir = transform.GetRow(2);
        lightData.dirW = float3(zDir[0], zDir[1], zDir[2]);
        
        // Get color and intensity
        auto diffuse = sceneDelegate->GetLightParamValue(id, HdLightTokens->diffuse).GetWithDefault<float>(1.0f);
        auto color = sceneDelegate->GetLightParamValue(id, HdLightTokens->color).GetWithDefault<GfVec3f>(GfVec3f(1,1,1));
        auto intensity = sceneDelegate->GetLightParamValue(id, HdLightTokens->intensity).GetWithDefault<float>(1.0f);
        lightData.intensity = float3(color[0], color[1], color[2]) * diffuse * intensity;
        
        // Cylinder surface area (without caps)
        lightData.surfaceArea = 2.0f * 3.14159265359f * _radius * _length;
        
        this->light_buffer->write_data(&lightData);
    }
    
    // Clear dirty bits
    *dirtyBits = Clean;
}

void Hd_USTC_CG_Dome_Light::_PrepareDomeLight(
    SdfPath const& id,
    HdSceneDelegate* sceneDelegate)
{
    const VtValue v =
        sceneDelegate->GetLightParamValue(id, HdLightTokens->textureFile);
    textureFileName = v.Get<pxr::SdfAssetPath>();

    env_texture.image =
        HioImage::OpenForReading(textureFileName.GetAssetPath(), 0, 0);

    if (env_texture.glTexture) {
        glDeleteTextures(1, &env_texture.glTexture);
        env_texture.glTexture = 0;
    }

    auto diffuse = sceneDelegate->GetLightParamValue(id, HdLightTokens->diffuse)
                       .Get<float>();
    radiance = sceneDelegate->GetLightParamValue(id, HdLightTokens->color)
                   .Get<GfVec3f>() *
               diffuse;
}

void Hd_USTC_CG_Dome_Light::Sync(
    HdSceneDelegate* sceneDelegate,
    HdRenderParam* renderParam,
    HdDirtyBits* dirtyBits)
{
    Hd_USTC_CG_Light::Sync(sceneDelegate, renderParam, dirtyBits);

    auto id = GetId();
    _PrepareDomeLight(id, sceneDelegate);
    
    // Clear dirty bits
    *dirtyBits = Clean;
}

void Hd_USTC_CG_Dome_Light::Finalize(HdRenderParam* renderParam)
{
    if (env_texture.glTexture) {
        glDeleteTextures(1, &env_texture.glTexture);
        env_texture.glTexture = 0;
    }

    Hd_USTC_CG_Light::Finalize(renderParam);
}

USTC_CG_NAMESPACE_CLOSE_SCOPE
