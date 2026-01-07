#include <RHI/cuda.hpp>
#include <glm/glm.hpp>
#include <set>

#include "GCore/Components/MeshComponent.h"
#include "GCore/Components/PointsComponent.h"
#include "GCore/geom_payload.hpp"
#include "nodes/core/def/node_def.hpp"
#include "rzsim_cuda/mass_spring_implicit.cuh"
#include "spdlog/spdlog.h"

NODE_DEF_OPEN_SCOPE

// Storage for persistent GPU simulation state
struct MassSpringImplicitGPUStorage {
    // Device buffers (persistent across frames)
    cuda::CUDALinearBufferHandle d_velocities;        // 3*n floats
    cuda::CUDALinearBufferHandle d_rest_positions;    // 3*n floats
    cuda::CUDALinearBufferHandle d_springs;           // 2*m ints
    cuda::CUDALinearBufferHandle d_rest_lengths;      // m floats
    cuda::CUDALinearBufferHandle d_spring_stiffness;  // m floats

    // Temporary device buffers
    cuda::CUDALinearBufferHandle d_positions;   // 3*n floats
    cuda::CUDALinearBufferHandle d_x_tilde;     // 3*n floats
    cuda::CUDALinearBufferHandle d_gradient;    // 3*n floats
    cuda::CUDALinearBufferHandle d_search_dir;  // 3*n floats
    cuda::CUDALinearBufferHandle d_temp_vec;    // 3*n floats
    cuda::CUDALinearBufferHandle d_M_diag;      // 3*n floats
    cuda::CUDALinearBufferHandle d_f_ext;       // 3*n floats

    // CSR format Hessian matrix
    cuda::CUDALinearBufferHandle d_row_offsets;  // (3*n+1) ints
    cuda::CUDALinearBufferHandle d_col_indices;  // nnz ints
    cuda::CUDALinearBufferHandle d_values;       // nnz floats

    int num_particles = 0;
    int num_springs = 0;
    int nnz = 0;  // Non-zeros in Hessian
    bool initialized = false;

    constexpr static bool has_storage = false;

    void allocate(int n_particles, int n_springs, int hessian_nnz)
    {
        num_particles = n_particles;
        num_springs = n_springs;
        nnz = hessian_nnz;

        // Allocate persistent buffers
        d_velocities = cuda::create_cuda_linear_buffer<float>(3 * n_particles);
        d_rest_positions =
            cuda::create_cuda_linear_buffer<float>(3 * n_particles);
        d_springs = cuda::create_cuda_linear_buffer<int>(2 * n_springs);
        d_rest_lengths = cuda::create_cuda_linear_buffer<float>(n_springs);
        d_spring_stiffness = cuda::create_cuda_linear_buffer<float>(n_springs);

        // Allocate temporary buffers
        d_positions = cuda::create_cuda_linear_buffer<float>(3 * n_particles);
        d_x_tilde = cuda::create_cuda_linear_buffer<float>(3 * n_particles);
        d_gradient = cuda::create_cuda_linear_buffer<float>(3 * n_particles);
        d_search_dir = cuda::create_cuda_linear_buffer<float>(3 * n_particles);
        d_temp_vec = cuda::create_cuda_linear_buffer<float>(3 * n_particles);
        d_M_diag = cuda::create_cuda_linear_buffer<float>(3 * n_particles);
        d_f_ext = cuda::create_cuda_linear_buffer<float>(3 * n_particles);

        // Allocate CSR matrix buffers
        d_row_offsets =
            cuda::create_cuda_linear_buffer<int>(3 * n_particles + 1);
        d_col_indices = cuda::create_cuda_linear_buffer<int>(hessian_nnz);
        d_values = cuda::create_cuda_linear_buffer<float>(hessian_nnz);

        initialized = true;
    }
};

NODE_DECLARATION_FUNCTION(mass_spring_implicit_gpu)
{
    b.add_input<Geometry>("Geometry");
    b.add_input<float>("Mass").default_val(1.0f).min(0.01f).max(100.0f);
    b.add_input<float>("Stiffness")
        .default_val(1000.0f)
        .min(1.0f)
        .max(10000.0f);
    b.add_input<float>("Damping").default_val(0.99f).min(0.0f).max(1.0f);
    b.add_input<int>("Newton Iterations").default_val(30).min(1).max(100);
    b.add_input<float>("Newton Tolerance")
        .default_val(1e-2f)
        .min(1e-8f)
        .max(1e-1f);
    b.add_input<float>("Gravity").default_val(-9.81f).min(-20.0f).max(0.0f);
    b.add_input<float>("Ground Restitution")
        .default_val(0.3f)
        .min(0.0f)
        .max(1.0f);
    b.add_input<bool>("Flip Normal").default_val(false);

    b.add_output<Geometry>("Geometry");
}

