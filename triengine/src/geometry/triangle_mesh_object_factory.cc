#include "triangle_mesh_object.hh"
#include "../misc/debug_utils.hh"

namespace triengine::geometry
{
    /**
     * Refs:
     * https://github.com/isl-org/Open3D/blob/db00e339c1645440dea6951c2971ffa759934112/cpp/open3d/geometry/TriangleMeshFactory.cpp
     */

    std::shared_ptr<triangle_mesh_object> triangle_mesh_object::create_box(
        const float width,
        const float height,
        const float depth)
    {
        TRIENGINE_ASSERT(width > 0);
        TRIENGINE_ASSERT(height > 0);
        TRIENGINE_ASSERT(depth > 0);

        auto mesh = std::make_shared<triangle_mesh_object>();
        auto& vertex_positions = mesh->vertex_positions;
        auto& vertex_normals = mesh->vertex_normals;
        auto& triangle_indices = mesh->triangle_indices;

        // Normal vector for each plane of the box
        static const std::array<vec3_f32, 6> normals{
            vec3_f32{ 1.0f,  0.0f,  0.0f }, // right-plane
            vec3_f32{-1.0f,  0.0f,  0.0f }, // left-plane
            vec3_f32{ 0.0f,  1.0f,  0.0f }, // top-plane
            vec3_f32{ 0.0f, -1.0f,  0.0f }, // bottom-plane
            vec3_f32{ 0.0f,  0.0f,  1.0f }, // front-plane
            vec3_f32{ 0.0f,  0.0f, -1.0f }  // back-plane
        };

        // Create 8 corners of the box
        const float
            w = width * 0.5f,
            h = height * 0.5f,
            d = depth * 0.5f;

        vertex_positions.emplace_back(-w, -h, -d); vertex_normals.emplace_back(normals[1]);
        vertex_positions.emplace_back(-w, -h, d); vertex_normals.emplace_back(normals[1]);
        vertex_positions.emplace_back(-w, h, d); vertex_normals.emplace_back(normals[1]);
        vertex_positions.emplace_back(-w, h, -d); vertex_normals.emplace_back(normals[1]);

        vertex_positions.emplace_back(w, -h, -d); vertex_normals.emplace_back(normals[0]);
        vertex_positions.emplace_back(w, -h, d); vertex_normals.emplace_back(normals[0]);
        vertex_positions.emplace_back(w, h, d); vertex_normals.emplace_back(normals[0]);
        vertex_positions.emplace_back(w, h, -d); vertex_normals.emplace_back(normals[0]);

        vertex_positions.emplace_back(-w, h, -d); vertex_normals.emplace_back(normals[2]);
        vertex_positions.emplace_back(-w, h, d); vertex_normals.emplace_back(normals[2]);
        vertex_positions.emplace_back(w, h, d); vertex_normals.emplace_back(normals[2]);
        vertex_positions.emplace_back(w, h, -d); vertex_normals.emplace_back(normals[2]);

        vertex_positions.emplace_back(-w, -h, -d); vertex_normals.emplace_back(normals[3]);
        vertex_positions.emplace_back(-w, -h, d); vertex_normals.emplace_back(normals[3]);
        vertex_positions.emplace_back(w, -h, d); vertex_normals.emplace_back(normals[3]);
        vertex_positions.emplace_back(w, -h, -d); vertex_normals.emplace_back(normals[3]);

        vertex_positions.emplace_back(-w, -h, d); vertex_normals.emplace_back(normals[4]);
        vertex_positions.emplace_back(w, -h, d); vertex_normals.emplace_back(normals[4]);
        vertex_positions.emplace_back(w, h, d); vertex_normals.emplace_back(normals[4]);
        vertex_positions.emplace_back(-w, h, d); vertex_normals.emplace_back(normals[4]);

        vertex_positions.emplace_back(-w, -h, -d); vertex_normals.emplace_back(normals[5]);
        vertex_positions.emplace_back(w, -h, -d); vertex_normals.emplace_back(normals[5]);
        vertex_positions.emplace_back(w, h, -d); vertex_normals.emplace_back(normals[5]);
        vertex_positions.emplace_back(-w, h, -d); vertex_normals.emplace_back(normals[5]);

        // Create index array (6 planes, 2 triangles per plane)
        triangle_indices = {
            vec3_i32{ 0,  1,  2 },  vec3_i32{ 0,  2,  3 },  // left-plane
            vec3_i32{ 4,  5,  6 },  vec3_i32{ 4,  6,  7 },  // right-plane
            vec3_i32{ 8,  9,  10 }, vec3_i32{ 8,  10, 11 }, // top-plane
            vec3_i32{ 12, 13, 14 }, vec3_i32{ 12, 14, 15 }, // bottom-plane
            vec3_i32{ 16, 17, 18 }, vec3_i32{ 16, 18, 19 }, // front-plane
            vec3_i32{ 20, 21, 22 }, vec3_i32{ 20, 22, 23 }  // back-plane
        };

        return mesh;
    }

