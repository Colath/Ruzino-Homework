#include <Eigen/Sparse>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

#include "GCore/Components/MeshComponent.h"
#include "GCore/util_openmesh_bind.h"
#include "geom_node_base.h"
#include "spdlog/spdlog.h"

/*
** @brief HW4_TutteParameterization
**
** This file contains two nodes whose primary function is to map the boundary of
*a mesh to a plain
** convex closed curve (circle of square), setting the stage for subsequent
*Laplacian equation
** solution and mesh parameterization tasks.
**
** Key to this node's implementation is the adept manipulation of half-edge data
*structures
** to identify and modify the boundary of the mesh.
**
** Task Overview:
** - The two execution functions (node_map_boundary_to_square_exec,
** node_map_boundary_to_circle_exec) require an update to accurately map the
*mesh boundary to a and
** circles. This entails identifying the boundary edges, evenly distributing
*boundary vertices along
** the square's perimeter, and ensuring the internal vertices' positions remain
*unchanged.
** - A focus on half-edge data structures to efficiently traverse and modify
*mesh boundaries.
*/

NODE_DEF_OPEN_SCOPE

namespace {
void extract_longest_boundary_loop(const PolyMesh& mesh, std::vector<int>& out_loop)
{
    out_loop.clear();
    const int n_vertices = static_cast<int>(mesh.n_vertices());
    std::vector<int> boundary_next(n_vertices, -1);

    for (const auto& heh : mesh.halfedges()) {
        if (!heh.is_boundary()) {
            continue;
        }
        boundary_next[heh.from().idx()] = heh.to().idx();
    }

    std::vector<std::vector<int>> loops;
    std::vector<bool> visited(n_vertices, false);
    for (int start = 0; start < n_vertices; ++start) {
        if (boundary_next[start] < 0 || visited[start]) {
            continue;
        }

        std::vector<int> loop;
        int cur = start;
        do {
            if (cur < 0 || visited[cur]) {
                break;
            }
            visited[cur] = true;
            loop.push_back(cur);
            cur = boundary_next[cur];
        } while (cur != start);

        if (!loop.empty() && cur == start) {
            loops.push_back(std::move(loop));
        }
    }

    if (loops.empty()) {
        return;
    }

    std::sort(
        loops.begin(),
        loops.end(),
        [](const auto& a, const auto& b) { return a.size() > b.size(); });
    out_loop = std::move(loops.front());
}

void normalized_boundary_arclength(
    PolyMesh* mesh,
    const std::vector<int>& loop,
    std::vector<double>& t)
{
    t.clear();
    t.resize(loop.size(), 0.0);
    if (loop.empty()) {
        return;
    }

    double total_len = 0.0;
    std::vector<double> edge_len(loop.size(), 0.0);
    for (size_t i = 0; i < loop.size(); ++i) {
        const auto vh0 = mesh->vertex_handle(loop[i]);
        const auto vh1 = mesh->vertex_handle(loop[(i + 1) % loop.size()]);
        const auto p0 = mesh->point(vh0);
        const auto p1 = mesh->point(vh1);
        edge_len[i] = (p1 - p0).length();
        total_len += edge_len[i];
    }

    if (total_len <= 1e-12) {
        for (size_t i = 0; i < loop.size(); ++i) {
            t[i] = static_cast<double>(i) / static_cast<double>(loop.size());
        }
        return;
    }

    double acc = 0.0;
    for (size_t i = 0; i < loop.size(); ++i) {
        t[i] = acc / total_len;
        acc += edge_len[i];
    }
}
}  // namespace

/*
** HW4_TODO: Node to map the mesh boundary to a circle.
*/

NODE_DECLARATION_FUNCTION(hw5_circle_boundary_mapping)
{
    // Input-1: Original 3D mesh with boundary
    b.add_input<Geometry>("Input");
    // Output-1: Processed 3D mesh whose boundary is mapped to a square and the
    // interior vertices remains the same
    b.add_output<Geometry>("Output");
}