NODE_EXECUTION_FUNCTION(mass_spring_implicit_gpu)
{
    auto& global_payload = params.get_global_payload<GeomPayload&>();
    auto& storage = params.get_storage<MassSpringImplicitGPUStorage&>();

    // Get inputs
    auto input_geom = params.get_input<Geometry>("Geometry");
    input_geom.apply_transform();

    float mass = params.get_input<float>("Mass");
    float stiffness = params.get_input<float>("Stiffness");
    float damping = params.get_input<float>("Damping");
    int max_iterations = params.get_input<int>("Newton Iterations");
    float tolerance = params.get_input<float>("Newton Tolerance");
    tolerance = std::max(tolerance, 1e-8f);
    float gravity = params.get_input<float>("Gravity");
    float restitution = params.get_input<float>("Ground Restitution");
    bool flip_normal = params.get_input<bool>("Flip Normal");
    float dt = global_payload.delta_time;

    printf(
        "[GPU Params] mass=%.2f, k=%.1f, damp=%.3f, maxIter=%d, tol=%.2e, "
        "g=%.2f, rest=%.2f, dt=%.6f\\n",
        mass,
        stiffness,
        damping,
        max_iterations,
        tolerance,
        gravity,
        restitution,
        dt);

    // Get mesh component
    auto mesh_component = input_geom.get_component<MeshComponent>();
    std::vector<glm::vec3> positions;
    std::vector<int> face_vertex_indices;
    std::vector<int> face_counts;

    if (mesh_component) {
        positions = mesh_component->get_vertices();
        face_vertex_indices = mesh_component->get_face_vertex_indices();
        face_counts = mesh_component->get_face_vertex_counts();
    }
    else {
        auto points_component = input_geom.get_component<PointsComponent>();
        if (!points_component) {
            params.set_output<Geometry>("Geometry", std::move(input_geom));
            return true;
        }
        positions = points_component->get_vertices();
    }

    int num_particles = positions.size();
    if (num_particles == 0) {
        params.set_output<Geometry>("Geometry", std::move(input_geom));
        return true;
    }

    // Convert positions to flat array
    std::vector<float> positions_flat(3 * num_particles);
    for (int i = 0; i < num_particles; i++) {
        positions_flat[3 * i] = positions[i].x;
        positions_flat[3 * i + 1] = positions[i].y;
        positions_flat[3 * i + 2] = positions[i].z;
    }

    // Prepare output arrays
    std::vector<float> positions_out(3 * num_particles);
    std::vector<float> velocities_out(3 * num_particles);

    // Initialize storage if needed
    if (!storage.initialized) {
        // Estimate Hessian NNZ based on mesh connectivity
        int hessian_nnz = 3 * num_particles * 10;  // ~10 neighbors per vertex
        storage.allocate(num_particles, 0, hessian_nnz);
    }

    // Call GPU simulation, passing device pointer from storage
    bool success = rzsim_cuda::mass_spring_implicit_gpu_step(
        reinterpret_cast<float*>(storage.d_positions->get_device_ptr()),
        positions_flat.data(),
        num_particles,
        face_vertex_indices.data(),
        face_vertex_indices.size(),
        face_counts.data(),
        face_counts.size(),
        positions_out.data(),
        velocities_out.data(),
        mass,
        stiffness,
        damping,
        max_iterations,
        tolerance,
        gravity,
        restitution,
        dt);

    if (!success) {
        spdlog::warn("[GPU] Simulation failed");
    }

    // Convert back to glm format
    std::vector<glm::vec3> new_positions(num_particles);
    for (int i = 0; i < num_particles; i++) {
        new_positions[i].x = positions_out[3 * i];
        new_positions[i].y = positions_out[3 * i + 1];
        new_positions[i].z = positions_out[3 * i + 2];
    }

    // Update geometry with new positions
    if (mesh_component) {
        mesh_component->set_vertices(new_positions);

        // Recalculate normals
        std::vector<glm::vec3> normals;
        normals.reserve(face_vertex_indices.size());

        int idx = 0;
        for (int face_count : face_counts) {
            if (face_count >= 3) {
                int i0 = face_vertex_indices[idx];
                int i1 = face_vertex_indices[idx + 1];
                int i2 = face_vertex_indices[idx + 2];

                glm::vec3 edge1 = new_positions[i1] - new_positions[i0];
                glm::vec3 edge2 = new_positions[i2] - new_positions[i0];
                glm::vec3 normal = glm::cross(
                    flip_normal ? edge1 : edge2, flip_normal ? edge2 : edge1);

                float length = glm::length(normal);
                if (length > 1e-8f) {
                    normal = normal / length;
                }
                else {
                    normal = glm::vec3(0.0f, 0.0f, 1.0f);
                }

                for (int i = 0; i < face_count; ++i) {
                    normals.push_back(normal);
                }
            }
            idx += face_count;
        }

        if (!normals.empty()) {
            mesh_component->set_normals(normals);
        }
    }
    else {
        auto points_component = input_geom.get_component<PointsComponent>();
        points_component->set_vertices(new_positions);
    }

    params.set_output<Geometry>("Geometry", std::move(input_geom));
    return true;
}

NODE_DECLARATION_UI(mass_spring_implicit_gpu);
NODE_DEF_CLOSE_SCOPE
