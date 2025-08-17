#pragma once
#include <GCore/api.h>
#include <pxr/base/gf/vec2f.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/vt/array.h>

#include <Eigen/Eigen>
#include <string>

USTC_CG_NAMESPACE_OPEN_SCOPE
struct MeshComponent;

struct GEOMETRY_API ConstMeshIGLView {
    explicit ConstMeshIGLView(const MeshComponent& mesh);

    // Vertex positions as Nx3 matrix
    Eigen::MatrixXd get_vertices() const;

    // Face indices as Fx3 matrix (assuming triangular faces)
    Eigen::MatrixXi get_faces() const;

    // Face vertex counts
    Eigen::VectorXi get_face_vertex_counts() const;

    // Face vertex indices (for general polygons)
    Eigen::VectorXi get_face_vertex_indices() const;

    // Normals as Nx3 matrix
    Eigen::MatrixXd get_normals() const;

    // UV coordinates as Nx2 matrix
    Eigen::MatrixXd get_uv_coordinates() const;

    // Display colors as Nx3 matrix
    Eigen::MatrixXd get_display_colors() const;

    // Get scalar quantities
    Eigen::VectorXd get_vertex_scalar_quantity(const std::string& name) const;
    Eigen::VectorXd get_face_scalar_quantity(const std::string& name) const;

    // Get vector quantities
    Eigen::MatrixXd get_vertex_vector_quantity(const std::string& name) const;
    Eigen::MatrixXd get_face_vector_quantity(const std::string& name) const;

    // Get color quantities
    Eigen::MatrixXd get_vertex_color_quantity(const std::string& name) const;
    Eigen::MatrixXd get_face_color_quantity(const std::string& name) const;

    // Get parameterization quantities
    Eigen::MatrixXd get_vertex_parameterization_quantity(
        const std::string& name) const;
    Eigen::MatrixXd get_face_corner_parameterization_quantity(
        const std::string& name) const;

   protected:
    const MeshComponent& mesh_;

    // Utility functions for conversion
    static Eigen::MatrixXd vec3f_array_to_matrix(
        const pxr::VtArray<pxr::GfVec3f>& array);
    static Eigen::MatrixXd vec2f_array_to_matrix(
        const pxr::VtArray<pxr::GfVec2f>& array);
    static Eigen::VectorXd float_array_to_vector(
        const pxr::VtArray<float>& array);
    static Eigen::VectorXi int_array_to_vector(const pxr::VtArray<int>& array);
};

struct GEOMETRY_API MeshIGLView : public ConstMeshIGLView {
    MeshIGLView(MeshComponent& mesh);

    // Set vertex positions from Nx3 matrix
    void set_vertices(const Eigen::MatrixXd& vertices);

    // Set face indices from Fx3 matrix (for triangular meshes)
    void set_faces(const Eigen::MatrixXi& faces);

    // Set face vertex counts and indices (for general polygons)
    void set_face_topology(
        const Eigen::VectorXi& face_vertex_counts,
        const Eigen::VectorXi& face_vertex_indices);

    // Set normals from Nx3 matrix
    void set_normals(const Eigen::MatrixXd& normals);

    // Set UV coordinates from Nx2 matrix
    void set_uv_coordinates(const Eigen::MatrixXd& uv_coords);

    // Set display colors from Nx3 matrix
    void set_display_colors(const Eigen::MatrixXd& colors);

    // Set scalar quantities
    void set_vertex_scalar_quantity(
        const std::string& name,
        const Eigen::VectorXd& values);
    void set_face_scalar_quantity(
        const std::string& name,
        const Eigen::VectorXd& values);

    // Set vector quantities
    void set_vertex_vector_quantity(
        const std::string& name,
        const Eigen::MatrixXd& vectors);
    void set_face_vector_quantity(
        const std::string& name,
        const Eigen::MatrixXd& vectors);

    // Set color quantities
    void set_vertex_color_quantity(
        const std::string& name,
        const Eigen::MatrixXd& colors);
    void set_face_color_quantity(
        const std::string& name,
        const Eigen::MatrixXd& colors);

    // Set parameterization quantities
    void set_vertex_parameterization_quantity(
        const std::string& name,
        const Eigen::MatrixXd& params);
    void set_face_corner_parameterization_quantity(
        const std::string& name,
        const Eigen::MatrixXd& params);

   private:
    MeshComponent& mutable_mesh_;

    // Utility functions for conversion
    static pxr::VtArray<pxr::GfVec3f> matrix_to_vec3f_array(
        const Eigen::MatrixXd& matrix);
    static pxr::VtArray<pxr::GfVec2f> matrix_to_vec2f_array(
        const Eigen::MatrixXd& matrix);
    static pxr::VtArray<float> vector_to_float_array(
        const Eigen::VectorXd& vector);
    static pxr::VtArray<int> vector_to_int_array(const Eigen::VectorXi& vector);
};

USTC_CG_NAMESPACE_CLOSE_SCOPE
