
#include <random>

#include "GCore/Components/PointsComponent.h"
#include "GCore/GOP.h"
#include "nodes/core/def/node_def.hpp"
#include "nvrhi/nvrhi.h"

NODE_DEF_OPEN_SCOPE
NODE_DECLARATION_FUNCTION(points_contact)
{
    b.add_input<nvrhi::BufferHandle>("AABB Buffer");
    b.add_input<nvrhi::BufferHandle>("Points Buffer");
    b.add_input<bool>("Enable Scene Collision").default_val(false);
    b.add_output<nvrhi::BufferHandle>("Contact ID Pairs");
}

NODE_EXECUTION_FUNCTION(points_contact)
{
    auto aabb_buffer = params.get_input<nvrhi::BufferHandle>("AABB Buffer");
    auto points_buffer = params.get_input<nvrhi::BufferHandle>("Points Buffer");

    // Placeholder for actual contact detection logic
    // In a real implementation, GPU compute shaders would be used here

    nvrhi::BufferHandle
        contact_id_pairs_buffer;  // This would be created and filled with data

    // For demonstration, we just return an empty buffer handle
    params.set_output("Contact ID Pairs", contact_id_pairs_buffer);
}
NODE_DECLARATION_UI(points_contact);
NODE_DEF_CLOSE_SCOPE
