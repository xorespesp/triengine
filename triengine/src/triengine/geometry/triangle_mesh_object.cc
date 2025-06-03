#include "triangle_mesh_object.hh"

#include <triengine/utility/debug_utils.hh>
#include <triengine/utility/hash_utils.hh>
#include <unordered_map>

namespace triengine::geometry
{
    triangle_mesh_object& triangle_mesh_object::remove_duplicated_vertices()
    {
        const bool has_vertex_normals = this->has_triangle_normals();
        const bool has_vertex_colors = this->has_triangle_colors();
        const bool has_vertex_uvs = this->has_triangle_uvs();

        if (has_vertex_uvs) {
            TRIENGINE_WARN(
                "%s : The mesh #%llX contains triangle uvs that are not handled in this operation"
                , __func__
                , this->get_id()
            );
        }

        const size_t old_vertices_size = vertex_positions.size();

        std::unordered_map<
            Eigen::Vector3f/*point*/,
            size_t/*old index*/,
            utility::hash_eigen<Eigen::Vector3f>
        > point_2_old_idx_map;

        std::vector<size_t> old_idx_2_new_idx_map; // index: old index, value: new index
        old_idx_2_new_idx_map.resize(old_vertices_size, std::numeric_limits<size_t>::max()); 

        size_t new_idx = 0;
        for (size_t old_idx = 0; old_idx < old_vertices_size; ++old_idx)
        {
            const auto [
                it, 
                inserted
            ] = point_2_old_idx_map.insert({ vertex_positions[old_idx], old_idx });

            if (inserted)
            {
                vertex_positions[new_idx] = vertex_positions[old_idx];
                if (has_vertex_normals) { vertex_normals[new_idx] = vertex_normals[old_idx]; }
                if (has_vertex_colors) { vertex_colors[new_idx] = vertex_colors[old_idx]; }

                old_idx_2_new_idx_map[old_idx] = new_idx;
                ++new_idx;
            }
            else
            {
                old_idx_2_new_idx_map[old_idx] = old_idx_2_new_idx_map[it->second];
            }
        } // for

        const size_t new_vertices_size = new_idx;

        vertex_positions.resize(new_vertices_size);
        if (has_vertex_normals) { vertex_normals.resize(new_vertices_size); }
        if (has_vertex_colors) { vertex_colors.resize(new_vertices_size); }
        if (new_vertices_size < old_vertices_size) {
            for (auto& triangle_indice : triangle_indices) {
                triangle_indice(0) = static_cast<int32_t>(old_idx_2_new_idx_map[triangle_indice(0)]);
                triangle_indice(1) = static_cast<int32_t>(old_idx_2_new_idx_map[triangle_indice(1)]);
                triangle_indice(2) = static_cast<int32_t>(old_idx_2_new_idx_map[triangle_indice(2)]);
            }
        }

        TRIENGINE_DEBUG("%s() : %lld vertices have been removed"
            , __func__
            , static_cast<int64_t>(old_vertices_size) - static_cast<int64_t>(new_vertices_size)
        );

        return *this;
    }

    triangle_mesh_object& triangle_mesh_object::remove_unreferenced_vertices()
    {
        const bool has_vertex_normals = this->has_triangle_normals();
        const bool has_vertex_colors = this->has_triangle_colors();
        const bool has_vertex_uvs = this->has_triangle_uvs();

        if (has_vertex_uvs) {
            TRIENGINE_WARN(
                "%s : The mesh #%llX contains triangle uvs that are not handled in this operation"
                , __func__
                , this->get_id()
            );
        }

        const size_t old_vertices_size = vertex_positions.size();

        std::vector<bool> vertices_ref_map; // index: vertex index, value: ref flag
        vertices_ref_map.resize(old_vertices_size, false);
        for (const auto& triangle_indice : triangle_indices) {
            vertices_ref_map[triangle_indice(0)] = true;
            vertices_ref_map[triangle_indice(1)] = true;
            vertices_ref_map[triangle_indice(2)] = true;
        }

        std::vector<size_t> old_idx_2_new_idx_map; // index: old index, value: new index
        old_idx_2_new_idx_map.resize(old_vertices_size, std::numeric_limits<size_t>::max());

        size_t new_idx = 0;
        for (size_t old_idx = 0; old_idx < old_vertices_size; ++old_idx)
        {
            const bool is_referenced = vertices_ref_map[old_idx];
            if (is_referenced) {
                vertex_positions[new_idx] = vertex_positions[old_idx];
                if (has_vertex_normals) { vertex_normals[new_idx] = vertex_normals[old_idx]; }
                if (has_vertex_colors) { vertex_colors[new_idx] = vertex_colors[old_idx]; }

                old_idx_2_new_idx_map[old_idx] = new_idx;
                ++new_idx;
            }
        }

        const size_t new_vertices_size = new_idx;

        vertex_positions.resize(new_vertices_size);
        if (has_vertex_normals) { vertex_normals.resize(new_vertices_size); }
        if (has_vertex_colors) { vertex_colors.resize(new_vertices_size); }
        if (new_vertices_size < old_vertices_size) {
            for (auto& triangle_indice : triangle_indices) {
                triangle_indice(0) = static_cast<int32_t>(old_idx_2_new_idx_map[triangle_indice(0)]);
                triangle_indice(1) = static_cast<int32_t>(old_idx_2_new_idx_map[triangle_indice(1)]);
                triangle_indice(2) = static_cast<int32_t>(old_idx_2_new_idx_map[triangle_indice(2)]);
            }
        }

        TRIENGINE_DEBUG("%s() : %lld vertices have been removed"
            , __func__
            , static_cast<int64_t>(old_vertices_size) - static_cast<int64_t>(new_vertices_size)
        );

        return *this;
    }

