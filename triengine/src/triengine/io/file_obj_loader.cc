#include "file_obj_loader.hh"
#include <triengine/utility/debug_utils.hh>
#include <triengine/extern/tiny_obj_loader.h>

#include <algorithm>
#include <numeric>

namespace triengine::io
{
    bool load_obj_file(
        const std::filesystem::path& file_path,
        geometry::triangle_mesh_object& mesh /* out */)
    {
        TRIENGINE_TRACE("Load obj file: %s", file_path.string().c_str());

        const std::filesystem::path mtl_base_dir = file_path.parent_path().string(); // Path to material files

        tinyobj::ObjReaderConfig tinyobj_reader_config{};
        tinyobj_reader_config.triangulate = true;
        tinyobj_reader_config.vertex_color = true;
        tinyobj_reader_config.mtl_search_path = mtl_base_dir.string();

        tinyobj::ObjReader tinyobj_reader;
        const bool succeeded = tinyobj_reader.ParseFromFile(file_path.string(), tinyobj_reader_config);

        if (!tinyobj_reader.Error().empty()) {
            TRIENGINE_TRACE("Load obj error: %s", tinyobj_reader.Error().c_str());
        }

        if (!tinyobj_reader.Warning().empty()) {
            TRIENGINE_TRACE("Load obj warning: %s", tinyobj_reader.Warning().c_str());
        }

        if (!succeeded) {
            return false;
        }

        mesh.clear();

        const tinyobj::attrib_t& tinyobj_attrib = tinyobj_reader.GetAttrib();
        const std::vector<tinyobj::shape_t>& tinyobj_shapes = tinyobj_reader.GetShapes();
        const std::vector<tinyobj::material_t>& tinyobj_materials = tinyobj_reader.GetMaterials();

        std::vector<triengine::vec3_f32> vertex_positions;
        std::vector<triengine::vec3_f32> vertex_normals;
        std::vector<triengine::vec2_f32> vertex_uvs;
        std::vector<triengine::color3_f32> vertex_colors; // Optional

        /**
         * 실제 OBJ 모델에서는 "하나의 vertex position에 여러 개의 UV 좌표 세트가 할당되는 경우"가 있을 수 있다.
         * 이는 OBJ 파일 내에서, 각 face(vertex)의 요소는 위치(vertex position), 노멀(vertex normal), 텍스처 좌표(vertex uv)에 대한 서로 다른 인덱스를 가지기 때문이다.
         * 
         * 예를 들어 "하나의 vertex position"이 여러 개의 면(face)에서 동시에 사용될 때, 
         * 해당 vertex position에 대응되는 텍스처 uv 좌표나 normal 인덱스가 매핑된 면에 따라 각각 달라질 수가 있다.
         * (다른 face에서 같은 정점 위치를 참조하면서, 해당 위치에 대해 서로 다른 UV를 지정하는 케이스가 있을 수도 있다는 얘기)
         *
         * 때문에, 이를 고려하지 않고 단순히 "반드시 하나의 정점당 하나의 UV 좌표가 매핑된다"고 가정해버릴 경우,
         * 모델의 가장자리나 텍스처 경계 부근에서 UV가 어긋나는 문제가 발생한다.
         * 이 문제가 발생하지 않도록 하기 위해서는, "정점"에 대한 개념을 단순히 "기하학적 위치(vertex position)"로만 정의해서는 안 되며,
         * "위치(vertex position), 노멀(normal), UV(텍스처 좌표)" 이 3가지 속성들을 하나로 묶은 "튜플 세트"를 "하나의 고유한 버텍스"로 간주해야 한다.
         * 간단히 말해, face를 순회하면서 position, normal, uv 인덱스의 조합으로 새로운 실제 정점을 만들어내고, 
         * 이미 동일한 (position, normal, uv) 조합의 정점이 생성되었는지 확인한 뒤, 중복이면 재사용하고 아니면 새로 추가하는 방식으로 로딩해야 한다는 얘기다.
         * 이렇게 해야 하나의 기하학적 위치를 공유하더라도 서로 다른 UV 좌표를 올바르게 표현할 수 있고, 텍스처 맵핑시 엣지에 UV가 깨지는 문제를 방지할 수 있다.
         * 
         * 그리고 이러한 (position, normal, uv) 조합은 tinyobjloader 라이브러리에서는 `tinyobj::index_t` 오브젝트로 표현된다.
         */

        struct tinyobj_index_hasher {
            std::size_t operator()(const tinyobj::index_t& k) const {
                static_assert(sizeof(tinyobj::index_t) == 12, "!!");
                std::size_t seed{};
                std::hash<int> hasher;
                seed ^= hasher(k.vertex_index) + 0x9e3779b97f4a7c16ULL + (seed << 6) + (seed >> 2);
                seed ^= hasher(k.normal_index) + 0x9e3779b97f4a7c16ULL + (seed << 6) + (seed >> 2);
                seed ^= hasher(k.texcoord_index) + 0x9e3779b97f4a7c16ULL + (seed << 6) + (seed >> 2);
                return seed;
            }
        };

        struct tinyobj_index_equal {
            bool operator()(const tinyobj::index_t& a, const tinyobj::index_t& b) const noexcept {
                return
                    a.vertex_index == b.vertex_index &&
                    a.normal_index == b.normal_index &&
                    a.texcoord_index == b.texcoord_index;
            }
        };

        std::unordered_map<
            tinyobj::index_t/* key: (position, normal, uv) */, 
            size_t/* value: vertex index */, 
            tinyobj_index_hasher/* key_hasher */,
            tinyobj_index_equal/* key_equ*/
        > vertex_index_map;

        const auto process_new_vertex =
            [&](const tinyobj::index_t& tinyobj_idx) -> size_t
            {
                auto [map_insert_it, map_insert_succeeded] = vertex_index_map.insert(
                    { tinyobj_idx, static_cast<size_t>(-1)/*npos*/ }
                );

                if (!map_insert_succeeded) {
                    return map_insert_it->second;
                }

                // Vertex Position
                Eigen::Vector3f curr_vertex_position{
                    tinyobj_attrib.vertices[3 * size_t(tinyobj_idx.vertex_index) + 0],
                    tinyobj_attrib.vertices[3 * size_t(tinyobj_idx.vertex_index) + 1],
                    tinyobj_attrib.vertices[3 * size_t(tinyobj_idx.vertex_index) + 2]
                };

                // Vertex Normal
                Eigen::Vector3f curr_vertex_normal{ Eigen::Vector3f::Zero() };
                if (const bool has_normal_data = tinyobj_idx.normal_index >= 0; // Check if `normal_index` is zero or positive. negative = no normal data
                    has_normal_data) {
                    curr_vertex_normal(0/*x*/) = tinyobj_attrib.normals[3 * size_t(tinyobj_idx.normal_index) + 0];
                    curr_vertex_normal(1/*y*/) = tinyobj_attrib.normals[3 * size_t(tinyobj_idx.normal_index) + 1];
                    curr_vertex_normal(2/*z*/) = tinyobj_attrib.normals[3 * size_t(tinyobj_idx.normal_index) + 2];
                }

                // Vertex Texture UV
                Eigen::Vector2f curr_vertex_uv{ Eigen::Vector2f::Zero() };
                if (const bool has_uv_data = tinyobj_idx.texcoord_index >= 0; // Check if `texcoord_index` is zero or positive. negative = no texcoord data
                    has_uv_data) {
                    curr_vertex_uv(0/*u*/) = tinyobj_attrib.texcoords[2 * size_t(tinyobj_idx.texcoord_index) + 0];
                    curr_vertex_uv(1/*v*/) = tinyobj_attrib.texcoords[2 * size_t(tinyobj_idx.texcoord_index) + 1];
                }

                const size_t curr_vertex_idx = vertex_positions.size();
                map_insert_it->second = curr_vertex_idx; // update map
                vertex_positions.push_back(curr_vertex_position);
                vertex_normals.push_back(curr_vertex_normal);
                vertex_uvs.push_back(curr_vertex_uv);
                return curr_vertex_idx;
            };

        // Loop over shapes
        for (size_t s = 0/* shape idx */; s < tinyobj_shapes.size(); ++s)
        {
            // Loop over faces(triangles)
            size_t index_offset = 0;
            for (size_t f = 0/* face(triangle) idx */; f < tinyobj_shapes[s].mesh.num_face_vertices.size(); ++f)
            {
                const size_t num_face_vertices = static_cast<size_t>(tinyobj_shapes[s].mesh.num_face_vertices[f]);
                if (num_face_vertices != 3) {
                    TRIENGINE_TRACE("Unexpected face polygon type. (non-triangular face found, fv=%zu)", num_face_vertices);
                    return false;
                }

                // Loop over vertices in the face.
                Eigen::Vector3i face_idx; // face(triangle) index
                for (size_t v = 0/* face(triangle) vertex idx */; v < num_face_vertices; ++v)
                {
                    // access to face(triangle) vertex
                    const tinyobj::index_t& tinyobj_idx = tinyobj_shapes[s].mesh.indices[index_offset + v];

                    const size_t curr_vertex_idx = process_new_vertex(tinyobj_idx);
                    face_idx(v) = static_cast<int>(curr_vertex_idx);
                } // for

                mesh.triangle_indices.push_back(face_idx);

                // TODO: support matterial id system
                // per-face material
                //mesh.triangle_material_ids_.push_back(
                //    obj_shapes[s].mesh.material_ids[f]
                //);

                index_offset += num_face_vertices;
            } // for
        } // for

        // Vertex colors (optional)
        for (size_t vidx = 0; vidx < tinyobj_attrib.colors.size(); vidx += 3) {
            TRIENGINE_ASSERT(vidx + 2 < tinyobj_attrib.colors.size());
            const tinyobj::real_t
                r = tinyobj_attrib.colors[vidx + 0],
                g = tinyobj_attrib.colors[vidx + 1],
                b = tinyobj_attrib.colors[vidx + 2];
            vertex_colors.emplace_back(r, g, b);
        }

        if (!vertex_colors.empty() && vertex_colors.size() != vertex_positions.size()) {
            vertex_colors.clear();
        }

        // 이제 positions, normals, uvs, colors를 mesh에 옮긴다.
        {
            mesh.vertex_positions = std::move(vertex_positions);

            // if not all normals have been set, then remove the vertex normals
            if (const bool has_normals = std::all_of(
                vertex_normals.begin(), vertex_normals.end(),
                [](auto& n) { return n.squaredNorm() > 0.f; });
                has_normals)
            {
                mesh.vertex_normals = std::move(vertex_normals);
            }
            else
            {
                TRIENGINE_TRACE("Computing normals..");
                mesh.compute_vertex_normals(true);
            }

            // if not all triangles have corresponding uvs, then remove uvs
            if (const bool has_uvs = std::all_of(
                vertex_uvs.begin(), vertex_uvs.end(),
                [](auto& t) {return t.squaredNorm() > 0.f; });
                has_uvs)
            {
                mesh.vertex_uvs = std::move(vertex_uvs);
            }

            mesh.vertex_colors = std::move(vertex_colors);
        }

        if (!tinyobj_materials.empty())
        {
            mesh.set_shading_mode(geometry::triangle_mesh_object::shading_mode::texture);

            auto* const curr_mesh_material = mesh.get_texture_shading_material();
            for (size_t i = 0; i < tinyobj_materials.size(); ++i)
            {
                const tinyobj::material_t& tinyobj_material = tinyobj_materials[i];

                TRIENGINE_TRACE(
                    "\ndiffuse_texname: %s"
                    "\nspecular_texname: %s"
                    , tinyobj_material.diffuse_texname.c_str()
                    , tinyobj_material.specular_texname.c_str()
                );

                if (!tinyobj_material.diffuse_texname.empty()) {
                    curr_mesh_material->diffuse_map.create_from_file(mtl_base_dir / tinyobj_material.diffuse_texname);
                }

                if (!tinyobj_material.specular_texname.empty()) {
                    curr_mesh_material->specular_map.create_from_file(mtl_base_dir / tinyobj_material.specular_texname);
                }
            } // for

            // if the obj file does not have a diffuse map, just create randomly
            if (!curr_mesh_material->diffuse_map.is_valid()) {
                curr_mesh_material->diffuse_map.create_from_uniform_color(triengine::color3_f32::all(1.0f));
            }

            // if the obj file does not have a specular map, just create randomly
            if (!curr_mesh_material->specular_map.is_valid()) {
                curr_mesh_material->specular_map.create_from_uniform_color(triengine::color3_f32::all(1.0f));
            }
        }

        TRIENGINE_TRACE("loaded vertex_positions size: %lu", mesh.vertex_positions.size());
        TRIENGINE_TRACE("loaded vertex_normals size: %lu", mesh.vertex_normals.size());
        TRIENGINE_TRACE("loaded vertex_colors size: %lu", mesh.vertex_colors.size());
        TRIENGINE_TRACE("loaded triangle_indices size: %lu", mesh.triangle_indices.size());
        TRIENGINE_TRACE("loaded vertex_uvs size: %lu", mesh.vertex_uvs.size());
        return true;
    }

} // namespace