    std::shared_ptr<triangle_mesh_object> triangle_mesh_object::create_sphere(
        const float radius,
        const int resolution)
    {
        TRIENGINE_ASSERT(radius > 0);
        TRIENGINE_ASSERT(resolution > 0);

        auto mesh = std::make_shared<triangle_mesh_object>();
        auto& vertex_positions = mesh->vertex_positions;
        auto& vertex_normals = mesh->vertex_normals;
        auto& triangle_indices = mesh->triangle_indices;

        const int sector_count = resolution * 2; // longitude, # of slices
        const int stack_count = resolution; // latitude, # of stacks
        const float sector_step = 2 * math::pi<float>() / sector_count;
        const float stack_step = math::pi<float>() / stack_count;
        const float radius_inv = 1.0f / radius; // normal

        float sector_angle = 0, stack_angle = 0;
        float x = 0.f, y = 0.f, z = 0.f, xy = 0.f;
        float nx = 0.f, ny = 0.f, nz = 0.f;

        for (int i = 0; i <= stack_count; ++i) {
            stack_angle = math::pi<float>() / 2 - i * stack_step; // starting from pi/2 to -pi/2
            xy = radius * std::cos(stack_angle); // r * cos(u)
            z = radius * std::sin(stack_angle); // r * sin(u)

            // add (sectorCount+1) vertices per stack
            for (int j = 0; j <= sector_count; ++j) {
                sector_angle = j * sector_step; // starting from 0 to 2pi

                // vertex position
                x = xy * std::cos(sector_angle); // r * cos(u) * cos(v)
                y = xy * std::sin(sector_angle); // r * cos(u) * sin(v)

                // normalized vertex normal
                nx = x * radius_inv;
                ny = y * radius_inv;
                nz = z * radius_inv;

                vertex_positions.emplace_back(x, y, z);
                vertex_normals.emplace_back(nx, ny, nz);
            }
        } // for

        // indices
        //  k1--k1+1
        //  |  / |
        //  | /  |
        //  k2--k2+1
        uint32_t k1 = 0, k2 = 0;
        for (int i = 0; i < stack_count; ++i) {
            k1 = i * (sector_count + 1); // beginning of current stack
            k2 = k1 + sector_count + 1;  // beginning of next stack

            for (int j = 0; j < sector_count; ++j, ++k1, ++k2) {
                // 2 triangles per sector excluding 1st and last stacks
                if (i != 0) {
                    // (k1)---(k2)---(k1+1)
                    triangle_indices.emplace_back(k1, k2, k1 + 1);
                }

                if (i != (stack_count - 1)) {
                    // (k1+1)---(k2)---(k2+1)
                    triangle_indices.emplace_back(k1 + 1, k2, k2 + 1);
                }
            }
        } // for

        return mesh;
    }


