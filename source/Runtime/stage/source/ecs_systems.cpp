#include "stage/ecs_systems.hpp"

#include <pxr/usd/usdGeom/xformable.h>

#include <glm/gtc/type_ptr.hpp>

#include "../../../Editor/geometry/include/GCore/geom_payload.hpp"
#include "stage/animation.h"
#include "stage/stage.hpp"

#ifdef GEOM_USD_EXTENSION
#include "GCore/usd_extension.h"
#endif

RUZINO_NAMESPACE_OPEN_SCOPE
namespace ecs {

// ============================================================================
// Animation System Implementation
// ============================================================================

AnimationSystem::AnimationSystem(Stage* stage) : stage_(stage)
{
}

void AnimationSystem::update(entt::registry& registry, float delta_time)
{
    // Iterate all entities with AnimationComponent and UsdPrimComponent
    auto view = registry.view<AnimationComponent, UsdPrimComponent>();

    for (auto entity : view) {
        auto& anim = view.get<AnimationComponent>(entity);
        auto& usd_prim = view.get<UsdPrimComponent>(entity);

        update_single_entity(registry, entity, anim, usd_prim, delta_time);
    }
}

void AnimationSystem::update_single_entity(
    entt::registry& registry,
    entt::entity entity,
    AnimationComponent& anim,
    UsdPrimComponent& usd_prim,
    float delta_time)
{
    if (!anim.node_tree || !anim.node_tree_executor) {
        return;
    }

    // Check if node tree has changed
    auto json_attr = usd_prim.prim.GetAttribute(pxr::TfToken("node_json"));
    if (json_attr) {
        pxr::VtValue json;
        json_attr.Get(&json);
        auto new_tree_desc = json.Get<std::string>();

        if (anim.tree_desc_cache != new_tree_desc) {
            anim.tree_desc_cache = new_tree_desc;
            anim.node_tree->deserialize(anim.tree_desc_cache);
            anim.node_tree_executor->mark_tree_structure_changed();

            // Clear old time sample data
            clear_time_samples(usd_prim.prim);

            // Reset time state
            usd_prim.current_time = pxr::UsdTimeCode(0.0f);
            usd_prim.render_time = pxr::UsdTimeCode(0.0f);
            anim.simulation_begun = false;
        }
    }

    // Update render time
    usd_prim.render_time = stage_->get_render_time();

    // Check if simulation should proceed
    if (usd_prim.render_time < usd_prim.current_time) {
        return;
    }

    // Execute node tree
    auto& payload = anim.node_tree_executor->get_global_payload<GeomPayload&>();
    payload.delta_time = delta_time;

#ifdef GEOM_USD_EXTENSION
    payload.stage = usd_prim.prim.GetStage();
    payload.prim_path = usd_prim.prim.GetPath();
    payload.current_time = usd_prim.current_time;
#endif

    payload.has_simulation = false;
    payload.is_simulating = anim.simulation_begun;

    if (!anim.simulation_begun) {
        anim.simulation_begun = true;
    }

    anim.node_tree_executor->execute(anim.node_tree.get());

    // Update simulation time
    auto current = usd_prim.current_time.GetValue();
    current += delta_time;
    usd_prim.current_time = pxr::UsdTimeCode(current);

    // Mark as dirty, needs to sync to USD
    if (!registry.all_of<DirtyComponent>(entity)) {
        registry.emplace<DirtyComponent>(entity);
    }
    auto& dirty = registry.get<DirtyComponent>(entity);
    dirty.needs_geometry_update = true;
}

void AnimationSystem::clear_time_samples(const pxr::UsdPrim& prim) const
{
    if (!prim.IsValid()) {
        return;
    }

    // Recursively clear time samples for all child prims
    for (const auto& child : prim.GetChildren()) {
        clear_time_samples(child);
    }

    // Clear all attribute time samples for current prim
    for (const auto& attr : prim.GetAttributes()) {
        const auto& attr_name = attr.GetName();
        if (attr_name == pxr::TfToken("node_json") ||
            attr_name == pxr::TfToken("Animatable")) {
            continue;
        }

        if (attr.GetNumTimeSamples() > 0) {
            attr.Clear();
        }
    }
}

// ============================================================================
// USD Sync System Implementation
// ============================================================================

UsdSyncSystem::UsdSyncSystem(Stage* stage) : stage_(stage)
{
}

void UsdSyncSystem::sync(entt::registry& registry, pxr::UsdTimeCode time)
{
    // Sync all dirty entities
    auto view = registry.view<DirtyComponent, UsdPrimComponent>();

    int synced_count = 0;
    for (auto entity : view) {
        auto& dirty = view.get<DirtyComponent>(entity);
        auto& usd_prim = view.get<UsdPrimComponent>(entity);

        // If geometry has changed, sync to USD
        if (dirty.needs_geometry_update) {
            if (registry.all_of<GeometryComponent>(entity)) {
                auto& geom_comp = registry.get<GeometryComponent>(entity);
                if (geom_comp.geometry) {
                    usd_prim.sync_from_geometry(*geom_comp.geometry, time);
                }
            }
        }

        // Clear dirty flags
        dirty.needs_geometry_update = false;
        dirty.needs_usd_sync = false;
        synced_count++;
    }
}

entt::entity UsdSyncSystem::create_entity_from_prim(
    entt::registry& registry,
    const pxr::UsdPrim& prim)
{
    auto entity = registry.create();

    // Add USD prim component
    registry.emplace<UsdPrimComponent>(entity, prim);

    // Check if it has animation
    auto animatable_attr = prim.GetAttribute(pxr::TfToken("Animatable"));
    if (animatable_attr) {
        bool is_animatable = false;
        animatable_attr.Get(&is_animatable);

        if (is_animatable) {
            registry.emplace<AnimatableTag>(entity);
            // Can initialize AnimationComponent here
        }
    }

    // Can add other components...

    return entity;
}

void UsdSyncSystem::update_prim_from_entity(
    entt::registry& registry,
    entt::entity entity,
    pxr::UsdTimeCode time)
{
    auto& usd_prim = registry.get<UsdPrimComponent>(entity);

    // Sync Geometry (including Transform)
    if (registry.all_of<GeometryComponent>(entity)) {
        auto& geometry = registry.get<GeometryComponent>(entity);
        if (geometry.geometry) {
            usd_prim.sync_from_geometry(*geometry.geometry, time);
        }
    }
}

void UsdSyncSystem::sync_geometry(
    const GeometryComponent& geometry,
    pxr::UsdPrim& prim,
    pxr::UsdTimeCode time)
{
#ifdef GEOM_USD_EXTENSION
    if (geometry.geometry) {
        write_geometry_to_usd(
            *geometry.geometry, prim.GetStage(), prim.GetPath(), time);
    }
#endif
}

// ============================================================================
// Physics System Implementation (reserved)
// ============================================================================

PhysicsSystem::PhysicsSystem()
{
}

PhysicsSystem::~PhysicsSystem()
{
    shutdown();
}

bool PhysicsSystem::initialize()
{
    if (initialized_) {
        return true;
    }

    // TODO: Initialize PhysX SDK
    // physics_sdk_ = PxCreatePhysics(...);
    // physics_scene_ = physics_sdk_->createScene(...);

    initialized_ = true;
    return true;
}

void PhysicsSystem::shutdown()
{
    if (!initialized_) {
        return;
    }

    // TODO: Cleanup PhysX resources

    initialized_ = false;
}

void PhysicsSystem::update(entt::registry& registry, float delta_time)
{
    if (!initialized_) {
        return;
    }

    // TODO: Update physics simulation
    // physics_scene_->simulate(delta_time);
    // physics_scene_->fetchResults(true);

    // Update transform for all physics entities
    auto view =
        registry.view<PhysicsComponent, GeometryComponent, UsdPrimComponent>();
    for (auto entity : view) {
        auto& physics = view.get<PhysicsComponent>(entity);
        auto& geom_comp = view.get<GeometryComponent>(entity);
        auto& usd_prim = view.get<UsdPrimComponent>(entity);

        // TODO: Get transform from PhysX actor and update Geometry's
        // XformComponent if (physics.physics_actor && geom_comp.geometry) {
        //     PxTransform px_transform =
        //     static_cast<PxRigidActor*>(physics.physics_actor)->getGlobalPose();
        //     auto xform_comp =
        //     geom_comp.geometry->get_component<XformComponent>(); if
        //     (xform_comp) {
        //         xform_comp->translation[0] = glm::vec3(px_transform.p.x,
        //         px_transform.p.y, px_transform.p.z); xform_comp->rotation[0]
        //         = glm::quat(px_transform.q.w, px_transform.q.x,
        //         px_transform.q.y, px_transform.q.z);
        //     }
        // }
    }
}

void PhysicsSystem::add_physics_actor(
    entt::registry& registry,
    entt::entity entity)
{
    // TODO: Create PhysX actor
}

void PhysicsSystem::remove_physics_actor(
    entt::registry& registry,
    entt::entity entity)
{
    // TODO: Destroy PhysX actor
}

// ============================================================================
// Scene Query System Implementation (reserved)
// ============================================================================

SceneQuerySystem::SceneQuerySystem(PhysicsSystem* physics_system)
    : physics_system_(physics_system)
{
}

bool SceneQuerySystem::raycast(
    const glm::vec3& origin,
    const glm::vec3& direction,
    float max_distance,
    entt::entity& hit_entity,
    glm::vec3& hit_position)
{
    // TODO: Implement raycast
    return false;
}

bool SceneQuerySystem::overlap_sphere(
    const glm::vec3& center,
    float radius,
    std::vector<entt::entity>& overlapping_entities)
{
    // TODO: Implement overlap test
    return false;
}

}  // namespace ecs
RUZINO_NAMESPACE_CLOSE_SCOPE
