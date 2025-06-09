#include "GPUContext/compute_context.hpp"
#include "RHI/internal/resources.hpp"
#include "nodes/core/def/node_def.hpp"
#include "nvrhi/nvrhi.h"
#include "render_node_base.h"
#include "shaders/shaders/utils/CameraParameters.h"
#include "shaders/shaders/utils/ray.slang"
#include "shaders/shaders/utils/view_cb.h"
#include "utils/cam_to_view_contants.h"
#include "utils/math.h"
#include "utils/resource_cleaner.hpp"

NODE_DEF_OPEN_SCOPE

struct OldConstants {
    constexpr static bool has_storage = false;

    float aperture = 0;
    float focus_distance = 2;

    bool operator==(const OldConstants& rhs) const
    {
        return aperture == rhs.aperture && focus_distance == rhs.focus_distance;
    }

    bool operator!=(const OldConstants& rhs) const
    {
        return !(*this == rhs);
    }
};

NODE_DECLARATION_FUNCTION(node_render_ray_generation)
{
    b.add_input<nvrhi::TextureHandle>("random seeds");
    b.add_input<float>("Aperture").min(0).max(1).default_val(0);
    b.add_input<float>("Focus Distance").min(0).max(20).default_val(2);
    b.add_input<bool>("Scatter Rays").default_val(false);

    b.add_output<nvrhi::BufferHandle>("Pixel Target");
    b.add_output<nvrhi::BufferHandle>("Rays");
}

NODE_EXECUTION_FUNCTION(node_render_ray_generation)
{
    Hd_USTC_CG_Camera* free_camera = get_free_camera(params);
    auto image_size = free_camera->dataWindow.GetSize();

    auto aperture = params.get_input<float>("Aperture");
    auto focus_distance = params.get_input<float>("Focus Distance");

    if (params.get_storage<OldConstants>() !=
        OldConstants{ aperture, focus_distance }) {
        params.set_storage(OldConstants{ aperture, focus_distance });
        global_payload.reset_accumulation = true;
    }

    auto ray_buffer = create_buffer<RayInfo>(
        params, image_size[0] * image_size[1], false, true);

    auto pixel_target_buffer = create_buffer<GfVec2i>(
        params, image_size[0] * image_size[1], false, true);

    // Prepare the shader using reflection
    ProgramDesc cs_program_desc;
    cs_program_desc.shaderType = nvrhi::ShaderType::Compute;
    cs_program_desc.set_path("shaders/raygen.slang").set_entry_name("main");

    std::vector<ShaderMacro> macro_defines;
    if (params.get_input<bool>("Scatter Rays"))
        macro_defines.push_back(ShaderMacro{ "SCATTER_RAYS", "1" });
    else
        macro_defines.push_back(ShaderMacro{ "SCATTER_RAYS", "0" });

    cs_program_desc.define(macro_defines);

    ProgramHandle cs_program = resource_allocator.create(cs_program_desc);
    MARK_DESTROY_NVRHI_RESOURCE(cs_program);
    CHECK_PROGRAM_ERROR(cs_program);

    auto random_seeds = params.get_input<nvrhi::TextureHandle>("random seeds");
    auto constant_buffer = get_free_camera_planarview_cb(params);
    MARK_DESTROY_NVRHI_RESOURCE(constant_buffer);

    ProgramVars program_vars(resource_allocator, cs_program);
    program_vars["rays"] = ray_buffer;
    program_vars["random_seeds"] = random_seeds;
    program_vars["pixel_targets"] = pixel_target_buffer;
    program_vars["viewConstant"] = constant_buffer;

    CameraParameters camera_params;
    camera_params.aperture = aperture;
    camera_params.focusDistance = focus_distance;

    auto camera_param_cb = create_constant_buffer(params, camera_params);
    MARK_DESTROY_NVRHI_RESOURCE(camera_param_cb);

    program_vars["cameraParams"] = camera_param_cb;

    program_vars.finish_setting_vars();

    ComputeContext context(resource_allocator, program_vars);
    context.finish_setting_pso();

    context.begin();
    context.dispatch({}, program_vars, image_size[0], 32, image_size[1], 32);
    context.finish();

    params.set_output("Rays", ray_buffer);
    params.set_output("Pixel Target", pixel_target_buffer);
    return true;
}

NODE_DECLARATION_UI(node_render_ray_generation);
NODE_DEF_CLOSE_SCOPE
