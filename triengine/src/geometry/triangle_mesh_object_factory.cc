#include "triangle_mesh_object.hh"
#include "../misc/debug_utils.hh"

namespace triengine::geometry
{
    std::shared_ptr<triangle_mesh_object> triangle_mesh_object::create_box(
        const float width,
        const float height,
        const float depth)
    {
        TRIENGINE_ASSERT(width > 0);
        TRIENGINE_ASSERT(height > 0);
        TRIENGINE_ASSERT(depth > 0);

        /// FIXME: consider triangle winding order

        auto new_object = std::make_shared<triangle_mesh_object>();
        auto& vertex_positions = new_object->vertex_positions;
        auto& vertex_normals = new_object->vertex_normals;
        auto& triangle_indices = new_object->triangle_indices;

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

        return new_object;
    }

    std::shared_ptr<triangle_mesh_object> triangle_mesh_object::create_sphere(
        const float radius)
    {
        TRIENGINE_ASSERT(radius > 0);

        /// FIXME: consider triangle winding order

        auto new_object = std::make_shared<triangle_mesh_object>();
        auto& vertex_positions = new_object->vertex_positions;
        auto& vertex_normals = new_object->vertex_normals;
        auto& triangle_indices = new_object->triangle_indices;

        constexpr int sector_count = 36; // longitude, # of slices
        constexpr int stack_count = 18; // latitude, # of stacks
        constexpr float sector_step = 2 * math::pi<float>() / sector_count;
        constexpr float stack_step = math::pi<float>() / stack_count;

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

        return new_object;
    }

    std::shared_ptr<triangle_mesh_object> triangle_mesh_object::create_cylinder(
        const float base_radius,
        const float height)
    {
        TRIENGINE_ASSERT(base_radius > 0);
        TRIENGINE_ASSERT(height > 0);

        /// FIXME: consider triangle winding order

        auto new_object = std::make_shared<triangle_mesh_object>();
        auto& vertex_positions = new_object->vertex_positions;
        auto& vertex_normals = new_object->vertex_normals;
        auto& triangle_indices = new_object->triangle_indices;

        constexpr int sector_count = 36; // longitude, # of slices of the base circle
        constexpr float sector_step = 2 * math::pi<float>() / sector_count;

        const float base_radius_inv = 1.0f / base_radius;    // normal

        float sector_angle = 0;
        float x = 0.f, y = 0.f, z = 0.f;
        float nx = 0.f, ny = 0.f;

        for (int circle_index = 0; circle_index < 2; ++circle_index) {
            z = circle_index == 0 ? height / 2 : -height / 2;

            for (int j = 0; j <= sector_count; ++j) {
                sector_angle = j * sector_step;           // starting from 0 to 2pi

                // vertex position
                x = base_radius * std::cos(sector_angle);             // r * cos(v)
                y = base_radius * std::sin(sector_angle);             // r * sin(v)

                // normalized vertex normal
                nx = x * base_radius_inv;
                ny = y * base_radius_inv;

                vertex_positions.emplace_back(x, y, z);
                vertex_normals.emplace_back(nx, ny, 0.0f);
            }
        }

        // indices
        //  k1--k1+1
        //  |  / |
        //  | /  |
        //  k2--k2+1
        //
        // Only side surface is needed for our case
        uint32_t k1 = 0;                         // beginning of bottom circle
        uint32_t k2 = k1 + sector_count + 1;    // beginning of top circle
        for (int j = 0; j < sector_count; ++j, ++k1, ++k2) {
            // (k1)---(k2)---(k1+1)
            triangle_indices.emplace_back(k1, k2, k1 + 1);

            // (k1+1)---(k2)---(k2+1)
            triangle_indices.emplace_back(k1 + 1, k2, k2 + 1);
        }

        return new_object;
    }

