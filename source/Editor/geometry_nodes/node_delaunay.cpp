#include "GCore/Components/MeshComponent.h"
#include "GCore/algorithms/delauney.h"
#include "nodes/core/def/node_def.hpp"

NODE_DEF_OPEN_SCOPE

NODE_DECLARATION_FUNCTION(delaunay)
{
    b.add_input<Geometry>("Input");
    b.add_input<float>("Maximum Radius").default_val(1.0f).min(0.1f).max(10);
    b.add_output<Geometry>("Output");
}

NODE_EXECUTION_FUNCTION(delaunay)
{
    auto input_geom = params.get_input<Geometry>("Input");
    auto maximum_radius = params.get_input<float>("Maximum Radius");

    // Apply any pending transforms
    input_geom.apply_transform();

    // Check if input has mesh component
    auto mesh_component = input_geom.get_component<MeshComponent>();
    if (!mesh_component) {
        // If no mesh component, return the input geometry unchanged
        params.set_output<Geometry>("Output", std::move(input_geom));
        return true;
    }

    // Perform Delaunay triangulation
    Geometry delaunay_result =
        geom_algorithm::delaunay(input_geom, maximum_radius);

    // If triangulation failed, return empty geometry
    if (!delaunay_result.get_component<MeshComponent>()) {
        params.set_output<Geometry>("Output", Geometry::CreateMesh());
        return true;
    }

    // Iterate through components of the original geometry
    for (const auto& component : input_geom.get_components()) {
        // Check if the component is NOT a MeshComponent
        // If it's not a mesh component, attach it to the new geometry.
        // The delaunay geometry already has the new MeshComponent.
        if (!std::dynamic_pointer_cast<MeshComponent>(component)) {
            delaunay_result.attach_component(component);
        }
    }

    params.set_output<Geometry>("Output", std::move(delaunay_result));

    return true;
}

NODE_DECLARATION_UI(delaunay);

NODE_DEF_CLOSE_SCOPE
