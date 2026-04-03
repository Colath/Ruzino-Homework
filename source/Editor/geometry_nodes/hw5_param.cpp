#include <Eigen/Sparse>
#include <cmath>

//#include "GCore/Components/MeshOperand.h"
#include "GCore/util_openmesh_bind.h"
#include <pxr/usd/usdGeom/mesh.h>

#include <Eigen/Core>
#include <Eigen/Eigen>
#include <algorithm>
#include <cfloat>
#include <cstdlib>
#include <memory>
#include <unordered_set>
#include <utility>
#include <vector>

#include "GCore/Components.h"
#include "GCore/Components/MeshComponent.h"
#include "GCore/GOP.h"
#include "GCore/util_openmesh_bind.h"
#include "geom_node_base.h"
#include "nodes/core/def/node_def.hpp"
#include "geom_node_base.h"
#include "spdlog/spdlog.h"

/*
** @brief HW4_TutteParameterization
**
** This file presents the basic framework of a "node", which processes inputs
** received from the left and outputs specific variables for downstream nodes to
** use.
** - In the first function, node_declare, you can set up the node's input and
** output variables.
** - The second function, node_exec is the execution part of the node, where we
** need to implement the node's functionality.
** Your task is to fill in the required logic at the specified locations
** within this template, especially in node_exec.
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
        return ;
    }

    std::sort(
        loops.begin(),
        loops.end(),
        [](const std::vector<int>& a, const std::vector<int>& b) { return a.size() > b.size(); });
    out_loop = std::move(loops.front());
}

double cotangent_at_halfedge(const PolyMesh& mesh, PolyMesh::HalfedgeHandle hh)
{
    if (!hh.is_valid() || mesh.is_boundary(hh)) {
        return 0.0;
    }

    const auto vh_i = mesh.from_vertex_handle(hh);
    const auto vh_j = mesh.to_vertex_handle(hh);
    const auto vh_k = mesh.to_vertex_handle(mesh.next_halfedge_handle(hh));

    const auto pi = mesh.point(vh_i);
    const auto pj = mesh.point(vh_j);
    const auto pk = mesh.point(vh_k);

    Eigen::Vector3d a(pi[0] - pk[0], pi[1] - pk[1], pi[2] - pk[2]);
    Eigen::Vector3d b(pj[0] - pk[0], pj[1] - pk[1], pj[2] - pk[2]);
    const double denom = a.cross(b).norm();
    if (denom < 1e-12) {
        return 0.0;
    }
    return a.dot(b) / denom;
}

double edge_weight(
    const PolyMesh& weight_mesh,
    int vi,
    int vj,
    int weight_type)
{
    if (weight_type == 0) {
        return 1.0;
    }

    const auto vh_i = weight_mesh.vertex_handle(vi);
    const auto vh_j = weight_mesh.vertex_handle(vj);
    const auto hh_ij = weight_mesh.find_halfedge(vh_i, vh_j);
    const auto hh_ji = weight_mesh.find_halfedge(vh_j, vh_i);
    return cotangent_at_halfedge(weight_mesh, hh_ij) +
           cotangent_at_halfedge(weight_mesh, hh_ji);
}

double safe_angle_between(const Eigen::Vector3d& a, const Eigen::Vector3d& b)
{
    const double na = a.norm();
    const double nb = b.norm();
    if (na < 1e-12 || nb < 1e-12) {
        return 0.0;
    }
    double c = a.dot(b) / (na * nb);
    c = std::max(-1.0, std::min(1.0, c));
    return std::acos(c);
}

void build_neighbor_weights(
    const PolyMesh& topology_mesh,
    const PolyMesh& weight_mesh,
    PolyMesh::VertexHandle vh,
    int weight_type,
    std::vector<std::pair<int, double>>& neighbors)
{
    neighbors.clear();
    std::vector<int> ordered_neighbors;
    for (auto vv_it = topology_mesh.cvv_iter(vh); vv_it.is_valid(); ++vv_it) {
        ordered_neighbors.push_back(vv_it->idx());
    }
    if (ordered_neighbors.empty()) {
        return ;
    }

    // 0: Uniform, 1: Cotangent, 2: Floater(Mean Value) weights.
    if (weight_type < 0 || weight_type > 2) {
        weight_type = 0;
    }

    neighbors.reserve(ordered_neighbors.size());

    if (weight_type == 2) {
        const int m = static_cast<int>(ordered_neighbors.size());
        std::vector<Eigen::Vector3d> vecs;
        std::vector<double> lens;
        std::vector<double> alphas;
        vecs.resize(m);
        lens.resize(m);
        alphas.resize(m);
        const auto p = weight_mesh.point(weight_mesh.vertex_handle(vh.idx()));

        for (int j = 0; j < m; ++j) {
            const auto pj = weight_mesh.point(weight_mesh.vertex_handle(ordered_neighbors[j]));
            vecs[j] = Eigen::Vector3d(pj[0] - p[0], pj[1] - p[1], pj[2] - p[2]);
            lens[j] = vecs[j].norm();
        }

        for (int j = 0; j < m; ++j) {
            const int jn = (j + 1) % m;
            alphas[j] = safe_angle_between(vecs[j], vecs[jn]);
        }

        for (int j = 0; j < m; ++j) {
            const int jp = (j - 1 + m) % m;
            const double lj = lens[j];
            double wj = 0.0;
            if (lj > 1e-12) {
                const double t0 = std::tan(0.5 * alphas[jp]);
                const double t1 = std::tan(0.5 * alphas[j]);
                wj = (t0 + t1) / lj;
            }
            if (!std::isfinite(wj)) {
                wj = 0.0;
            }
            neighbors.emplace_back(ordered_neighbors[j], wj);
        }
        return;
    }

    const int vi = vh.idx();
    for (int vj : ordered_neighbors) {
        const double wij = edge_weight(weight_mesh, vi, vj, weight_type);
        if (!std::isfinite(wij)) {
            continue;
        }
        neighbors.emplace_back(vj, wij);
    }
}
}  // namespace

NODE_DECLARATION_FUNCTION(hw5_param)
{
    // Input-1: Original 3D mesh with boundary
    b.add_input<Geometry>("Input");
    b.add_input<Geometry>("Reference");
    b.add_input<int>("Weight Type")
        .default_val(0)
        .min(0)
        .max(2);  // 0: Uniform, 1: Cotangent, 2: Floater

    /*
    ** NOTE: You can add more inputs or outputs if necessary. For example, in
    *some cases,
    ** additional information (e.g. other mesh geometry, other parameters) is
    *required to perform
    ** the computation.
    **
    ** Be sure that the input/outputs do not share the same name. You can add
    *one geometry as
    **
    **                b.add_input<Geometry>("Input");
    **
    ** Or maybe you need a value buffer like:
    **
    **                b.add_input<float1Buffer>("Weights");
    */

    // Output-1: Minimal surface with fixed boundary
    b.add_output<Geometry>("Output");
}

