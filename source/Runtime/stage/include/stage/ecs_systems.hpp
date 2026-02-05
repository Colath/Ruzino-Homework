#pragma once

#include <pxr/usd/usd/stage.h>

#include <entt/entt.hpp>

#include "api.h"
#include "stage/ecs_components.hpp"

RUZINO_NAMESPACE_OPEN_SCOPE

// Forward declaration
class Stage;

namespace ecs {

// ============================================================================
// Animation System - Handle animation logic updates
// ============================================================================
class STAGE_API AnimationSystem {
   public:
    AnimationSystem(Stage* stage);

    // Update all animation entities
    void update(entt::registry& registry, float delta_time);

   private:
    void update_single_entity(
        entt::registry& registry,
        entt::entity entity,
        AnimationComponent& anim,
        UsdPrimComponent& usd_prim,
        float delta_time);

    void clear_time_samples(const pxr::UsdPrim& prim) const;

    Stage* stage_;
};

// ============================================================================
// USD Sync System - Synchronize ECS data to USD
// ============================================================================
class STAGE_API UsdSyncSystem {
   public:
    UsdSyncSystem(Stage* stage);

    // Sync all dirty entities to USD
    void sync(entt::registry& registry, pxr::UsdTimeCode time);

    // Create entity from USD
    entt::entity create_entity_from_prim(
        entt::registry& registry,
        const pxr::UsdPrim& prim);

    // Update USD prim from entity
    void update_prim_from_entity(
        entt::registry& registry,
        entt::entity entity,
        pxr::UsdTimeCode time);

   private:
    void sync_geometry(
        const GeometryComponent& geometry,
        pxr::UsdPrim& prim,
        pxr::UsdTimeCode time);

    Stage* stage_;
};

// ============================================================================
// Physics System - Physics simulation system (reserved for PhysX)
// ============================================================================
class STAGE_API PhysicsSystem {
   public:
    PhysicsSystem();
    ~PhysicsSystem();

    // Initialize physics engine
    bool initialize();

    // Shutdown physics engine
    void shutdown();

    // Update physics simulation
    void update(entt::registry& registry, float delta_time);

    // Add physics actor
    void add_physics_actor(entt::registry& registry, entt::entity entity);

    // Remove physics actor
    void remove_physics_actor(entt::registry& registry, entt::entity entity);

   private:
    // PhysX related members (using void* temporarily to avoid dependencies)
    void* physics_sdk_ = nullptr;
    void* physics_scene_ = nullptr;
    bool initialized_ = false;
};

// ============================================================================
// Scene Query System - For collision detection and other queries
// ============================================================================
class STAGE_API SceneQuerySystem {
   public:
    SceneQuerySystem(PhysicsSystem* physics_system);

    // Raycast
    bool raycast(
        const glm::vec3& origin,
        const glm::vec3& direction,
        float max_distance,
        entt::entity& hit_entity,
        glm::vec3& hit_position);

    // Overlap test
    bool overlap_sphere(
        const glm::vec3& center,
        float radius,
        std::vector<entt::entity>& overlapping_entities);

   private:
    PhysicsSystem* physics_system_;
};

}  // namespace ecs

RUZINO_NAMESPACE_CLOSE_SCOPE