    std::shared_ptr<triangle_mesh_object> triangle_mesh_object::create_cylinder(
        const float radius,
        const float height,
        const int resolution,
        const int split)
    {
        TRIENGINE_ASSERT(radius > 0);
        TRIENGINE_ASSERT(height > 0);
        TRIENGINE_ASSERT(resolution > 0);
        TRIENGINE_ASSERT(split > 0);

        auto mesh = std::make_shared<triangle_mesh_object>();
        auto& vertex_positions = mesh->vertex_positions;
        auto& vertex_normals = mesh->vertex_normals;
        auto& triangle_indices = mesh->triangle_indices;

        const int sector_count = resolution; // longitude, # of slices of the base circle
        const float sector_step = 2 * math::pi<float>() / sector_count;
        const float height_step = height / split; // height of each split segment
        const float base_radius_inv = 1.0f / radius;    // normal

        float sector_angle = 0;
        float x = 0.f, y = 0.f, z = 0.f;
        float nx = 0.f, ny = 0.f;

        // Generate vertices and normals for each level (split segments)
        for (int i = 0; i <= split; ++i) {
            z = -height / 2 + i * height_step; // current z level

            for (int j = 0; j <= sector_count; ++j) {
                sector_angle = j * sector_step; // starting from 0 to 2pi

                // vertex position
                x = radius * std::cos(sector_angle); // r * cos(v)
                y = radius * std::sin(sector_angle); // r * sin(v)

                // normalized vertex normal
                nx = x * base_radius_inv;
                ny = y * base_radius_inv;

                vertex_positions.emplace_back(x, y, z);
                vertex_normals.emplace_back(nx, ny, 0.0f);
            }
        }

        // Generate triangle indices for the sides of the cylinder
        for (int i = 0; i < split; ++i) {
            //  k1--k1+1
            //  |  / |
            //  | /  |
            //  k2--k2+1
            // NOTE: Only side surface is needed for our case
            uint32_t k1 = i * (sector_count + 1); // beginning of the current level
            uint32_t k2 = k1 + sector_count + 1;  // beginning of the next(up) level
            for (int j = 0; j < sector_count; ++j, ++k1, ++k2) {
                // (k1)---(k2)---(k1+1)
                triangle_indices.emplace_back(k1, k2, k1 + 1);

                // (k1+1)---(k2)---(k2+1)
                triangle_indices.emplace_back(k1 + 1, k2, k2 + 1);
            }
        }

        return mesh;
    }

    std::shared_ptr<triangle_mesh_object> triangle_mesh_object::create_cone(
        const float base_radius,
        const float height,
        const int resolution,
        const int split)
    {
        TRIENGINE_ASSERT(base_radius > 0);
        TRIENGINE_ASSERT(height > 0);
        TRIENGINE_ASSERT(resolution > 0);
        TRIENGINE_ASSERT(split > 0);

        std::vector<vec3_f32> tmp_vertex_positions; {
            tmp_vertex_positions.resize(resolution * split + 2);
            tmp_vertex_positions[0] = vec3_f32(0.0f, 0.0f, 0.0f); // bottom base center vertex
            tmp_vertex_positions[1] = vec3_f32(0.0f, 0.0f, static_cast<float>(height)); // top vertex
            const double step = math::pi<double>() * 2.0 / static_cast<double>(resolution);
            const double h_step = height / static_cast<double>(split);
            const double r_step = base_radius / static_cast<double>(split);
            for (int i = 0; i < split; ++i) {
                const int base_index = 2 + resolution * i; // start index of each rings
                const double r = r_step * (split - i);
                for (int j = 0; j < resolution; ++j) {
                    const double theta = step * j;
                    tmp_vertex_positions[base_index + j] = vec3_f32(
                        static_cast<float>(std::cos(theta) * r),
                        static_cast<float>(std::sin(theta) * r),
                        static_cast<float>(h_step * i)
                    );
                }
            }
        }

        std::vector<vec3_i32> tmp_triangle_indices; {
            tmp_triangle_indices.reserve(split * resolution);

            for (int j = 0; j < resolution; ++j) {
                const int j1 = (j + 1) % resolution;
                // create bottom-surfase triangles (cone's base surface) 
                int base_index = 2;
                tmp_triangle_indices.emplace_back(0, base_index + j1, base_index + j);

                // create top-surface triangles (top segment of cone's conical surface)
                base_index = 2 + resolution * (split - 1);
                tmp_triangle_indices.emplace_back(1, base_index + j, base_index + j1);
            }

            // create side-surface triagnles (cone's conical surface other than top-segment)
            for (int i = 0; i < split - 1; ++i) {
                const int base_index1 = 2 + resolution * i;
                const int base_index2 = base_index1 + resolution;
                for (int j = 0; j < resolution; ++j) {
                    const int j1 = (j + 1) % resolution;
                    tmp_triangle_indices.emplace_back(base_index2 + j1, base_index1 + j, base_index1 + j1);
                    tmp_triangle_indices.emplace_back(base_index2 + j1, base_index2 + j, base_index1 + j);
                }
            }
        }

        auto mesh = std::make_shared<triangle_mesh_object>(); {
            auto& vertex_positions = mesh->vertex_positions;
            auto& vertex_normals = mesh->vertex_normals;
            auto& triangle_indices = mesh->triangle_indices;

            vertex_positions.reserve(tmp_triangle_indices.size() * 3);
            vertex_normals.reserve(tmp_triangle_indices.size() * 3);
            triangle_indices.reserve(tmp_triangle_indices.size());

            for (const vec3_i32& triangle_indice : tmp_triangle_indices)
            {
                const vec3_f32
                    & p0 = tmp_vertex_positions[triangle_indice[0]],
                    & p1 = tmp_vertex_positions[triangle_indice[1]],
                    & p2 = tmp_vertex_positions[triangle_indice[2]];

                const vec3_f32
                    new_triangle_face_normal = (p1 - p0).cross(p2 - p0).normalized();

                const size_t
                    new_triangle_begin_index = vertex_positions.size();

                // NOTE: 각각의 측면 삼각형을 위한 독립 정점을 중복해서 추가 (for flat shading)
                vertex_positions.push_back(p0);
                vertex_positions.push_back(p1);
                vertex_positions.push_back(p2);

                // NOTE: 각 정점에 동일한 face normal을 중복해서 추가 (for flat shading)
                vertex_normals.push_back(new_triangle_face_normal);
                vertex_normals.push_back(new_triangle_face_normal);
                vertex_normals.push_back(new_triangle_face_normal);

                triangle_indices.emplace_back(
                    static_cast<int>(new_triangle_begin_index),
                    static_cast<int>(new_triangle_begin_index + 1),
                    static_cast<int>(new_triangle_begin_index + 2)
                );
            }
        }

        return mesh;
    }

