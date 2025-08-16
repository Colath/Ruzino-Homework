#include "GCore/GOP.h"

#include <pxr/base/tf/stringUtils.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usdGeom/basisCurves.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/pointInstancer.h>
#include <pxr/usd/usdGeom/points.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>
#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>
#include <pxr/usd/usdVol/openVDBAsset.h>
#include <pxr/usd/usdVol/volume.h>
#include <spdlog/spdlog.h>

#include <string>

#include "GCore/Components.h"
#include "GCore/Components/CurveComponent.h"
#include "GCore/Components/InstancerComponent.h"
#include "GCore/Components/MaterialComponent.h"
#include "GCore/Components/MeshComponent.h"
#include "GCore/Components/PointsComponent.h"
#include "GCore/Components/VolumeComponent.h"
#include "GCore/Components/XformComponent.h"
#include "GCore/geom_payload.hpp"
#include "global_stage.hpp"
#include "pxr/base/gf/rotation.h"
#include "pxr/usd/usd/payloads.h"
#include "pxr/usd/usdGeom/xform.h"

USTC_CG_NAMESPACE_OPEN_SCOPE
Geometry::Geometry()
{
}

Geometry::~Geometry()
{
}

void Geometry::apply_transform()
{
    auto xform_component = get_component<XformComponent>();
    if (!xform_component) {
        return;
    }

    auto transform = xform_component->get_transform();

    for (auto&& component : components_) {
        if (component) {
            component->apply_transform(transform);
        }
    }
}

Geometry::Geometry(const Geometry& operand)
{
    *(this) = operand;
}

Geometry::Geometry(Geometry&& operand) noexcept
{
    *(this) = std::move(operand);
}

Geometry& Geometry::operator=(const Geometry& operand)
{
    for (auto&& operand_component : operand.components_) {
        this->components_.push_back(operand_component->copy(this));
    }

    return *this;
}

Geometry& Geometry::operator=(Geometry&& operand) noexcept
{
    this->components_ = std::move(operand.components_);
    return *this;
}

Geometry Geometry::CreateMesh()
{
    Geometry geometry;
    std::shared_ptr<MeshComponent> mesh =
        std::make_shared<MeshComponent>(&geometry);
    geometry.attach_component(mesh);
    return std::move(geometry);
}

Geometry Geometry::CreatePoints()
{
    Geometry geometry;
    std::shared_ptr<PointsComponent> points =
        std::make_shared<PointsComponent>(&geometry);
    geometry.attach_component(points);
    return std::move(geometry);
}

Geometry Geometry::CreateVolume()
{
    Geometry geometry;
    std::shared_ptr<VolumeComponent> volume =
        std::make_shared<VolumeComponent>(&geometry);
    geometry.attach_component(volume);
    return std::move(geometry);
}

std::string Geometry::to_string() const
{
    std::ostringstream out;
    out << "Contains components:\n";
    for (auto&& component : components_) {
        if (component) {
            out << "    " << component->to_string() << "\n";
        }
    }
    return out.str();
}

void Geometry::attach_component(const GeometryComponentHandle& component)
{
    if (component->get_attached_operand() != this) {
        spdlog::warn(
            "A component should never be attached to two operands, unless you "
            "know what you are doing");
    }
    components_.push_back(component);
}

void Geometry::detach_component(const GeometryComponentHandle& component)
{
    auto iter = std::find(components_.begin(), components_.end(), component);
    components_.erase(iter);
}

Stage* g_stage = nullptr;
void init(Stage* stage)
{
    g_stage = stage;
}

bool legal(const std::string& string)
{
    if (string.empty()) {
        return false;
    }
    if (std::find_if(string.begin(), string.end(), [](char val) {
            return val == '(' || val == ')' || val == ',';
        }) == string.end()) {
        return true;
    }
    return false;
}

