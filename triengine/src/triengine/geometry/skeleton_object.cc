#include "skeleton_object.hh"
#include <triengine/misc/debug_utils.hh>

namespace triengine::geometry
{
    namespace {
        namespace detail
        {
            inline void compute_rotation_between_vectors(
                mat4_f32& rotation/* out */,
                const vec3_f32& v0,
                const vec3_f32& v1)
            {
                vec3_f32
                    u0 = v0.normalized(),
                    u1 = v1.normalized(),
                    v = u0.cross(u1);

                const float
                    sinTheta = v.norm(); // get vector length

                if (sinTheta < 0.00001f) {
                    rotation = mat4_f32::Identity();
                    return;
                }

                const float
                    cosTheta = u0.dot(u1),
                    scale = 1.0f / (1.0f + cosTheta);

                // Ref: https://en.wikipedia.org/wiki/Rodrigues%27_rotation_formula
                mat4_f32 vx;
                vx <<
                    0.0f, -v.z(), v.y(), 0.0f,
                    v.z(), 0.0f, -v.x(), 0.0f,
                    -v.y(), v.x(), 0.0f, 0.0f,
                    0.0f, 0.0f, 0.0f, 1.0f;

                const mat4_f32 vx2 = vx * vx;
                const mat4_f32 vx2Scaled = vx2 * scale;

                rotation = mat4_f32::Identity() + vx + vx2Scaled;
                rotation(3, 3) = 1.0f;
            }

            inline void compute_cylinder_parameters(
                mat4_f32& cylinder_model/* out */,
                float& cylinder_height/* out */,
                const vec3_f32& start,
                const vec3_f32& end)
            {
                const vec3_f32 centralAxis = start - end;
                cylinder_height = centralAxis.norm(); // get vector length

                const vec3_f32 centerPosition = (start + end) * 0.5f;

                // Create translation matrix
                // Note: https://stackoverflow.com/questions/59222806/how-does-glm-handle-translation
                mat4_f32 translation{ mat4_f32::Identity() };
                translation.block<3, 1>(0, 3) = centerPosition;

                mat4_f32 rotation;
                const vec3_f32 zAxis(0.0f, 0.0f, 1.0f);
                compute_rotation_between_vectors(
                    rotation,
                    zAxis,
                    centralAxis
                );

                cylinder_model = translation * rotation;
            }

        } // namespace
    } // namespace

    void skeleton_object::translate(
        const vec3_f32& t, 
        const bool relative)
    {
        geometry_object_base::translate(t, relative);
        this->_update_objects_model();
    }

    void skeleton_object::rotate(
        const mat3_f32& R, 
        const bool relative)
    {
        geometry_object_base::rotate(R, relative);
        this->_update_objects_model();
    }

    void skeleton_object::rotate(
        const quat_f32& Q, 
        const bool relative)
    {
        geometry_object_base::rotate(Q, relative);
        this->_update_objects_model();
    }

    void skeleton_object::transform(
        const mat4_f32& T, 
        const bool relative)
    {
        geometry_object_base::transform(T, relative);
        this->_update_objects_model();
    }

    void skeleton_object::add_joint(
        const vec3_f32& joint_pos,
        const quat_f32& joint_rot,
        const color3_f32& joint_color)
    {
        auto mesh = geometry::triangle_mesh_object::create_sphere(kDefaultJointRadius);
        mesh->translate(joint_pos);
        mesh->rotate(joint_rot);
        mesh->transform(this->get_model(), true);
        mesh->paint_uniform_color(joint_color);
        joint_objects.emplace_back(mesh);
    }

    void skeleton_object::add_bone(
        const vec3_f32& from_joint_pos,
        const vec3_f32& to_joint_pos,
        const color3_f32& bone_color)
    {
        mat4_f32 cylinder_model;
        float cylinder_height;
        detail::compute_cylinder_parameters(
            cylinder_model,
            cylinder_height,
            from_joint_pos,
            to_joint_pos
        );
        auto mesh = geometry::triangle_mesh_object::create_skeletal_bone(
            std::max(0.0125f, cylinder_height * kDefaultBoneRadiusRatio), 
            cylinder_height
        );
        mesh->transform(cylinder_model);
        mesh->transform(this->get_model(), true);
        mesh->paint_uniform_color(bone_color);
        bone_objects.emplace_back(mesh);
    }

    void skeleton_object::clear()
    {
        joint_objects.clear();
        bone_objects.clear();
    }

    void skeleton_object::_update_objects_model()
    {
        const auto& model = this->get_model();
        for (auto& obj : joint_objects) {
            obj->transform(model, true);
        }
        for (auto& obj : bone_objects) {
            obj->transform(model, true);
        }
    }

} // namespace