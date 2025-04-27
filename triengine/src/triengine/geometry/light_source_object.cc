#include "light_source_object.hh"
#include <triengine/math/constants.hh>
#include <triengine/utility/debug_utils.hh>
#include <triengine/utility/debug_utils.hh>

namespace triengine::geometry
{
    std::shared_ptr<light_source_object> light_source_object::create(
        const float radius)
    {
        TRIENGINE_ASSERT(radius > 0);

        auto new_object = std::make_shared<light_source_object>();
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
            } // for
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
            } // for
        } // for

        return new_object;
    }

} // namespace