NODE_EXECUTION_FUNCTION(hw5_param)
{
    // Get the input from params
    auto input = params.get_input<Geometry>("Input");
    auto reference = params.get_input<Geometry>("Reference");
    int weight_type = params.get_input<int>("Weight Type");

    // (TO BE UPDATED) Avoid processing the node when there is no input
    if (!input.get_component<MeshComponent>()) {
        spdlog::error("Minimal Surface: Need Geometry Input.");
        return false;
    }

    /* ----------------------------- Preprocess -------------------------------
    ** Create a halfedge structure (using OpenMesh) for the input mesh. The
    ** half-edge data structure is a widely used data structure in geometric
    ** processing, offering convenient operations for traversing and modifying
    ** mesh elements.
    */
    auto halfedge_mesh = operand_to_openmesh(&input);
    std::shared_ptr<PolyMesh> weight_mesh = halfedge_mesh;
    if (reference.get_component<MeshComponent>()) {
        auto ref_mesh = operand_to_openmesh(&reference);
        if (ref_mesh && ref_mesh->n_vertices() == halfedge_mesh->n_vertices()) {
            weight_mesh = ref_mesh;
        }
    }
    const int n_vertices = static_cast<int>(halfedge_mesh->n_vertices());
    if (n_vertices == 0) {
        auto geometry = openmesh_to_operand(halfedge_mesh.get());
        params.set_output("Output", std::move(*geometry));
        return true;
    }

    std::vector<int> boundary_loop;
    extract_longest_boundary_loop(*halfedge_mesh, boundary_loop);
    if (boundary_loop.empty()) {
        spdlog::warn(
            "hw5_param: no boundary loop found, pass-through input mesh.");
        params.set_output("Output", std::move(input));
        return true;
    }

    std::vector<char> is_boundary(n_vertices, 0);
    for (const auto& vh : halfedge_mesh->vertices()) {
        if (halfedge_mesh->is_boundary(vh)) {
            is_boundary[vh.idx()] = 1;
        }
    }

    std::vector<int> vid_to_row(n_vertices, -1);
    int n_interior = 0;
    int n_boundary = 0;
    for (const auto& vh : halfedge_mesh->vertices()) {
        if (!is_boundary[vh.idx()]) {
            vid_to_row[vh.idx()] = n_interior++;
        }
        else {
            ++n_boundary;
        }
    }
    spdlog::info(
        "hw5_param: vertices={}, boundary={}, interior={}, weight_type={}",
        n_vertices,
        n_boundary,
        n_interior,
        weight_type);

    if (n_interior > 0) {
        std::vector<Eigen::Triplet<double>> triplets;
        triplets.reserve(static_cast<size_t>(n_interior) * 8);

        Eigen::VectorXd bx = Eigen::VectorXd::Zero(n_interior);
        Eigen::VectorXd by = Eigen::VectorXd::Zero(n_interior);
        Eigen::VectorXd bz = Eigen::VectorXd::Zero(n_interior);

        for (const auto& vh : halfedge_mesh->vertices()) {
            const int vi = vh.idx();
            if (is_boundary[vi]) {
                continue;
            }

            const int row = vid_to_row[vi];
            triplets.emplace_back(row, row, 1.0);

            std::vector<std::pair<int, double>> neighbors;
            build_neighbor_weights(*halfedge_mesh, *weight_mesh, vh, weight_type, neighbors);
            double w_sum = 0.0;
            for (size_t i = 0; i < neighbors.size(); ++i) {
                w_sum += neighbors[i].second;
            }

            if (neighbors.empty()) {
                continue;
            }

            if (std::abs(w_sum) < 1e-12) {
                const double uniform = 1.0 / static_cast<double>(neighbors.size());
                for (size_t i = 0; i < neighbors.size(); ++i) {
                    neighbors[i].second = uniform;
                }
            }
            else {
                for (size_t i = 0; i < neighbors.size(); ++i) {
                    neighbors[i].second /= w_sum;
                }
            }

            for (size_t i = 0; i < neighbors.size(); ++i) {
                const int vj = neighbors[i].first;
                const double wij = neighbors[i].second;
                if (is_boundary[vj]) {
                    const auto pj =
                        halfedge_mesh->point(halfedge_mesh->vertex_handle(vj));
                    bx(row) += wij * pj[0];
                    by(row) += wij * pj[1];
                    bz(row) += wij * pj[2];
                }
                else {
                    triplets.emplace_back(row, vid_to_row[vj], -wij);
                }
            }
        }

        Eigen::SparseMatrix<double> A(n_interior, n_interior);
        A.setFromTriplets(triplets.begin(), triplets.end());
        A.makeCompressed();

        Eigen::SparseLU<Eigen::SparseMatrix<double>> solver;
        solver.compute(A);
        if (solver.info() != Eigen::Success) {
            spdlog::warn(
                "hw5_param: matrix factorization failed, pass-through input mesh.");
            params.set_output("Output", std::move(input));
            return true;
        }

        const Eigen::VectorXd x = solver.solve(bx);
        const Eigen::VectorXd y = solver.solve(by);
        const Eigen::VectorXd z = solver.solve(bz);
        if (solver.info() != Eigen::Success) {
            spdlog::warn(
                "hw5_param: linear solve failed, pass-through input mesh.");
            params.set_output("Output", std::move(input));
            return true;
        }

        if (!x.allFinite() || !y.allFinite() || !z.allFinite()) {
            spdlog::warn(
                "hw5_param: non-finite solution detected, pass-through input "
                "mesh.");
            params.set_output("Output", std::move(input));
            return true;
        }

        for (const auto& vh : halfedge_mesh->vertices()) {
            const int vi = vh.idx();
            if (is_boundary[vi]) {
                continue;
            }
            const int row = vid_to_row[vi];
            halfedge_mesh->point(vh)[0] = static_cast<float>(x(row));
            halfedge_mesh->point(vh)[1] = static_cast<float>(y(row));
            halfedge_mesh->point(vh)[2] = static_cast<float>(z(row));
        }
    }
    /* ---------------- [HW4_TODO] TASK 1: Minimal Surface --------------------
    ** In this task, you are required to generate a 'minimal surface' mesh with
    ** the boundary of the input mesh as its boundary.
    **
    ** Specifically, the positions of the boundary vertices of the input mesh
    ** should be fixed. By solving a global Laplace equation on the mesh,
    ** recalculate the coordinates of the vertices inside the mesh to achieve
    ** the minimal surface configuration.
    **
    ** (Recall the Poisson equation with Dirichlet Boundary Condition in HW3)
    */

    /*
    ** Algorithm Pseudocode for Minimal Surface Calculation
    ** ------------------------------------------------------------------------
    ** 1. Initialize mesh with input boundary conditions.
    **    - For each boundary vertex, fix its position.
    **    - For internal vertices, initialize with initial guess if necessary.
    **
    ** 2. Construct Laplacian matrix for the mesh.
    **    - Compute weights for each edge based on the chosen weighting scheme
    **      (e.g., uniform weights for simplicity).
    **    - Assemble the global Laplacian matrix.
    **
    ** 3. Solve the Laplace equation for interior vertices.
    **    - Apply Dirichlet boundary conditions for boundary vertices.
    **    - Solve the linear system (Laplacian * X = 0) to find new positions
    **      for internal vertices.
    **
    ** 4. Update mesh geometry with new vertex positions.
    **    - Ensure the mesh respects the minimal surface configuration.
    **
    ** Note: This pseudocode outlines the general steps for calculating a
    ** minimal surface mesh given fixed boundary conditions using the Laplace
    ** equation. The specific implementation details may vary based on the mesh
    ** representation and numerical methods used.
    **
    */

    /* ----------------------------- Postprocess ------------------------------
    ** Convert the minimal surface mesh from the halfedge structure back to
    ** Geometry format as the node's output.
    */
    auto geometry = openmesh_to_operand(halfedge_mesh.get());

    // Set the output of the nodes
    params.set_output("Output", std::move(*geometry));
    return true;
}

NODE_DECLARATION_UI(hw5_param);
NODE_DEF_CLOSE_SCOPE