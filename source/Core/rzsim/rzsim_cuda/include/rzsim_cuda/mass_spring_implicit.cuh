#pragma once

#include <RHI/cuda.hpp>

#include "api.h"

RUZINO_NAMESPACE_OPEN_SCOPE

namespace rzsim_cuda {

// Main GPU simulation function (uses float* for positions to avoid glm types in
// header) positions_in: flat array of 3*n floats (x0,y0,z0, x1,y1,z1, ...)
// positions_out, velocities_out: preallocated arrays of 3*n floats
// d_positions: device pointer to position buffer
RZSIM_CUDA_API bool mass_spring_implicit_gpu_step(
    float* d_positions,
    const float* positions_in,
    int num_particles,
    const int* face_vertex_indices,
    int num_face_indices,
    const int* face_counts,
    int num_faces,
    float* positions_out,
    float* velocities_out,
    float mass,
    float stiffness,
    float damping,
    int max_iterations,
    float tolerance,
    float gravity,
    float restitution,
    float dt);

}  // namespace rzsim_cuda

RUZINO_NAMESPACE_CLOSE_SCOPE