bool write_geometry_to_usd(
    const Geometry& geometry,
    pxr::UsdStageRefPtr stage,
    const pxr::SdfPath& sdf_path,
    pxr::UsdTimeCode time)
{
    auto mesh = geometry.get_component<MeshComponent>();
    auto points = geometry.get_component<PointsComponent>();
    auto curve = geometry.get_component<CurveComponent>();
    auto volume = geometry.get_component<VolumeComponent>();
    auto instancer = geometry.get_component<InstancerComponent>();

    assert(!(points && mesh));

    pxr::SdfPath actual_path = sdf_path;
    if (instancer) {
        actual_path = sdf_path.AppendPath(pxr::SdfPath("Prototype"));
    }

    if (mesh) {
        pxr::UsdGeomMesh usdgeom = pxr::UsdGeomMesh::Define(stage, actual_path);

        if (usdgeom) {
#if USE_USD_SCRATCH_BUFFER
            copy_prim(mesh->get_usd_mesh().GetPrim(), usdgeom.GetPrim());
#else
            usdgeom.CreatePointsAttr().Set(mesh->get_vertices(), time);
            usdgeom.CreateFaceVertexCountsAttr().Set(
                mesh->get_face_vertex_counts(), time);
            usdgeom.CreateFaceVertexIndicesAttr().Set(
                mesh->get_face_vertex_indices(), time);

            auto primVarAPI = pxr::UsdGeomPrimvarsAPI(usdgeom);

            if (!mesh->get_normals().empty()) {
                usdgeom.CreateNormalsAttr().Set(mesh->get_normals(), time);
                if (mesh->get_normals().size() == mesh->get_vertices().size()) {
                    usdgeom.SetNormalsInterpolation(pxr::UsdGeomTokens->vertex);
                }
                else {
                    usdgeom.SetNormalsInterpolation(
                        pxr::UsdGeomTokens->faceVarying);
                }
                usdgeom.CreateSubdivisionSchemeAttr().Set(
                    pxr::UsdGeomTokens->none);
            }
            else {
                usdgeom.CreateNormalsAttr().Block();
            }
            if (!mesh->get_display_color().empty()) {
                auto colorPrimvar = primVarAPI.CreatePrimvar(
                    pxr::TfToken("displayColor"),
                    pxr::SdfValueTypeNames->Color3fArray);
                colorPrimvar.SetInterpolation(pxr::UsdGeomTokens->vertex);
                colorPrimvar.Set(mesh->get_display_color(), time);
            }
            if (!mesh->get_texcoords_array().empty()) {
                auto primvar = primVarAPI.CreatePrimvar(
                    pxr::TfToken("UVMap"),
                    pxr::SdfValueTypeNames->TexCoord2fArray);
                primvar.Set(mesh->get_texcoords_array(), time);
                if (mesh->get_texcoords_array().size() ==
                    mesh->get_vertices().size()) {
                    primvar.SetInterpolation(pxr::UsdGeomTokens->vertex);
                }
                else {
                    primvar.SetInterpolation(pxr::UsdGeomTokens->faceVarying);
                }
            }

#endif
            usdgeom.CreateDoubleSidedAttr().Set(true);

            // Write all mesh attributes
            for (const std::string& name :
                 mesh->get_vertex_scalar_quantity_names()) {
                auto values = mesh->get_vertex_scalar_quantity(name);
                const std::string primvar_name =
                    "polyscope:vertex:scalar:" + name;
                auto primvar = primVarAPI.CreatePrimvar(
                    pxr::TfToken(primvar_name.c_str()),
                    pxr::SdfValueTypeNames->FloatArray);
                primvar.SetInterpolation(pxr::UsdGeomTokens->vertex);
                primvar.Set(values, time);
            }

            for (const std::string& name :
                 mesh->get_face_scalar_quantity_names()) {
                auto values = mesh->get_face_scalar_quantity(name);
                const std::string primvar_name =
                    "polyscope:face:scalar:" + name;
                auto primvar = primVarAPI.CreatePrimvar(
                    pxr::TfToken(primvar_name.c_str()),
                    pxr::SdfValueTypeNames->FloatArray);
                primvar.SetInterpolation(pxr::UsdGeomTokens->uniform);
                primvar.Set(values, time);
            }

            for (std::string& name : mesh->get_vertex_color_quantity_names()) {
                auto values = mesh->get_vertex_color_quantity(name);
                const std::string primvar_name =
                    "polyscope:vertex:color:" + name;
                auto primvar = primVarAPI.CreatePrimvar(
                    pxr::TfToken(primvar_name.c_str()),
                    pxr::SdfValueTypeNames->Color3fArray);
                primvar.SetInterpolation(pxr::UsdGeomTokens->vertex);
                primvar.Set(values, time);
            }

            for (const std::string& name :
                 mesh->get_face_color_quantity_names()) {
                auto values = mesh->get_face_color_quantity(name);
                const std::string primvar_name = "polyscope:face:color:" + name;
                auto primvar = primVarAPI.CreatePrimvar(
                    pxr::TfToken(primvar_name.c_str()),
                    pxr::SdfValueTypeNames->Color3fArray);
                primvar.SetInterpolation(pxr::UsdGeomTokens->uniform);
                primvar.Set(values, time);
            }

            for (const std::string& name :
                 mesh->get_vertex_vector_quantity_names()) {
                auto values = mesh->get_vertex_vector_quantity(name);
                const std::string primvar_name =
                    "polyscope:vertex:vector:" + name;
                auto primvar = primVarAPI.CreatePrimvar(
                    pxr::TfToken(primvar_name.c_str()),
                    pxr::SdfValueTypeNames->Vector3fArray);
                primvar.SetInterpolation(pxr::UsdGeomTokens->vertex);
                primvar.Set(values, time);
            }

            for (const std::string& name :
                 mesh->get_face_vector_quantity_names()) {
                auto values = mesh->get_face_vector_quantity(name);
                const std::string primvar_name =
                    "polyscope:face:vector:" + name;
                auto primvar = primVarAPI.CreatePrimvar(
                    pxr::TfToken(primvar_name.c_str()),
                    pxr::SdfValueTypeNames->Vector3fArray);
                primvar.SetInterpolation(pxr::UsdGeomTokens->uniform);
                primvar.Set(values, time);
            }

            for (const std::string& name :
                 mesh->get_face_corner_parameterization_quantity_names()) {
                auto values =
                    mesh->get_face_corner_parameterization_quantity(name);
                const std::string primvar_name =
                    "polyscope:face_corner:parameterization:" + name;
                auto primvar = primVarAPI.CreatePrimvar(
                    pxr::TfToken(primvar_name.c_str()),
                    pxr::SdfValueTypeNames->TexCoord2fArray);
                primvar.SetInterpolation(pxr::UsdGeomTokens->faceVarying);
                primvar.Set(values, time);
            }

            for (const std::string& name :
                 mesh->get_vertex_parameterization_quantity_names()) {
                auto values = mesh->get_vertex_parameterization_quantity(name);
                const std::string primvar_name =
                    "polyscope:vertex:parameterization:" + name;
                auto primvar = primVarAPI.CreatePrimvar(
                    pxr::TfToken(primvar_name.c_str()),
                    pxr::SdfValueTypeNames->TexCoord2fArray);
                primvar.SetInterpolation(pxr::UsdGeomTokens->vertex);
                primvar.Set(values, time);
            }
        }
    }
    else if (points) {
        pxr::UsdGeomPoints usdpoints =
            pxr::UsdGeomPoints::Define(stage, actual_path);

        usdpoints.CreatePointsAttr().Set(points->get_vertices(), time);

        if (points->get_width().size() > 0) {
            usdpoints.CreateWidthsAttr().Set(points->get_width(), time);
        }

        auto PrimVarAPI = pxr::UsdGeomPrimvarsAPI(usdpoints);
        if (points->get_display_color().size() > 0) {
            pxr::UsdGeomPrimvar colorPrimvar = PrimVarAPI.CreatePrimvar(
                pxr::TfToken("displayColor"),
                pxr::SdfValueTypeNames->Color3fArray);
            colorPrimvar.SetInterpolation(pxr::UsdGeomTokens->vertex);
            colorPrimvar.Set(points->get_display_color(), time);
        }
    }
    else if (curve) {
        pxr::UsdGeomBasisCurves usd_curve =
            pxr::UsdGeomBasisCurves::Define(stage, actual_path);
        if (usd_curve) {
#if USE_USD_SCRATCH_BUFFER
            copy_prim(curve->get_usd_curve().GetPrim(), usd_curve.GetPrim());
#else
            usd_curve.CreatePointsAttr().Set(curve->get_vertices(), time);
            usd_curve.CreateWidthsAttr().Set(curve->get_width(), time);
            usd_curve.CreateCurveVertexCountsAttr().Set(
                curve->get_vert_count(), time);
            usd_curve.CreateNormalsAttr().Set(curve->get_curve_normals(), time);
            usd_curve.CreateDisplayColorAttr().Set(
                curve->get_display_color(), time);
            usd_curve.CreateWrapAttr().Set(
                curve->get_periodic() ? pxr::UsdGeomTokens->periodic
                                      : pxr::UsdGeomTokens->nonperiodic,
                time);
            usd_curve.CreateTypeAttr().Set(
                curve->get_type() == CurveComponent::CurveType::Linear
                    ? pxr::UsdGeomTokens->linear
                    : pxr::UsdGeomTokens->cubic,
                time);
#endif
        }
    }
    else if (volume) {
        pxr::UsdVolVolume usd_volume =
            pxr::UsdVolVolume::Define(stage, actual_path);

        if (!usd_volume) {
            return false;
        }

        auto vdb_asset_path = actual_path.AppendChild(pxr::TfToken("field"));
        auto openvdb_asset =
            pxr::UsdVolOpenVDBAsset::Define(stage, vdb_asset_path);

        if (!openvdb_asset) {
            return false;
        }

        auto file_name = "volume" + actual_path.GetName() +
                         std::to_string(time.GetValue()) + ".vdb";
        volume->write_disk(file_name);

        openvdb_asset.CreateFilePathAttr().Set(
            pxr::SdfAssetPath(file_name), time);

        if (!usd_volume.CreateFieldRelationship(
                pxr::TfToken("field"), vdb_asset_path)) {
            return false;
        }

        usd_volume.MakeVisible(time);
    }
    else {
        return false;
    }

    // Handle instancer
    if (instancer) {
        auto instancer_component =
            pxr::UsdGeomPointInstancer::Define(stage, sdf_path);
        instancer_component.CreatePrototypesRel().SetTargets({ actual_path });

        auto transforms = instancer->get_instances();

        pxr::VtVec3fArray positions = pxr::VtVec3fArray(transforms.size());
        pxr::VtQuathArray orientations = pxr::VtQuathArray(transforms.size());
        pxr::VtVec3fArray scales = pxr::VtVec3fArray(transforms.size());

        for (size_t i = 0; i < transforms.size(); ++i) {
            pxr::GfVec3f translation;
            pxr::GfQuath rotation;
            translation = pxr::GfVec3f(transforms[i].ExtractTranslation());
            rotation = pxr::GfQuath(transforms[i].ExtractRotationQuat());
            positions[i] = translation;
            orientations[i] = rotation;
        }
        instancer_component.CreateProtoIndicesAttr().Set(
            pxr::VtIntArray(instancer->get_proto_indices()));
        instancer_component.CreatePositionsAttr().Set(positions);
        instancer_component.CreateOrientationsAttr().Set(orientations);
    }

    // Handle materials
    auto material_component = geometry.get_component<MaterialComponent>();
    if (material_component) {
        auto usdgeom = pxr::UsdGeomXformable::Get(stage, actual_path);
        if (legal(std::string(material_component->textures[0].c_str()))) {
            auto material_path = material_component->get_material_path();
            auto material =
                material_component->define_material(stage, material_path);
            usdgeom.GetPrim().ApplyAPI(pxr::UsdShadeTokens->MaterialBindingAPI);
            pxr::UsdShadeMaterialBindingAPI(usdgeom).Bind(material);
        }
    }

    // Handle transforms
    auto xform_component = geometry.get_component<XformComponent>();
    auto usdgeom = pxr::UsdGeomXformable::Get(stage, actual_path);
    auto xform_op = usdgeom.GetTransformOp();
    if (!xform_op) {
        xform_op = usdgeom.AddTransformOp();
    }

    if (xform_component) {
        assert(
            xform_component->translation.size() ==
            xform_component->rotation.size());
        pxr::GfMatrix4d final_transform = xform_component->get_transform();
        xform_op.Set(final_transform, time);
    }
    else {
        xform_op.Set(pxr::GfMatrix4d(1), time);
    }

    pxr::UsdGeomImageable(stage->GetPrimAtPath(actual_path)).MakeVisible();
    return true;
}

USTC_CG_NAMESPACE_CLOSE_SCOPE
