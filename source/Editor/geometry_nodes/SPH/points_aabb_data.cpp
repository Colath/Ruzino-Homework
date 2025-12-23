
#include "GCore/Components/PointsComponent.h"
#include "GCore/GOP.h"
#include "nodes/core/def/node_def.hpp"
#include "nvrhi/nvrhi.h"

NODE_DEF_OPEN_SCOPE
NODE_DECLARATION_FUNCTION(points_to_aabb_data)
{
    b.add_input<Geometry>("Points");
    b.add_output<nvrhi::BufferHandle>("Points Buffer");
    b.add_output<nvrhi::BufferHandle>("AABB Data Buffer");
}

NODE_EXECUTION_FUNCTION(points_to_aabb_data)
{
    auto points_buffer = params.get_input<nvrhi::BufferHandle>("Points Buffer");

    // Placeholder for actual AABB computation logic
    // In a real implementation, GPU compute shaders would be used here

    nvrhi::BufferHandle
        aabb_data_buffer;  // This would be created and filled with data

    // For demonstration, we just return an empty buffer handle
    params.set_output("AABB Data Buffer", aabb_data_buffer);
}

NODE_DECLARATION_UI(points_to_aabb_data);
NODE_DEF_CLOSE_SCOPE