    // Ref: https://github.com/isl-org/Open3D/blob/db00e339c1645440dea6951c2971ffa759934112/cpp/open3d/geometry/TriangleMeshFactory.cpp#L539
    std::shared_ptr<triangle_mesh_object> triangle_mesh_object::create_cone(
        const float radius,
        const float height,
        const int resolution,
        const int split)
    {
        TRIENGINE_ASSERT(radius > 0);
        TRIENGINE_ASSERT(height > 0);
        TRIENGINE_ASSERT(resolution > 0);
        TRIENGINE_ASSERT(split > 0);

        /// FIXME: consider triangle winding order

        auto mesh = std::make_shared<triangle_mesh_object>();
        auto& vertex_positions = mesh->vertex_positions;
        auto& triangle_indices = mesh->triangle_indices;

        vertex_positions.resize(resolution * split + 2);
        vertex_positions[0] = Eigen::Vector3f(0.0f, 0.0f, 0.0f);
        vertex_positions[1] = Eigen::Vector3f(0.0f, 0.0f, static_cast<float>(height));

        const double step = math::pi<double>() * 2.0 / static_cast<double>(resolution);
        const double h_step = height / static_cast<double>(split);
        const double r_step = radius / static_cast<double>(split);

        for (int i = 0; i < split; ++i) {
            const int base = 2 + resolution * i;
            const double r = r_step * (split - i);
            for (int j = 0; j < resolution; ++j) {
                const double theta = step * j;
                auto& vertex_pos = vertex_positions[base + j];
                vertex_pos.x() = static_cast<float>(std::cos(theta) * r);
                vertex_pos.y() = static_cast<float>(std::sin(theta) * r);
                vertex_pos.z() = static_cast<float>(h_step * i);
            } // for
        } // for

        for (int j = 0; j < resolution; ++j) {
            const int j1 = (j + 1) % resolution;
            // Triangles for bottom surface.
            int base = 2;
            triangle_indices.emplace_back(0, base + j, base + j1);

            // Triangles for top segment of conical surface.
            base = 2 + resolution * (split - 1);
            triangle_indices.emplace_back(1, base + j, base + j1);
        }

        // Triangles for conical surface other than top-segment.
        for (int i = 0; i < split - 1; ++i) {
            const int base1 = 2 + resolution * i;
            const int base2 = base1 + resolution;
            for (int j = 0; j < resolution; ++j) {
                const int j1 = (j + 1) % resolution;
                triangle_indices.emplace_back(base2 + j1, base1 + j, base1 + j1);
                triangle_indices.emplace_back(base2 + j1, base2 + j, base1 + j);
            }
        }

        mesh->compute_vertex_normals();
        return mesh;
    }

    std::shared_ptr<triangle_mesh_object> triangle_mesh_object::create_axis_frame(
        const float size,
        const vec3_f32 origin_point)
    {
        constexpr float
            kDefaultOriginSphereRadius = 0.035f,
            kDefaultAxisThickness = 0.025f,
            kDefaultAxisHeight = 1.0f;

        const float
            size_scale = size / 1.0f,
            scaled_origin_sphere_radius = kDefaultOriginSphereRadius * size_scale,
            scaled_axis_thickness = kDefaultAxisThickness * size_scale,
            scaled_axis_height = kDefaultAxisHeight * size_scale;

        auto axis_center = create_sphere(scaled_origin_sphere_radius); // origin sphere
        axis_center->paint_uniform_color(color3_f32(0.8f, 0.8f, 0.8f));

        auto x_axis = create_cylinder(scaled_axis_thickness, scaled_axis_height);
        x_axis->paint_uniform_color(color3_f32(1.0f, 0.0f, 0.0f)); // R
        x_axis->rotate(math::mat3_rotation_y(math::deg2rad(90.0f))); // rotate along y-axis
        x_axis->translate(vec3_f32(scaled_axis_height * 0.5f, 0.0f, 0.0f));

        auto y_axis = create_cylinder(scaled_axis_thickness, scaled_axis_height);
        y_axis->paint_uniform_color(color3_f32(0.0f, 1.0f, 0.0f)); // G
        y_axis->rotate(math::mat3_rotation_x(math::deg2rad(90.0f))); // rotate along x-axis
        y_axis->translate(vec3_f32(0.0f, scaled_axis_height * 0.5f, 0.0f));

        auto z_axis = create_cylinder(scaled_axis_thickness, scaled_axis_height);
        z_axis->paint_uniform_color(color3_f32(0.0f, 0.0f, 1.0f)); // B
        z_axis->translate(vec3_f32(0.0f, 0.0f, scaled_axis_height * 0.5f));

        *axis_center += *x_axis;
        *axis_center += *y_axis;
        *axis_center += *z_axis;

        axis_center->translate(origin_point);
        return axis_center;
    }

} // namespace