    std::shared_ptr<triangle_mesh_object> triangle_mesh_object::create_frustum(
        const float base_radius, 
        const float top_radius, 
        const float height,
        const int resolution, 
        const int split)
    {
        TRIENGINE_ASSERT(base_radius > 0);
        TRIENGINE_ASSERT(top_radius > 0);
        TRIENGINE_ASSERT(height > 0);
        TRIENGINE_ASSERT(resolution > 0);
        TRIENGINE_ASSERT(split > 0);

        std::vector<vec3_f32> tmp_vertex_positions; {
            // Total vertices: bottom center(1) + top center(1) + rings((split+1)*resolution)
            tmp_vertex_positions.resize(2 + (split + 1) * resolution);
            tmp_vertex_positions[0] = vec3_f32(0.0f, 0.0f, 0.0f); // bottom center vertex
            tmp_vertex_positions[1] = vec3_f32(0.0f, 0.0f, height); // top center vertex

            const double step = math::pi<double>() * 2.0 / static_cast<double>(resolution);
            const double h_step = height / static_cast<double>(split);
            // For each ring(0) ... ring(split): (total `split+1` rings)
            for (int i = 0; i <= split; ++i) {
                const int base_index = 2 + resolution * i; // start index of each rings
                const double r = base_radius + (top_radius - base_radius) * (static_cast<double>(i) / split);
                for (int j = 0; j < resolution; ++j) {
                    const double theta = step * j;
                    tmp_vertex_positions[base_index + j] = vec3_f32(
                        static_cast<float>(std::cos(theta) * r),
                        static_cast<float>(std::sin(theta) * r),
                        static_cast<float>(h_step * i)
                    );
                }
            }
        }

        std::vector<vec3_i32> tmp_triangle_indices; {
            tmp_triangle_indices.reserve((2 + split) * resolution);

            // create bottom face
            // bottom center: index 0
            // bottom ring: index [2, 2+resolution-1]
            for (int j = 0; j < resolution; ++j) {
                const int j1 = (j + 1) % resolution;
                const int base_index = 2;
                tmp_triangle_indices.emplace_back(0, base_index + j1, base_index + j);
            }

            // create top face
            // top center: index 1
            // top ring: index [2 + resolution*split, 2 + resolution*split + resolution - 1]
            {
                const int top_ring_base = 2 + resolution * split;
                for (int j = 0; j < resolution; ++j) {
                    const int j1 = (j + 1) % resolution;
                    tmp_triangle_indices.emplace_back(1, top_ring_base + j, top_ring_base + j1);
                }
            }

            // create side faces
            // Connect each pair of rings to form quads, then split them into two triangles.
            // connect ring(`i`) and ring(`i+1`)
            // range of `i`: `0/*bottom ring*/ <= i < split/*top ring*/`
            // num of side faces: `split`
            for (int i = 0; i < split; ++i) {
                const int base_index1 = 2 + resolution * i;
                const int base_index2 = base_index1 + resolution;
                for (int j = 0; j < resolution; ++j) {
                    const int j1 = (j + 1) % resolution;
                    // Each quad formed by vertices (base1+j, base1+j1, base2+j, base2+j1)
                    // Split into two triangles:
                    //   (base1+j, base2+j1, base2+j)
                    //   (base1+j, base1+j1, base2+j1)
                    tmp_triangle_indices.emplace_back(base_index1 + j, base_index2 + j1, base_index2 + j);
                    tmp_triangle_indices.emplace_back(base_index1 + j, base_index1 + j1, base_index2 + j1);
                }
            }
        }

        auto mesh = std::make_shared<triangle_mesh_object>();
        {
            auto& vertex_positions = mesh->vertex_positions;
            auto& vertex_normals = mesh->vertex_normals;
            auto& triangle_indices = mesh->triangle_indices;

            vertex_positions.reserve(tmp_triangle_indices.size() * 3);
            vertex_normals.reserve(tmp_triangle_indices.size() * 3);
            triangle_indices.reserve(tmp_triangle_indices.size());

            for (const vec3_i32& triangle_indice : tmp_triangle_indices)
            {
                const vec3_f32
                    & p0 = tmp_vertex_positions[triangle_indice[0]],
                    & p1 = tmp_vertex_positions[triangle_indice[1]],
                    & p2 = tmp_vertex_positions[triangle_indice[2]];

                const vec3_f32 
                    new_triangle_face_normal = (p1 - p0).cross(p2 - p0).normalized();

                const size_t 
                    new_triangle_begin_index = vertex_positions.size();

                // NOTE: 각각의 측면 삼각형을 위한 독립 정점을 중복해서 추가 (for flat shading)
                vertex_positions.push_back(p0);
                vertex_positions.push_back(p1);
                vertex_positions.push_back(p2);

                // NOTE: 각 정점에 동일한 face normal을 중복해서 추가 (for flat shading)
                vertex_normals.push_back(new_triangle_face_normal);
                vertex_normals.push_back(new_triangle_face_normal);
                vertex_normals.push_back(new_triangle_face_normal);

                triangle_indices.emplace_back(
                    static_cast<int>(new_triangle_begin_index),
                    static_cast<int>(new_triangle_begin_index + 1),
                    static_cast<int>(new_triangle_begin_index + 2)
                );
            }
        }

        return mesh;
    }

