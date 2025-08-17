#include <GCore/Components/MeshViews.h>

#include <algorithm>

#include "GCore/Components/MeshComponent.h"

USTC_CG_NAMESPACE_OPEN_SCOPE

// ConstMeshIGLView Implementation
ConstMeshIGLView::ConstMeshIGLView(const MeshComponent& mesh) : mesh_(mesh)
{
}

Eigen::MatrixXd ConstMeshIGLView::get_vertices() const
{
    return vec3f_array_to_matrix(mesh_.get_vertices());
}

Eigen::MatrixXi ConstMeshIGLView::get_faces() const
{
    auto face_vertex_counts = mesh_.get_face_vertex_counts();
    auto face_vertex_indices = mesh_.get_face_vertex_indices();

    // Count triangular faces only
    int triangle_count = 0;
    for (int count : face_vertex_counts) {
        if (count == 3)
            triangle_count++;
    }

    Eigen::MatrixXi faces(triangle_count, 3);
    int face_idx = 0;
    int vertex_idx = 0;

    for (int count : face_vertex_counts) {
        if (count == 3) {
            faces(face_idx, 0) = face_vertex_indices[vertex_idx];
            faces(face_idx, 1) = face_vertex_indices[vertex_idx + 1];
            faces(face_idx, 2) = face_vertex_indices[vertex_idx + 2];
            face_idx++;
        }
        vertex_idx += count;
    }

    return faces;
}

Eigen::VectorXi ConstMeshIGLView::get_face_vertex_counts() const
{
    return int_array_to_vector(mesh_.get_face_vertex_counts());
}

Eigen::VectorXi ConstMeshIGLView::get_face_vertex_indices() const
{
    return int_array_to_vector(mesh_.get_face_vertex_indices());
}

Eigen::MatrixXd ConstMeshIGLView::get_normals() const
{
    return vec3f_array_to_matrix(mesh_.get_normals());
}

Eigen::MatrixXd ConstMeshIGLView::get_uv_coordinates() const
{
    return vec2f_array_to_matrix(mesh_.get_texcoords_array());
}

Eigen::MatrixXd ConstMeshIGLView::get_display_colors() const
{
    return vec3f_array_to_matrix(mesh_.get_display_color());
}

Eigen::VectorXd ConstMeshIGLView::get_vertex_scalar_quantity(
    const std::string& name) const
{
    return float_array_to_vector(mesh_.get_vertex_scalar_quantity(name));
}

Eigen::VectorXd ConstMeshIGLView::get_face_scalar_quantity(
    const std::string& name) const
{
    return float_array_to_vector(mesh_.get_face_scalar_quantity(name));
}

Eigen::MatrixXd ConstMeshIGLView::get_vertex_vector_quantity(
    const std::string& name) const
{
    return vec3f_array_to_matrix(mesh_.get_vertex_vector_quantity(name));
}

Eigen::MatrixXd ConstMeshIGLView::get_face_vector_quantity(
    const std::string& name) const
{
    return vec3f_array_to_matrix(mesh_.get_face_vector_quantity(name));
}

Eigen::MatrixXd ConstMeshIGLView::get_vertex_color_quantity(
    const std::string& name) const
{
    return vec3f_array_to_matrix(mesh_.get_vertex_color_quantity(name));
}

Eigen::MatrixXd ConstMeshIGLView::get_face_color_quantity(
    const std::string& name) const
{
    return vec3f_array_to_matrix(mesh_.get_face_color_quantity(name));
}

Eigen::MatrixXd ConstMeshIGLView::get_vertex_parameterization_quantity(
    const std::string& name) const
{
    return vec2f_array_to_matrix(
        mesh_.get_vertex_parameterization_quantity(name));
}

Eigen::MatrixXd ConstMeshIGLView::get_face_corner_parameterization_quantity(
    const std::string& name) const
{
    return vec2f_array_to_matrix(
        mesh_.get_face_corner_parameterization_quantity(name));
}

// Utility functions for ConstMeshIGLView
Eigen::MatrixXd ConstMeshIGLView::vec3f_array_to_matrix(
    const pxr::VtArray<pxr::GfVec3f>& array)
{
    Eigen::MatrixXd matrix(array.size(), 3);
    for (size_t i = 0; i < array.size(); ++i) {
        matrix(i, 0) = array[i][0];
        matrix(i, 1) = array[i][1];
        matrix(i, 2) = array[i][2];
    }
    return matrix;
}

Eigen::MatrixXd ConstMeshIGLView::vec2f_array_to_matrix(
    const pxr::VtArray<pxr::GfVec2f>& array)
{
    Eigen::MatrixXd matrix(array.size(), 2);
    for (size_t i = 0; i < array.size(); ++i) {
        matrix(i, 0) = array[i][0];
        matrix(i, 1) = array[i][1];
    }
    return matrix;
}

Eigen::VectorXd ConstMeshIGLView::float_array_to_vector(
    const pxr::VtArray<float>& array)
{
    Eigen::VectorXd vector(array.size());
    std::memcpy(vector.data(), array.data(), array.size() * sizeof(float));
    return vector;
}

Eigen::VectorXi ConstMeshIGLView::int_array_to_vector(
    const pxr::VtArray<int>& array)
{
    Eigen::VectorXi vector(array.size());
    std::memcpy(vector.data(), array.data(), array.size() * sizeof(int));
    return vector;
}

