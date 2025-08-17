#include <GCore/algorithms/delauney.h>

#include "GCore/Components/MeshComponent.h"
#include <igl/triangle/triangulate.h>
#include <igl/triangle/cdt.h>
#include <igl/triangle/refine.h>
#include <sstream>
#include <map>

USTC_CG_NAMESPACE_OPEN_SCOPE

namespace geom_algorithm {
Geometry delaunay(const Geometry& geometry, float maximum_radius)
{
    auto input_mesh = geometry.get_component<MeshComponent>();
    if (!input_mesh) {
        return Geometry::CreateMesh();
    }

    auto input_view = input_mesh->get_igl_view();

    // Get input vertices and project to 2D (use X,Y coordinates)
    Eigen::MatrixXd vertices_3d = input_view.get_vertices();
    if (vertices_3d.rows() == 0) {
        return Geometry::CreateMesh();
    }

    // Project to 2D by taking X,Y coordinates
    Eigen::MatrixXd vertices_2d = vertices_3d.leftCols(2);

    // Create constraint edges from the boundary of the input mesh
    Eigen::MatrixXi constraint_edges;
    
    // If we have an input mesh with faces, extract boundary edges
    Eigen::MatrixXi input_faces = input_view.get_faces();
    if (input_faces.rows() > 0) {
        // Extract all edges and find boundary (edges that appear only once)
        std::map<std::pair<int, int>, int> edge_count;
        
        for (int i = 0; i < input_faces.rows(); ++i) {
            for (int j = 0; j < 3; ++j) {
                int v1 = input_faces(i, j);
                int v2 = input_faces(i, (j + 1) % 3);
                if (v1 > v2) std::swap(v1, v2);
                edge_count[{v1, v2}]++;
            }
        }
        
        // Collect boundary edges (appear only once)
        std::vector<std::pair<int, int>> boundary_edges;
        for (const auto& edge_pair : edge_count) {
            if (edge_pair.second == 1) {
                boundary_edges.push_back(edge_pair.first);
            }
        }
        
        constraint_edges.resize(boundary_edges.size(), 2);
        for (int i = 0; i < boundary_edges.size(); ++i) {
            constraint_edges(i, 0) = boundary_edges[i].first;
            constraint_edges(i, 1) = boundary_edges[i].second;
        }
    }
    else {
        // If no faces, create constraint edges from consecutive vertices (assuming boundary loop)
        if (vertices_2d.rows() > 2) {
            constraint_edges.resize(vertices_2d.rows(), 2);
            for (int i = 0; i < vertices_2d.rows(); ++i) {
                constraint_edges(i, 0) = i;
                constraint_edges(i, 1) = (i + 1) % vertices_2d.rows();
            }
        }
        else {
            constraint_edges.resize(0, 2);
        }
    }

    // Prepare triangle flags for quality mesh generation
    std::ostringstream flags_stream;
    flags_stream << "pzDq"; // p: PSLG, z: zero-based indexing, D: conforming Delaunay, q: quality mesh
    
    if (maximum_radius > 0) {
        // Use area constraint: maximum triangle area = (maximum_radius)^2
        flags_stream << "a" << (maximum_radius * maximum_radius);
    }
    
    std::string flags = flags_stream.str();

    // Output matrices
    Eigen::MatrixXd output_vertices;
    Eigen::MatrixXi output_faces;

    try {
        // Use triangulate function from libigl
        Eigen::MatrixXd holes; // No holes for now
        holes.resize(0, 2);
        
        igl::triangle::triangulate(
            vertices_2d,
            constraint_edges,
            holes,
            flags,
            output_vertices,
            output_faces
        );
    }
    catch (const std::exception& e) {
        // Fallback: try without area constraint
        try {
            flags = "pzDq"; // Remove area constraint
            Eigen::MatrixXd holes;
            holes.resize(0, 2);
            
            igl::triangle::triangulate(
                vertices_2d,
                constraint_edges,
                holes,
                flags,
                output_vertices,
                output_faces
            );
        }
        catch (const std::exception& e2) {
            // Final fallback: simple Delaunay without constraints
            try {
                flags = "zD";
                Eigen::MatrixXi empty_edges;
                empty_edges.resize(0, 2);
                Eigen::MatrixXd holes;
                holes.resize(0, 2);
                
                igl::triangle::triangulate(
                    vertices_2d,
                    empty_edges,
                    holes,
                    flags,
                    output_vertices,
                    output_faces
                );
            }
            catch (const std::exception& e3) {
                // Return empty mesh if all triangulation attempts fail
                return Geometry::CreateMesh();
            }
        }
    }

    // Create output geometry
    Geometry ret = Geometry::CreateMesh();
    auto ret_mesh = ret.get_component<MeshComponent>();
    auto ret_view = ret_mesh->get_igl_view();

    // Convert 2D vertices back to 3D (set Z = 0)
    Eigen::MatrixXd output_vertices_3d(output_vertices.rows(), 3);
    output_vertices_3d.leftCols(2) = output_vertices;
    output_vertices_3d.col(2).setZero();

    // Set the triangulated mesh
    ret_view.set_vertices(output_vertices_3d);
    ret_view.set_faces(output_faces);

    return ret;
}
}  // namespace geom_algorithm
USTC_CG_NAMESPACE_CLOSE_SCOPE