    std::shared_ptr<triangle_mesh_object> triangle_mesh_object::create_arrow(
        const float cylinder_radius, 
        const float cone_radius, 
        const float cylinder_height, 
        const float cone_height, 
        const int resolution, 
        const int cylinder_split, 
        const int cone_split)
    {
        TRIENGINE_ASSERT(cylinder_radius > 0);
        TRIENGINE_ASSERT(cone_radius > 0);
        TRIENGINE_ASSERT(cylinder_height > 0);
        TRIENGINE_ASSERT(cone_height > 0);
        TRIENGINE_ASSERT(resolution > 0);
        TRIENGINE_ASSERT(cylinder_split > 0);
        TRIENGINE_ASSERT(cone_split > 0);

        auto mesh_frame = create_cylinder(
            cylinder_radius, 
            cylinder_height,
            resolution, 
            cylinder_split
        );
        mesh_frame->translate(vec3_f32(0.0f, 0.0f, cylinder_height * 0.5f));
        mesh_frame->apply_model_in_place();

        auto mesh_cone = create_cone(
            cone_radius, 
            cone_height, 
            resolution, 
            cone_split
        );
        mesh_cone->translate(vec3_f32(0.0f, 0.0f, cylinder_height));
        *mesh_frame += *mesh_cone;

        return mesh_frame;
    }