// MeshIGLView Implementation
MeshIGLView::MeshIGLView(MeshComponent& mesh)
    : ConstMeshIGLView(mesh),
      mutable_mesh_(mesh)
{
}

void MeshIGLView::set_vertices(const Eigen::MatrixXd& vertices)
{
    mutable_mesh_.set_vertices(matrix_to_vec3f_array(vertices));
}

void MeshIGLView::set_faces(const Eigen::MatrixXi& faces)
{
    // Convert triangular faces to face vertex counts and indices
    pxr::VtArray<int> face_vertex_counts(faces.rows(), 3);
    pxr::VtArray<int> face_vertex_indices(faces.rows() * 3);

    for (int i = 0; i < faces.rows(); ++i) {
        face_vertex_indices[i * 3] = faces(i, 0);
        face_vertex_indices[i * 3 + 1] = faces(i, 1);
        face_vertex_indices[i * 3 + 2] = faces(i, 2);
    }

    mutable_mesh_.set_face_vertex_counts(face_vertex_counts);
    mutable_mesh_.set_face_vertex_indices(face_vertex_indices);
}

void MeshIGLView::set_face_topology(
    const Eigen::VectorXi& face_vertex_counts,
    const Eigen::VectorXi& face_vertex_indices)
{
    mutable_mesh_.set_face_vertex_counts(
        vector_to_int_array(face_vertex_counts));
    mutable_mesh_.set_face_vertex_indices(
        vector_to_int_array(face_vertex_indices));
}

void MeshIGLView::set_normals(const Eigen::MatrixXd& normals)
{
    mutable_mesh_.set_normals(matrix_to_vec3f_array(normals));
}

void MeshIGLView::set_uv_coordinates(const Eigen::MatrixXd& uv_coords)
{
    mutable_mesh_.set_texcoords_array(matrix_to_vec2f_array(uv_coords));
}

void MeshIGLView::set_display_colors(const Eigen::MatrixXd& colors)
{
    mutable_mesh_.set_display_color(matrix_to_vec3f_array(colors));
}

void MeshIGLView::set_vertex_scalar_quantity(
    const std::string& name,
    const Eigen::VectorXd& values)
{
    mutable_mesh_.add_vertex_scalar_quantity(
        name, vector_to_float_array(values));
}

void MeshIGLView::set_face_scalar_quantity(
    const std::string& name,
    const Eigen::VectorXd& values)
{
    mutable_mesh_.add_face_scalar_quantity(name, vector_to_float_array(values));
}

void MeshIGLView::set_vertex_vector_quantity(
    const std::string& name,
    const Eigen::MatrixXd& vectors)
{
    mutable_mesh_.add_vertex_vector_quantity(
        name, matrix_to_vec3f_array(vectors));
}

void MeshIGLView::set_face_vector_quantity(
    const std::string& name,
    const Eigen::MatrixXd& vectors)
{
    mutable_mesh_.add_face_vector_quantity(
        name, matrix_to_vec3f_array(vectors));
}

void MeshIGLView::set_vertex_color_quantity(
    const std::string& name,
    const Eigen::MatrixXd& colors)
{
    mutable_mesh_.add_vertex_color_quantity(
        name, matrix_to_vec3f_array(colors));
}

void MeshIGLView::set_face_color_quantity(
    const std::string& name,
    const Eigen::MatrixXd& colors)
{
    mutable_mesh_.add_face_color_quantity(name, matrix_to_vec3f_array(colors));
}

void MeshIGLView::set_vertex_parameterization_quantity(
    const std::string& name,
    const Eigen::MatrixXd& params)
{
    mutable_mesh_.add_vertex_parameterization_quantity(
        name, matrix_to_vec2f_array(params));
}

void MeshIGLView::set_face_corner_parameterization_quantity(
    const std::string& name,
    const Eigen::MatrixXd& params)
{
    mutable_mesh_.add_face_corner_parameterization_quantity(
        name, matrix_to_vec2f_array(params));
}

// Utility functions for MeshIGLView
pxr::VtArray<pxr::GfVec3f> MeshIGLView::matrix_to_vec3f_array(
    const Eigen::MatrixXd& matrix)
{
    pxr::VtArray<pxr::GfVec3f> array(matrix.rows());
    for (int i = 0; i < matrix.rows(); ++i) {
        array[i] = pxr::GfVec3f(matrix(i, 0), matrix(i, 1), matrix(i, 2));
    }
    return array;
}

pxr::VtArray<pxr::GfVec2f> MeshIGLView::matrix_to_vec2f_array(
    const Eigen::MatrixXd& matrix)
{
    pxr::VtArray<pxr::GfVec2f> array(matrix.rows());
    for (int i = 0; i < matrix.rows(); ++i) {
        array[i] = pxr::GfVec2f(matrix(i, 0), matrix(i, 1));
    }
    return array;
}

pxr::VtArray<float> MeshIGLView::vector_to_float_array(
    const Eigen::VectorXd& vector)
{
    pxr::VtArray<float> array(vector.size());
    for (int i = 0; i < vector.size(); ++i) {
        array[i] = static_cast<float>(vector[i]);
    }
    return array;
}

pxr::VtArray<int> MeshIGLView::vector_to_int_array(
    const Eigen::VectorXi& vector)
{
    pxr::VtArray<int> array(vector.size());
    std::memcpy(array.data(), vector.data(), vector.size() * sizeof(int));
    return array;
}

USTC_CG_NAMESPACE_CLOSE_SCOPE