NODE_EXECUTION_FUNCTION(hw5_circle_boundary_mapping)
{
    // Get the input from params
    auto input = params.get_input<Geometry>("Input");

    // (TO BE UPDATED) Avoid processing the node when there is no input
    if (!input.get_component<MeshComponent>()) {
        spdlog::error("Boundary Mapping: Need Geometry Input.");
        return false;
    }

    /* ----------------------------- Preprocess -------------------------------
    ** Create a halfedge structure (using OpenMesh) for the input mesh. The
    ** half-edge data structure is a widely used data structure in geometric
    ** processing, offering convenient operations for traversing and modifying
    ** mesh elements.
    */
    auto halfedge_mesh = operand_to_openmesh(&input);

    std::vector<int> boundary_loop;
    extract_longest_boundary_loop(*halfedge_mesh, boundary_loop);
    
    if (boundary_loop.empty()) {
        spdlog::warn(
            "hw5_circle_boundary_mapping: no boundary loop found, pass-through "
            "input mesh.");
        params.set_output("Output", std::move(input));
        return true;
    }

    std::vector<double> t;
    normalized_boundary_arclength(halfedge_mesh.get(), boundary_loop, t);
    
    constexpr double pi = 3.14159265358979323846;
    for (size_t i = 0; i < boundary_loop.size(); ++i) {
        const double theta = 2.0 * pi * t[i];
        auto vh = halfedge_mesh->vertex_handle(boundary_loop[i]);
        halfedge_mesh->point(vh)[0] = static_cast<float>(0.5 + 0.5 * std::cos(theta));
        halfedge_mesh->point(vh)[1] = static_cast<float>(0.5 + 0.5 * std::sin(theta));
        halfedge_mesh->point(vh)[2] = 0.0f;
    }

    /* ----------- [HW4_TODO] TASK 2.1: Boundary Mapping (to circle)
     *------------
     *... (comments omitted for brevity, kept in actual file) ...
     */

    /* ----------------------------- Postprocess ------------------------------
    ** Convert the result mesh from the halfedge structure back to Geometry
    *format as the node's
    ** output.
    */
    auto geometry = openmesh_to_operand(halfedge_mesh.get());

    // Set the output of the nodes
    params.set_output("Output", std::move(*geometry));
    return true;
}

/*
** HW4_TODO: Node to map the mesh boundary to a square.
*/

NODE_DECLARATION_FUNCTION(hw5_square_boundary_mapping)
{
    // Input-1: Original 3D mesh with boundary
    b.add_input<Geometry>("Input");

    // Output-1: Processed 3D mesh whose boundary is mapped to a square and the
    // interior vertices remains the same
    b.add_output<Geometry>("Output");
}

NODE_EXECUTION_FUNCTION(hw5_square_boundary_mapping)
{
    // Get the input from params
    auto input = params.get_input<Geometry>("Input");

    // (TO BE UPDATED) Avoid processing the node when there is no input
    if (!input.get_component<MeshComponent>()) {
        spdlog::error("Input does not contain a mesh");
        return false;
    }

    /* ----------------------------- Preprocess -------------------------------
    ** Create a halfedge structure (using OpenMesh) for the input mesh.
    */
    auto halfedge_mesh = operand_to_openmesh(&input);

    std::vector<int> boundary_loop;
    extract_longest_boundary_loop(*halfedge_mesh, boundary_loop);
    
    if (boundary_loop.empty()) {
        spdlog::warn(
            "hw5_square_boundary_mapping: no boundary loop found, pass-through "
            "input mesh.");
        params.set_output("Output", std::move(input));
        return true;
    }

    std::vector<double> t;
    normalized_boundary_arclength(halfedge_mesh.get(), boundary_loop, t);
    
    for (size_t i = 0; i < boundary_loop.size(); ++i) {
        const double s = std::clamp(t[i], 0.0, 1.0 - 1e-12);
        const double q = s * 4.0;
        double x = 0.0;
        double y = 0.0;
        if (q < 1.0) {
            x = q;
            y = 0.0;
        }
        else if (q < 2.0) {
            x = 1.0;
            y = q - 1.0;
        }
        else if (q < 3.0) {
            x = 3.0 - q;
            y = 1.0;
        }
        else {
            x = 0.0;
            y = 4.0 - q;
        }

        auto vh = halfedge_mesh->vertex_handle(boundary_loop[i]);
        halfedge_mesh->point(vh)[0] = static_cast<float>(x);
        halfedge_mesh->point(vh)[1] = static_cast<float>(y);
        halfedge_mesh->point(vh)[2] = 0.0f;
    }

    /* ----------- [HW4_TODO] TASK 2.2: Boundary Mapping (to square)
     *------------
     *... (comments omitted for brevity, kept in actual file) ...
     */

    /* ----------------------------- Postprocess ------------------------------
    ** Convert the result mesh from the halfedge structure back to Geometry
    *format as the node's
    ** output.
    */
    auto geometry = openmesh_to_operand(halfedge_mesh.get());

    // Set the output of the nodes
    params.set_output("Output", std::move(*geometry));
    return true;
}

NODE_DECLARATION_UI(hw5_circle_boundary_mapping);
NODE_DECLARATION_UI(hw5_square_boundary_mapping);

NODE_DEF_CLOSE_SCOPE