    std::shared_ptr<triangle_mesh_object> triangle_mesh_object::create_coordinate_frame(
        const float size,
        const vec3_f32 origin_point)
    {
        constexpr float
            kCylinderRadiusScale = 0.035f,
            kCylinderHeightScale = 0.8f,
            kConeRadiusScale = 0.06f,
            kConeHeightScale = 0.2f,
            kSphereRadiusScale = 0.06f;

        constexpr int
            kSphereResolution = 15,
            kArrowResolution = 20;

        constexpr int
            kArrowCylinderSplit = 1,
            kArrowConeSplit = 1;

        const float
            cylinder_radius = size * kCylinderRadiusScale,
            cylinder_height = size * kCylinderHeightScale,
            cone_radius = size * kConeRadiusScale,
            cone_height = size * kConeHeightScale,
            sphere_radius = size * kSphereRadiusScale;

        auto mesh_frame = create_sphere(sphere_radius, kSphereResolution);
        mesh_frame->paint_uniform_color(color3_f32(0.5f, 0.5f, 0.5f));

        std::shared_ptr<triangle_mesh_object> mesh_arrow;

        // X-Axis
        mesh_arrow = create_arrow(
            cylinder_radius, cone_radius, 
            cylinder_height, cone_height, 
            kArrowResolution, kArrowCylinderSplit, kArrowConeSplit);
        mesh_arrow->paint_uniform_color(color3_f32(1.0f, 0.0f, 0.0f)); // R
        mesh_arrow->rotate(math::mat3_rotation_y(math::deg2rad(-90.0f))); // rotate along y-axis
        *mesh_frame += *mesh_arrow;

        // Y-Axis
        mesh_arrow = create_arrow(
            cylinder_radius, cone_radius, 
            cylinder_height, cone_height, 
            kArrowResolution, kArrowCylinderSplit, kArrowConeSplit);
        mesh_arrow->paint_uniform_color(color3_f32(0.0f, 1.0f, 0.0f)); // G
        mesh_arrow->rotate(math::mat3_rotation_x(math::deg2rad(90.0f))); // rotate along x-axis
        *mesh_frame += *mesh_arrow;

        // Z-Axis
        mesh_arrow = create_arrow(
            cylinder_radius, cone_radius, 
            cylinder_height, cone_height, 
            kArrowResolution, kArrowCylinderSplit, kArrowConeSplit);
        mesh_arrow->paint_uniform_color(color3_f32(0.0f, 0.0f, 1.0f)); // B
        *mesh_frame += *mesh_arrow;

        mesh_frame->translate(origin_point);
        return mesh_frame;
    }

