#pragma once
#include <triengine/geometry/geometry_object_base.hh>
#include <triengine/texture_params.hh>

#include <unordered_map>
#include <vector>
#include <memory>
#include <variant>
#include <utility>

namespace triengine::geometry
{
    // Ref: https://github.com/isl-org/Open3D/blob/db00e339c1645440dea6951c2971ffa759934112/cpp/open3d/geometry/TriangleMesh.h

    class mesh_object
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
            float ambient_intensity{ 1.0f }; /// ambient intensity; must be `>= 0`
            float diffuse_intensity{ 1.0f }; /// diffuse intensity; must be `>= 0`
            float specular_intensity{ 0.25f }; /// specular intensity; must be `>= 0`
            uint16_t shininess{ 128 }; /// object surface shininess scalar (must be `> 0`)
            float alpha{ 1.0f }; /// object transparency (WBOIT); must be `[0.0...1.0]`

            vertex_shading_material() = default;
            vertex_shading_material(
                float ambient_intensity_,
                float diffuse_intensity_,
                float specular_intensity_,
                uint16_t shininess_,
                float alpha_)
                : ambient_intensity{ ambient_intensity_ }
                , diffuse_intensity{ diffuse_intensity_ }
                , specular_intensity{ specular_intensity_ }
                , shininess{ shininess_ }
                , alpha{ alpha_ }
            {}

