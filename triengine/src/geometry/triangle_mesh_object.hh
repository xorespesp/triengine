#pragma once
#include "geometry_object_base.hh"
#include "../texture.hh"

#include <unordered_map>
#include <vector>
#include <memory>
#include <variant>
#include <utility>

namespace triengine::geometry
{
    // Ref: https://github.com/isl-org/Open3D/blob/db00e339c1645440dea6951c2971ffa759934112/cpp/open3d/geometry/TriangleMesh.h

    class triangle_mesh_object
        : public geometry_object_base
    {
    public:
        enum class shading_mode
        {
            vertex, /// Coloring by linear-interpolation from vertices, requires vertex colors (as rgb)
            texture /// Coloring by texture, requires texture uv coordinates (color information)
        };

        /// NOTE: Only used in vertex-shading mode
        struct vertex_shading_material
        {
            uint16_t shininess{ 128 }; /// surface shininess scalar (must be `> 0`)

            vertex_shading_material() = default;

            bool is_valid() const noexcept {
                return shininess > 0;
            }
        };

        /// NOTE: Only used in texture-shading mode
        struct texture_shading_material
        {
            texture_2d diffuse_map; /// diffuse-map texture
            texture_2d specular_map; /// specular-map texture
            uint16_t shininess{ 128 }; /// surface shininess scalar (must be `> 0`)

            texture_shading_material() = default;

            bool is_valid() const noexcept {
                return
                    diffuse_map.is_valid() &&
                    specular_map.is_valid() &&
                    shininess > 0;
            }
        };

    private:
        /// Shading mode
        shading_mode _shading_mode{ shading_mode::vertex };

        /// Material info
        std::variant<
            vertex_shading_material, // default value
            texture_shading_material
        > _material;

    public:
        std::vector<vec3_f32> vertex_positions; /// List of triangle vertex positions (3 * num_triangles)
        std::vector<vec3_f32> vertex_normals;   /// List of triangle vertex normals (3 * num_triangles)

        /// NOTE: Only used in vertex-shading mode
        std::vector<color3_f32> vertex_colors; /// List of triangle vertex colors (3 * num_triangles)

        /// NOTE: Only used in texture-shading mode
        std::vector<vec2_f32> vertex_uvs; /// List of texture uv coordinates per triangle vertex (3 * num_triangles)

        std::vector<vec3_i32> triangle_indices; /// List of triangles denoted by the index of points forming the triangle (num_triangles)

    public:
        triangle_mesh_object()
            : geometry_object_base{ geometry_object_type::triangle_mesh }
        {
            this->set_shading_mode(_shading_mode);
        }

        triangle_mesh_object(shading_mode mode)
            : geometry_object_base{ geometry_object_type::triangle_mesh }
        {
            this->set_shading_mode(mode);
        }

        shading_mode get_shading_mode() const noexcept {
            return _shading_mode;
        }

        void set_shading_mode(shading_mode mode) noexcept {
            _shading_mode = mode;
            switch (mode) {
            case shading_mode::vertex:
                _material = vertex_shading_material{};
                break;
            case shading_mode::texture:
                _material = texture_shading_material{};
                break;
            default:
                TRIENGINE_ASSERT(false); // not expected
                break;
            }
        }

        /// NOTE: Only used in vertex-shading mode
        const vertex_shading_material* get_vertex_shading_material() const noexcept {
            return std::get_if<vertex_shading_material>(&_material);
        }

        /// NOTE: Only used in vertex-shading mode
        vertex_shading_material* get_vertex_shading_material() noexcept {
            return std::get_if<vertex_shading_material>(&_material);
        }

        /// NOTE: Only used in texture-shading mode
        const texture_shading_material* get_texture_shading_material() const noexcept {
            return std::get_if<texture_shading_material>(&_material);
        }

        /// NOTE: Only used in texture-shading mode
        texture_shading_material* get_texture_shading_material() noexcept {
            return std::get_if<texture_shading_material>(&_material);
        }

        /// Returns `true` if the mesh contains triangles
        bool has_triangles() const {
            return !vertex_positions.empty() && !triangle_indices.empty();
        }

        /// Returns `true` if the mesh contains triangle normals
        bool has_triangle_normals() const {
            return this->has_triangles() && vertex_normals.size() > 0;
        }

        /// Returns `true` if the mesh contains triangle's uv map
        bool has_triangle_uvs() const {
            return this->has_triangles() && vertex_uvs.size() > 0;
        }

        ///// Returns `true` if the mesh has texture
        //bool has_textures() const {
        //    bool is_all_texture_valid = std::accumulate(
        //        textures.begin(), textures.end(), true,
        //        [](bool a, const Image& b) { return a && !b.IsEmpty(); });
        //    return !textures.empty() && is_all_texture_valid;
        //}

        //bool has_materials() const {
        //    return !materials_.empty();
        //}

        /// Normalize vertex normals to length 1.
        void normalize_vertex_normals();

        /// compute vertex normals. (usually called before rendering)
        void compute_vertex_normals(bool smooth_shading = false);

        void paint_uniform_color(const color3_f32& color) {
            vertex_colors.resize(vertex_positions.size(), color);
        }

        void clear() {
            vertex_positions.clear();
            vertex_normals.clear();
            vertex_colors.clear();
            vertex_uvs.clear();
            triangle_indices.clear();
            this->set_shading_mode(_shading_mode); // reset material
        }

        // concat operator
        triangle_mesh_object& operator+=(
            const triangle_mesh_object& rhs
        );

    public:
        static std::shared_ptr<triangle_mesh_object> create_box(
            float width = 1.0f,  /// x-directional length
            float height = 1.0f, /// y-directional length
            float depth = 1.0f   /// z-directional length
        );

        static std::shared_ptr<triangle_mesh_object> create_sphere(
            float radius = 1.0f
        );

        static std::shared_ptr<triangle_mesh_object> create_cylinder(
            float base_radius = 1.0f,
            float height = 1.0f
        );

        static std::shared_ptr<triangle_mesh_object> create_cone(
            float radius = 0.25, /// `radius` defines the radius of the cone
            float height = 1.0,  /// `height` defines the height of the cone
            int resolution = 20, /// `resolution` defines that the circle will be split into resolution segments
            int split = 1        /// `split` defines that the height will be split into split segments
        );

        static std::shared_ptr<triangle_mesh_object> create_axis_frame(
            float size = 1.0f,
            vec3_f32 origin_point = vec3_f32::Zero()
        );

    }; // class

} // namespace