    std::shared_ptr<triangle_mesh_object> triangle_mesh_object::create_skeletal_bone(
        const float radius, 
        const float height,
        const int resolution,
        const int split)
    {
        constexpr float
            kChildSideFrustumRadiusRatio = 0.25f;
        
        constexpr float
            kFrustumHeightRatio = 0.85f,
            kParentSideConeHeightRatio = (1.0f - kFrustumHeightRatio) * 0.6f,
            kChildSideConeHeightRatio = 1.0f - kFrustumHeightRatio - kParentSideConeHeightRatio;
        
        const float
            parent_side_frustum_radius = radius,
            child_side_frustum_radius = radius * kChildSideFrustumRadiusRatio,
            frustum_height = height * kFrustumHeightRatio,
            parent_side_cone_height = height * kParentSideConeHeightRatio,
            child_side_cone_height = height * kChildSideConeHeightRatio;

        const float
            height_half = height * 0.5f;

        std::vector<vec3_f32> tmp_vertex_positions; {
            // Total vertices: bottom center(1) + top center(1) + rings((split+1)*resolution)
            tmp_vertex_positions.resize(2 + (split + 1) * resolution);
            tmp_vertex_positions[0] = vec3_f32(0.0f, 0.0f, -height_half); // bottom center vertex
            tmp_vertex_positions[1] = vec3_f32(0.0f, 0.0f, height_half); // top center vertex

            const double step = math::pi<double>() * 2.0 / static_cast<double>(resolution);
            const double h_step = frustum_height / static_cast<double>(split);

            // For each ring(0) ... ring(split): (total `split+1` rings)
            for (int i = 0; i <= split; ++i) {
                const int base_index = 2 + resolution * i; // start index of each rings
                const double r = parent_side_frustum_radius + (child_side_frustum_radius - parent_side_frustum_radius) * (static_cast<double>(i) / split);
                for (int j = 0; j < resolution; ++j) {
                    const double theta = step * j + math::deg2rad(45.0)/* initial rotation offset */;
                    tmp_vertex_positions[base_index + j] = vec3_f32(
                        static_cast<float>(std::cos(theta) * r),
                        static_cast<float>(std::sin(theta) * r),
                        static_cast<float>(h_step * i) + parent_side_cone_height - height_half
                    );
                }
            }
        }

        std::vector<vec3_i32> tmp_triangle_indices; {
            tmp_triangle_indices.reserve((2 + split) * resolution);

            // create bottom face
            // bottom center: index 0
            // bottom ring: index [2, 2+resolution-1]
            for (int j = 0; j < resolution; ++j) {
                const int j1 = (j + 1) % resolution;
                const int base_index = 2;
                tmp_triangle_indices.emplace_back(0, base_index + j1, base_index + j);
            }

            // create top face
            // top center: index 1
            // top ring: index [2 + resolution*split, 2 + resolution*split + resolution - 1]
            {
                const int top_ring_base = 2 + resolution * split;
                for (int j = 0; j < resolution; ++j) {
                    const int j1 = (j + 1) % resolution;
                    tmp_triangle_indices.emplace_back(1, top_ring_base + j, top_ring_base + j1);
                }
            }

            // create side faces
            // Connect each pair of rings to form quads, then split them into two triangles.
            // connect ring(`i`) and ring(`i+1`)
            // range of `i`: `0/*bottom ring*/ <= i < split/*top ring*/`
            // num of side faces: `split`
            for (int i = 0; i < split; ++i) {
                const int base_index1 = 2 + resolution * i;
                const int base_index2 = base_index1 + resolution;
                for (int j = 0; j < resolution; ++j) {
                    const int j1 = (j + 1) % resolution;
                    // Each quad formed by vertices (base1+j, base1+j1, base2+j, base2+j1)
                    // Split into two triangles:
                    //   (base1+j, base2+j1, base2+j)
                    //   (base1+j, base1+j1, base2+j1)
                    tmp_triangle_indices.emplace_back(base_index1 + j, base_index2 + j1, base_index2 + j);
                    tmp_triangle_indices.emplace_back(base_index1 + j, base_index1 + j1, base_index2 + j1);
                }
            }
        }

        auto mesh = std::make_shared<triangle_mesh_object>();
        {
            auto& vertex_positions = mesh->vertex_positions;
            auto& vertex_normals = mesh->vertex_normals;
            auto& triangle_indices = mesh->triangle_indices;

            vertex_positions.reserve(tmp_triangle_indices.size() * 3);
            vertex_normals.reserve(tmp_triangle_indices.size() * 3);
            triangle_indices.reserve(tmp_triangle_indices.size());

            for (const vec3_i32& triangle_indice : tmp_triangle_indices)
            {
                const vec3_f32
                    & p0 = tmp_vertex_positions[triangle_indice[0]],
                    & p1 = tmp_vertex_positions[triangle_indice[1]],
                    & p2 = tmp_vertex_positions[triangle_indice[2]];

                const vec3_f32
                    new_triangle_face_normal = (p1 - p0).cross(p2 - p0).normalized();

                const size_t
                    new_triangle_begin_index = vertex_positions.size();

                // NOTE: 각각의 측면 삼각형을 위한 독립 정점을 중복해서 추가 (for flat shading)
                vertex_positions.push_back(p0);
                vertex_positions.push_back(p1);
                vertex_positions.push_back(p2);

                // NOTE: 각 정점에 동일한 face normal을 중복해서 추가 (for flat shading)
                vertex_normals.push_back(new_triangle_face_normal);
                vertex_normals.push_back(new_triangle_face_normal);
                vertex_normals.push_back(new_triangle_face_normal);

                triangle_indices.emplace_back(
                    static_cast<int>(new_triangle_begin_index),
                    static_cast<int>(new_triangle_begin_index + 1),
                    static_cast<int>(new_triangle_begin_index + 2)
                );
            }
        }

        return mesh;
    }

} // namespace