            bool is_valid() const noexcept {
                return 
                    0.0f <= ambient_intensity &&
                    0.0f <= diffuse_intensity &&
                    0.0f <= specular_intensity &&
                    0 < shininess &&
                    0.0f <= alpha && alpha <= 1.0f;
            }
        };

        /// NOTE: Only used in texture-shading mode
        struct texture_shading_material
        {
            texture_handle_t diffuse_map{ kInvalidTextureHandle }; /// diffuse-map texture. (TODO: replace texture_2d -> pure image object)
            texture_handle_t specular_map{ kInvalidTextureHandle }; /// specular-map texture (TODO: replace texture_2d -> pure image object)
            float ambient_intensity{ 1.0f }; /// ambient intensity; must be `>= 0`
            float diffuse_intensity{ 1.0f }; /// diffuse intensity; must be `>= 0`
            float specular_intensity{ 1.0f }; /// specular intensity; must be `>= 0`
            uint16_t shininess{ 128 }; /// object surface shininess scalar (must be `> 0`)
            float alpha{ 1.0f }; /// object transparency (WBOIT); must be `[0.0...1.0]`

            texture_shading_material() = default;
            texture_shading_material(
                texture_handle_t diffuse_map_,
                texture_handle_t specular_map_,
                float ambient_intensity_,
                float diffuse_intensity_,
                float specular_intensity_,
                uint16_t shininess_,
                float alpha_)
                : diffuse_map{ diffuse_map_ }
                , specular_map{ specular_map_ }
                , ambient_intensity{ ambient_intensity_ }
                , diffuse_intensity{ diffuse_intensity_ }
                , specular_intensity{ specular_intensity_ }
                , shininess{ shininess_ }
                , alpha{ alpha_ }
            {}

            bool is_valid() const noexcept {
                return
                    diffuse_map != kInvalidTextureHandle &&
                    specular_map != kInvalidTextureHandle &&
                    0.0f <= ambient_intensity &&
                    0.0f <= diffuse_intensity &&
                    0.0f <= specular_intensity &&
                    0 < shininess &&
                    0.0f <= alpha && alpha <= 1.0f;
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

    private:
        std::shared_ptr<geometry_object_base> clone_impl() const override {
            TRIENGINE_PANIC("Not implemented");
            //return nullptr;
        }

    public:
        mesh_object()
            : geometry_object_base{ geometry_object_type::mesh }
        {
            this->set_shading_mode(_shading_mode);
        }

        mesh_object(shading_mode mode)
            : geometry_object_base{ geometry_object_type::mesh }
        {
            this->set_shading_mode(mode);
        }

        std::shared_ptr<mesh_object> clone() const {
            return std::static_pointer_cast<mesh_object>(this->clone_impl());
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

        /// Returns `true` if the mesh contains triangle's vertex color
        bool has_triangle_colors() const {
            return this->has_triangles() && vertex_colors.size() > 0;
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

        /// Removes duplicated vertieces.
        mesh_object& remove_duplicated_vertices();

        /// Removes vertices from the triangle mesh that are 
        /// not referenced in any triangle of the mesh.
        mesh_object& remove_unreferenced_vertices();

        /// Normalize vertex normals to length 1.
        mesh_object& normalize_vertex_normals();

        /// compute vertex normals. (usually called before rendering)
        mesh_object& compute_vertex_normals(bool smooth_shading = false);

        void paint_uniform_color(const color3_f32& color) {
            vertex_colors.clear();
            vertex_colors.resize(vertex_positions.size(), color);
        }

        void apply_model_in_place() override {
            const mat4_f32& model_mat = this->get_model();
            for (vec3_f32& vp : vertex_positions) {
                vp = (model_mat * vec4_f32(vp.x(), vp.y(), vp.z(), 1.0f)).block<3, 1>(0, 0);;
            }

            // Calculate normal matrix for normal vector transformation
            // In case of non-uniform scaling, simply applying the model matrix does not correctly transform the normal vector.
            const mat3_f32 normal_mat = mat3_f32(model_mat.block<3, 3>(0, 0)).inverse().transpose();
            for (vec3_f32& vn : vertex_normals) {
                vn = (normal_mat * vn).normalized();
            }

            this->set_model(math::mat4_identity<float>());
        }

        vec3_f32 get_min_bound() const override {
            return this->compute_min_bound(vertex_positions);
        }

        vec3_f32 get_max_bound() const override {
            return this->compute_max_bound(vertex_positions);
        }
        
        vec3_f32 get_center() const override {
            return this->compute_center(vertex_positions);
        }

        bool is_opaque() const noexcept {
            const float alpha{ [this]() {
                switch (_shading_mode) {
                case shading_mode::vertex: return this->get_vertex_shading_material()->alpha;
                case shading_mode::texture: return this->get_texture_shading_material()->alpha;
                default: return 1.0f;
                }
            }() };
            return std::fabs(1.0f - std::clamp(alpha, 0.0f, 1.0f)) < std::numeric_limits<float>::epsilon();
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
        mesh_object& operator+=(
            const mesh_object& rhs
        );

    public:
        /// Factory function to create a box(3d cube) mesh
        /// \param width is x-directional length.
        /// \param height is y-directional length.
        /// \param depth is z-directional length.
        static std::shared_ptr<mesh_object> create_box(
            float width = 1.0f,
            float height = 1.0f,
            float depth = 1.0f
        );

        /// Factory function to create a sphere mesh
        /// The sphere with radius will be centered at (0, 0, 0).
        /// Its axis is aligned with z-axis.
        /// \param radius defines radius of the sphere.
        /// \param resolution defines the resolution of the sphere.
        static std::shared_ptr<mesh_object> create_sphere(
            float radius = 1.0f, /// radius of the sphere
            int resolution = 20 /// defines that the sphere will be split into resolution segments.
        );

        /// Factory function to create a cylinder mesh
        /// The axis of the cylinder will be from (0, 0, -height/2) to (0, 0, height/2).
        /// The circle with radius will be split into resolution segments.
        /// The height will be split into split segments.
        /// \param radius defines the radius of the cylinder.
        /// \param height defines the height of the cylinder.
        /// \param resolution defines that the circle will be split into resolution segments.
        /// \param split defines that the height will be split into split segments.
        static std::shared_ptr<mesh_object> create_cylinder(
            float radius = 0.1f,
            float height = 1.0f,
            int resolution = 20,
            int split = 1
        );

        /// Factory function to create a cone mesh
        /// The axis of the cone will be from (0, 0, 0) to (0, 0, height).
        /// The circle with radius will be split into resolution segments.
        /// The height will be split into split segments.
        /// \param base_radius defines the base radius of the cone.
        /// \param height defines the height of the cone.
        /// \param resolution defines that the circle will be split into resolution segments.
        /// \param split defines that the height will be split into split segments.
        static std::shared_ptr<mesh_object> create_cone(
            float base_radius = 0.25,
            float height = 1.0, 
            int resolution = 20,
            int split = 1
        );

        /// Factory function to create a conical frutum mesh
        /// https://en.wikipedia.org/wiki/Frustum
        /// The axis of the cone will be from (0, 0, 0) to (0, 0, height).
        /// The circle with radius will be split into resolution segments.
        /// The height will be split into split segments.
        /// \param base_radius defines the base radius of the conical frutum.
        /// \param top_radius defines the top radius of the conical frutum.
        /// \param height defines the height of the cone.
        /// \param resolution defines that the circle will be split into resolution segments.
        /// \param split defines that the height will be split into split segments.
        static std::shared_ptr<mesh_object> create_frustum(
            float base_radius = 0.15,
            float top_radius = 0.05,
            float height = 1.0,
            int resolution = 20,
            int split = 1
        );

        /// Factory function to create a Bifrustum mesh.
        /// https://en.wikipedia.org/wiki/Bifrustum
        /// The Bi-Frustum consists of two frustums joined at their middle.
        /// The axis of the Bi-Frustum will be from (0, 0, -height/2) to (0, 0, height/2).
        /// \param middle_radius defines the radius at the junction of the two frustums.
        /// \param bottom_cap_radius defines the radius of the bottom circular face.
        /// \param top_cap_radius defines the radius of the top circular face.
        /// \param height defines the total height of the Bi-Frustum.
        /// \param height_ratio_bottom defines the proportion of the total height occupied by the bottom frustum (0.0 to 1.0).
        ///                            0.0 means bottom frustum has no height (becomes a disk/cone base).
        ///                            1.0 means top frustum has no height.
        /// \param resolution defines that circles will be split into resolution segments.
        /// \param split_bottom defines the number of height segments for the bottom frustum.
        /// \param split_top defines the number of height segments for the top frustum.
        static std::shared_ptr<mesh_object> create_bifrustum(
            float middle_radius = 0.10f,
            float bottom_cap_radius = 0.05f,
            float top_cap_radius = 0.05f,
            float height = 1.0f,
            float height_ratio_bottom = 0.5f,
            int resolution = 20,
            int split_bottom = 1,
            int split_top = 1
        );

        /// Factory function to create a torus mesh
        /// https://en.wikipedia.org/wiki/Torus
        /// The torus will be centered at (0, 0, 0) and a radius of
        /// torus_radius. The tube of the torus will have a radius of
        /// tube_radius. The number of segments in radial and tubular direction are
        /// radial_resolution and tubular_resolution respectively.
        /// \param torus_radius defines the radius from the center of the torus to
        /// the center of the tube.
        /// \param tube_radius defines the radius of the torus tube.
        /// \param radial_resolution defines the he number of segments along the
        /// radial direction.
        /// \param tubular_resolution defines the number of segments along the
        /// tubular direction.
        //static std::shared_ptr<mesh_object> create_torus(
        //    double torus_radius = 1.0,
        //    double tube_radius = 0.5,
        //    int radial_resolution = 30,
        //    int tubular_resolution = 20
        //);

        /// Factory function to create a tetrahedron mesh
        /// https://en.wikipedia.org/wiki/Tetrahedron
        /// the mesh centroid will be at (0,0,0) and \p radius defines the
        /// distance from the center to the mesh vertices.
        /// \param radius defines the distance from centroid to mesh vetices.
        //static std::shared_ptr<mesh_object> create_tetrahedron(
        //    double radius = 1.0
        //);

        /// Factory function to create an octahedron mesh
        /// https://en.wikipedia.org/wiki/Octahedron
        /// the mesh centroid will be at (0,0,0) and \p radius defines the
        /// distance from the center to the mesh vertices.
        /// \param radius defines the distance from centroid to mesh vetices.
        //static std::shared_ptr<mesh_object> create_octahedron(
        //    double radius = 1.0
        //);

        /// Factory function to create an icosahedron mesh
        /// https://en.wikipedia.org/wiki/Icosahedron
        /// (trianglemeshfactory.cpp). The mesh centroid will be at (0,0,0) and
        /// \param radius defines the distance from the center to the mesh vertices.
        //static std::shared_ptr<mesh_object> create_icosahedron(
        //    double radius = 1.0
        //);

        /// Factory function to create an arrow-shape mesh
        /// The axis of the cone with cone_radius will be along the z-axis.
        /// The cylinder with cylinder_radius is from (0, 0, 0) to (0, 0, cylinder_height), 
        /// and the cone is from (0, 0, cylinder_height) to (0, 0, cylinder_height + cone_height).
        /// The cone will be split into resolution segments.
        /// The cylinder_height will be split into cylinder_split segments.
        /// The cone_height will be split into cone_split segments.
        /// \param cylinder_radius defines the radius of the cylinder.
        /// \param cone_radius defines the radius of the cone.
        /// \param cylinder_height defines the height of the cylinder. 
        ///        The cylinder is from (0, 0, 0) to (0, 0, cylinder_height)
        /// \param cone_height defines the height of the cone. 
        ///        The axis of the cone will be from (0, 0, cylinder_height) to (0, 0, cylinder_height + cone_height).
        /// \param resolution defines the cone will be split into resolution segments.
        /// \param cylinder_split defines the cylinder_height will be split into cylinder_split segments.
        /// \param cone_split defines the cone_height will be split into cone_split segments.
        static std::shared_ptr<mesh_object> create_arrow(
            float cylinder_radius = 0.1f,
            float cone_radius = 0.15f,
            float cylinder_height = 1.0f,
            float cone_height = 0.25f,
            int resolution = 20,
            int cylinder_split = 4,
            int cone_split = 1
        );

        /// Factory function to create a coordinate frame mesh arrows respectively.
        /// \p size is the length of the axes.
        /// \param size defines the size of the coordinate frame.
        /// \param origin defines the origin of the coordinate frame.
        static std::shared_ptr<mesh_object> create_coordinate_frame(
            float size = 1.0f,
            vec3_f32 origin_point = math::vec3_all(0.0f)
        );

    }; // class

} // namespace