    triangle_mesh_object& triangle_mesh_object::normalize_vertex_normals()
    {
        using scalar_type = decltype(vertex_normals)::value_type::Scalar;
        
        for (auto& vn : vertex_normals) {
            if (vn.norm()/* length */ > std::numeric_limits<scalar_type>::epsilon()) {
                vn.normalize();
            } else {
                vn = vec3_f32::UnitZ(); // 벡터 길이가 0일 경우, 임의의 normal(e.g: [0,0,1])로 설정
            }
        }

        return *this;
    }

    triangle_mesh_object& triangle_mesh_object::compute_vertex_normals(const bool smooth_shading)
    {
        /// TODO: improve this

        // Refs:
        // https://slideplayer.com/slide/8344016/
        // https://github.com/isl-org/Open3D/blob/db00e339c1645440dea6951c2971ffa759934112/cpp/open3d/geometry/TriangleMesh.cpp#L116
        // https://github.com/isl-org/Open3D/blob/db00e339c1645440dea6951c2971ffa759934112/cpp/open3d/geometry/TriangleMesh.cpp#L131

        if (!smooth_shading)
        {
            /// for flat shading
            vertex_normals.resize(vertex_positions.size());
            for (const auto& triangle_indice : triangle_indices)
            {
                const vec3_f32
                    & p0 = vertex_positions[triangle_indice(0)],
                    & p1 = vertex_positions[triangle_indice(1)],
                    & p2 = vertex_positions[triangle_indice(2)];

                const vec3_f32
                    normal = /*vec1*/(p1 - p0).cross(/*vec2*/p2 - p0).normalized();

                vertex_normals[triangle_indice(0)] = normal;
                vertex_normals[triangle_indice(1)] = normal;
                vertex_normals[triangle_indice(2)] = normal;
            }
        }
        else
        {
            /// for smooth shading
            vertex_normals.assign(vertex_positions.size(), math::vec3_all(0.0f));

            for (const auto& triangle : triangle_indices)
            {
                const vec3_f32
                    & p0 = vertex_positions[triangle(0)],
                    & p1 = vertex_positions[triangle(1)],
                    & p2 = vertex_positions[triangle(2)];

                // 각 삼각형에 대해 face normal 계산
                const vec3_f32
                    face_normal = (p1 - p0).cross(p2 - p0);

                // 계산된 face normal을 해당 face가 포함하고 있는 3개의 vertex normal에 누적한다.
                vertex_normals[triangle(0)] += face_normal;
                vertex_normals[triangle(1)] += face_normal;
                vertex_normals[triangle(2)] += face_normal;
            }

            // vertex normal 정규화
            this->normalize_vertex_normals();
        }

        return *this;
    }

    triangle_mesh_object& triangle_mesh_object::operator+=(const triangle_mesh_object& rhs)
    {
        // Ref: https://github.com/isl-org/Open3D/blob/db00e339c1645440dea6951c2971ffa759934112/cpp/open3d/geometry/TriangleMesh.cpp#L53

        if (this->get_shading_mode() != rhs.get_shading_mode() || 
            rhs.get_shading_mode() != shading_mode::vertex)
        {
            TRIENGINE_PANIC("Invalid operation");
        }

        const size_t new_positions_offset = vertex_positions.size();

        // Concat vertex positions
        {
            vertex_positions.reserve(vertex_positions.size() + rhs.vertex_positions.size());
            vertex_positions.insert(vertex_positions.end(), rhs.vertex_positions.begin(), rhs.vertex_positions.end());
            for (size_t i = new_positions_offset; i < new_positions_offset + rhs.vertex_positions.size(); ++i) {
                vec3_f32& new_position = vertex_positions[i];
                new_position = (rhs.get_model() * vec4_f32(new_position.x(), new_position.y(), new_position.z(), 1.0f)).block<3, 1>(0, 0);
            }
        }

        // Concat vertex normals
        if (this->has_triangle_normals() && rhs.has_triangle_normals())
        {
            const size_t new_normals_offset = vertex_normals.size();
            vertex_normals.reserve(vertex_normals.size() + rhs.vertex_normals.size());
            vertex_normals.insert(vertex_normals.end(), rhs.vertex_normals.begin(), rhs.vertex_normals.end());
            // Calculate normal matrix for normal vector transformation
            // In case of non-uniform scaling, simply applying the model matrix does not correctly transform the normal vector.
            const mat3_f32 rhs_normal_matrix = mat3_f32(rhs.get_model().block<3, 3>(0, 0)).inverse().transpose();
            for (size_t i = new_normals_offset; i < new_normals_offset + rhs.vertex_normals.size(); ++i) {
                vec3_f32& new_normal = vertex_normals[i];
                new_normal = (rhs_normal_matrix * new_normal).normalized();
            }
        }
        else
        {
            vertex_normals.clear();
        }

        // Concat vertex colors
        {
            vertex_colors.reserve(vertex_colors.size() + rhs.vertex_colors.size());
            vertex_colors.insert(vertex_colors.end(), rhs.vertex_colors.begin(), rhs.vertex_colors.end());
        }

        // Concat vertex indices
        {
            const size_t new_indices_offset = triangle_indices.size();
            triangle_indices.reserve(triangle_indices.size() + rhs.triangle_indices.size());
            triangle_indices.insert(triangle_indices.end(), rhs.triangle_indices.begin(), rhs.triangle_indices.end());
            for (size_t i = new_indices_offset; i < new_indices_offset + rhs.triangle_indices.size(); ++i) {
                auto& new_index = triangle_indices[i];
                new_index.x() += static_cast<int>(new_positions_offset);
                new_index.y() += static_cast<int>(new_positions_offset);
                new_index.z() += static_cast<int>(new_positions_offset);
            }
        }

        return *this;
    }

} // namespace