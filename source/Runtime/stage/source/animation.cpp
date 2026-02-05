#include "stage/animation.h"

#include <filesystem>

#include "../../../Editor/geometry/include/GCore/geom_payload.hpp"
#include "pxr/usd/usd/attribute.h"
#include "pxr/usd/usdGeom/xform.h"
#include "stage/stage.hpp"

RUZINO_NAMESPACE_OPEN_SCOPE
namespace animation {

std::once_flag WithDynamicLogicPrim::init_once;
std::shared_ptr<NodeTreeDescriptor> WithDynamicLogicPrim::node_tree_descriptor =
    nullptr;

WithDynamicLogic::WithDynamicLogic(Stage* stage) : stage_(stage)
{
}

WithDynamicLogicPrim::WithDynamicLogicPrim(
    const pxr::UsdPrim& prim,
    Stage* stage)
    : WithDynamicLogic(stage),
      prim(prim)
{
    std::call_once(init_once, [&] {
        // Only using this to initialize the node descriptor
        std::shared_ptr<NodeSystem> node_system =
            create_dynamic_loading_system();

        auto loaded = node_system->load_configuration("geometry_nodes.json");
        loaded = node_system->load_configuration("basic_nodes.json");

        // Load all plugin configurations
        auto plugin_path = std::filesystem::path("./Plugins");
        if (std::filesystem::exists(plugin_path)) {
            for (auto& p : std::filesystem::directory_iterator(plugin_path)) {
                if (p.path().extension() == ".json") {
                    node_system->load_configuration(p.path().string());
                }
            }
        }

        node_tree_descriptor = node_system->node_tree_descriptor();
    });

    node_tree = std::make_shared<NodeTree>(node_tree_descriptor);
    NodeTreeExecutorDesc executor_desc;
    executor_desc.policy = NodeTreeExecutorDesc::Policy::Eager;

    node_tree_executor = create_node_tree_executor(executor_desc);

    auto json_path = prim.GetAttribute(pxr::TfToken("node_json"));
    if (!json_path) {
        return;
    }

    auto json = pxr::VtValue();
    json_path.Get(&json);

    tree_desc_cache = json.Get<std::string>();
    node_tree->deserialize(tree_desc_cache);
}

WithDynamicLogicPrim::WithDynamicLogicPrim(const WithDynamicLogicPrim& prim)
    : WithDynamicLogic(prim.stage_)
{
    this->prim = prim.prim;
    this->node_tree = prim.node_tree;
    NodeTreeExecutorDesc executor_desc;

    executor_desc.policy = NodeTreeExecutorDesc::Policy::Eager;

    this->node_tree_executor = create_node_tree_executor(executor_desc);
}

WithDynamicLogicPrim& WithDynamicLogicPrim::operator=(
    const WithDynamicLogicPrim& prim)
{
    this->prim = prim.prim;
    this->node_tree = prim.node_tree;
    NodeTreeExecutorDesc executor_desc;

    executor_desc.policy = NodeTreeExecutorDesc::Policy::Eager;

    this->node_tree_executor = create_node_tree_executor(executor_desc);
    return *this;
}

void WithDynamicLogicPrim::update(float delta_time) const
{
    auto json_path = prim.GetAttribute(pxr::TfToken("node_json"));
    if (!json_path) {
        return;
    }

    auto json = pxr::VtValue();
    json_path.Get(&json);

    auto new_tree_desc = json.Get<std::string>();

    if (tree_desc_cache != new_tree_desc) {
        tree_desc_cache = new_tree_desc;
        node_tree->deserialize(tree_desc_cache);
        node_tree_executor->mark_tree_structure_changed();

        // Clear old time sample data
        clear_time_samples(prim);

        // Reset this prim's own time state
        prim_current_time = pxr::UsdTimeCode(0.0f);
        prim_render_time = pxr::UsdTimeCode(0.0f);

        stage_->set_render_time(0.0f);
        simulation_begun = false;
    }

    // Get current render time from Stage, update prim's render time
    prim_render_time = stage_->get_render_time();

    // Use this prim's own should_simulate to decide
    if (!should_simulate())
        return;

    assert(node_tree);
    assert(node_tree_executor);

    auto& payload = node_tree_executor->get_global_payload<GeomPayload&>();
    payload.delta_time = delta_time;
#ifdef GEOM_USD_EXTENSION
    payload.stage = prim.GetStage();
    payload.prim_path = prim.GetPath();
#endif
    payload.has_simulation = false;
    if (simulation_begun)
        payload.is_simulating = true;
    else {
        payload.is_simulating = false;
        simulation_begun = true;
    }
#ifdef GEOM_USD_EXTENSION
    payload.current_time = prim_current_time;
#endif
    node_tree_executor->execute(node_tree.get());

    // Update this prim's simulation time
    auto current = prim_current_time.GetValue();
    current += delta_time;
    prim_current_time = pxr::UsdTimeCode(current);
}

// Check whethe r important attributes have time samples
bool WithDynamicLogicPrim::is_animatable(const pxr::UsdPrim& prim)
{
    auto animatable = prim.GetAttribute(pxr::TfToken("Animatable"));

    if (!animatable) {
        return false;
    }
    bool is_animatable = false;
    animatable.Get(&is_animatable);
    return is_animatable;
}

void WithDynamicLogicPrim::clear_time_samples(const pxr::UsdPrim& prim) const
{
    if (!prim.IsValid()) {
        return;
    }

    // Recursively clear time samples for all child prims
    for (const auto& child : prim.GetChildren()) {
        clear_time_samples(child);
    }

    // Clear time samples for all attributes of current prim
    for (const auto& attr : prim.GetAttributes()) {
        // Skip attributes that shouldn't be cleared (configuration-related
        // attributes)
        const auto& attr_name = attr.GetName();
        if (attr_name == pxr::TfToken("node_json") ||
            attr_name == pxr::TfToken("Animatable")) {
            continue;
        }

        // Check if attribute has time samples
        if (attr.GetNumTimeSamples() > 0) {
            // Clear all time samples
            attr.Clear();
        }
    }
}
}  // namespace animation

RUZINO_NAMESPACE_CLOSE_SCOPE
