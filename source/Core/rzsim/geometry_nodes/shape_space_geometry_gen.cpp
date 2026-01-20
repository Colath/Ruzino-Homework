#include <filesystem>
#include <memory>

#ifdef _WIN32
#include <windows.h>
#endif

#include "GCore/Components/MeshComponent.h"
#include "GCore/geom_payload.hpp"
#include "GCore/read_geom.h"
#include "nodes/core/def/node_def.hpp"
#include "rzpython/rzpython.hpp"

using namespace Ruzino;

NODE_DEF_OPEN_SCOPE

NODE_DECLARATION_FUNCTION(shape_space_geometry_gen)
{
    b.add_input<float>("Shape Code").default_val(0.0f).min(0.0f).max(1.0f);
    b.add_input<bool>("Has Shape Code 2").default_val(false);
    b.add_input<float>("Shape Code 2").default_val(0.0f).min(0.0f).max(1.0f);
    b.add_input<std::string>("Model Path")
        .default_val("mesh_deform_dirichlet_20260120_113410");
    b.add_input<std::string>("Output Filename")
        .default_val("generated_shape.obj");
    b.add_output<Geometry>("Geometry");
}

NODE_EXECUTION_FUNCTION(shape_space_geometry_gen)
{
    spdlog::info("[ShapeSpaceGeometryGen] Node execution started");

    float shape_code = params.get_input<float>("Shape Code");
    bool has_shape_code_2 = params.get_input<bool>("Has Shape Code 2");
    float shape_code_2 = params.get_input<float>("Shape Code 2");
    std::string model_path = params.get_input<std::string>("Model Path");
    std::string output_filename =
        params.get_input<std::string>("Output Filename");

    try {
        // Import deducer module
        python::call<void>("import torch");
        python::call<void>(
            "import sys\n"
            "sys.path.insert(0, "
            "r'C:"
            "\\Users\\Pengfei\\WorkSpace\\Ruzino\\source\\Core\\rzsim\\geometry"
            "_nodes')\n"
            "import deducer");

        // Initialize basis set
        python::send("model_name", model_path);
        std::string init_result = python::call<std::string>(
            "deducer.initialize_basis_set(model_name)");
        spdlog::info("Basis set initialization: {}", init_result);

        // Generate OBJ file
        std::string test_result;
        if (has_shape_code_2) {
            python::send(
                "shape_codes", std::vector<float>{ shape_code, shape_code_2 });
            python::send("filename", output_filename);
            test_result = python::call<std::string>(
                "deducer.test_save_obj(shape_code_value=shape_codes, "
                "output_filename=filename)");
            spdlog::info(
                "Generated geometry with dual shape codes [{:.2f}, {:.2f}]",
                shape_code,
                shape_code_2);
        }
        else {
            python::send("shape_code", shape_code);
            python::send("filename", output_filename);
            test_result = python::call<std::string>(
                "deducer.test_save_obj(shape_code_value=shape_code, "
                "output_filename=filename)");
            spdlog::info(
                "Generated geometry with shape code {:.2f}", shape_code);
        }

        spdlog::info("OBJ generation result: {}", test_result);

        // Load the generated OBJ file
        std::filesystem::path executable_path;
#ifdef _WIN32
        char p[MAX_PATH];
        GetModuleFileNameA(NULL, p, MAX_PATH);
        executable_path = std::filesystem::path(p).parent_path();
#endif

        std::filesystem::path obj_path = executable_path / output_filename;

        if (!std::filesystem::exists(obj_path)) {
            spdlog::error(
                "Generated OBJ file does not exist: {}", obj_path.string());
            params.set_output<Geometry>("Geometry", Geometry());
            return false;
        }

        // Read the OBJ file back
        Geometry geometry = read_obj_geometry(obj_path.string());
        spdlog::info("Loaded geometry from {}", obj_path.string());

        params.set_output<Geometry>("Geometry", std::move(geometry));
        return true;
    }
    catch (const std::exception& e) {
        spdlog::error("Failed to generate geometry: {}", e.what());
        params.set_output<Geometry>("Geometry", Geometry());
        return false;
    }
}

NODE_DECLARATION_UI(shape_space_geometry_gen);

NODE_DEF_CLOSE_SCOPE
