#pragma once

#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/timeCode.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <string>

#include "api.h"

RUZINO_NAMESPACE_OPEN_SCOPE

// Forward declarations
class NodeTree;
class NodeTreeExecutor;
class Geometry;

namespace ecs {

// ============================================================================
// USD Component - Store USD prim reference and USD↔Geometry sync functionality
// ============================================================================
struct STAGE_API UsdPrimComponent {
    pxr::UsdPrim prim;

    // Prim's own time state (for independent animation timeline)
    pxr::UsdTimeCode current_time = pxr::UsdTimeCode(0.0);
    pxr::UsdTimeCode render_time = pxr::UsdTimeCode(0.0);

    UsdPrimComponent() = default;
    explicit UsdPrimComponent(const pxr::UsdPrim& p) : prim(p)
    {
    }

    // Sync geometry data from USD prim to Geometry object
    // Parameters: geometry - target Geometry object
    //             time - USD time code (default: use current_time)
    // Returns: whether sync was successful
    bool sync_to_geometry(
        class Geometry& geometry,
        const pxr::UsdTimeCode& time) const;
    bool sync_to_geometry(class Geometry& geometry) const;

    // Sync data from Geometry object to USD prim
    // Parameters: geometry - source Geometry object
    //             time - USD time code (default: use current_time)
    // Returns: whether sync was successful
    bool sync_from_geometry(
        const class Geometry& geometry,
        const pxr::UsdTimeCode& time);
    bool sync_from_geometry(const class Geometry& geometry);
};

// ============================================================================
// Animation Component - Store animation/node logic
// ============================================================================
struct STAGE_API AnimationComponent {
    std::shared_ptr<NodeTree> node_tree;
    std::shared_ptr<NodeTreeExecutor> node_tree_executor;

    // Cached tree description for detecting changes
    mutable std::string tree_desc_cache;

    // Simulation state
    mutable bool simulation_begun = false;

    AnimationComponent() = default;
};

// ============================================================================
// Geometry Component - Wrap Geometry system
// ============================================================================
struct STAGE_API GeometryComponent {
    std::shared_ptr<Geometry> geometry;

    GeometryComponent() = default;
    explicit GeometryComponent(std::shared_ptr<Geometry> geom)
        : geometry(std::move(geom))
    {
    }
};

// ============================================================================
// Material Component - Material reference
// ============================================================================
struct STAGE_API MaterialComponent {
    pxr::SdfPath material_path;
    std::string shader_path;  // Custom shader path

    MaterialComponent() = default;
};

// ============================================================================
// Physics Component - Reserved for PhysX
// ============================================================================
struct STAGE_API PhysicsComponent {
    enum class Type {
        Static,    // Static object
        Dynamic,   // Dynamic object
        Kinematic  // Kinematic object
    };

    Type type = Type::Static;
    float mass = 1.0f;
    bool is_trigger = false;

    // PhysX actor pointer (using void* temporarily to avoid dependency)
    void* physics_actor = nullptr;

    PhysicsComponent() = default;
};

// ============================================================================
// Dirty Flag Component - Mark entities that need to sync to USD
// ============================================================================
struct STAGE_API DirtyComponent {
    bool needs_usd_sync = false;
    bool needs_geometry_update =
        false;  // Geometry changed, needs to write to USD

    DirtyComponent() = default;
};

// ============================================================================
// Tag Components - Tags for filtering
// ============================================================================
struct STAGE_API AnimatableTag { };  // Mark animatable entity
struct STAGE_API RenderableTag { };  // Mark renderable entity
struct STAGE_API SimulationTag {
};  // Mark entity participating in physics simulation

}  // namespace ecs

RUZINO_NAMESPACE_CLOSE_SCOPE
