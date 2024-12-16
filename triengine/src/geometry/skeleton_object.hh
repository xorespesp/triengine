#pragma once
#include "geometry_object_base.hh"
#include "triangle_mesh_object.hh"

#include <list>
#include <memory>

namespace triengine::geometry
{
    class skeleton_object
        : public geometry_object_base
    {
        static constexpr float
            kDefaultBoneRadiusRatio{ 0.09f },
            kDefaultJointRadius{ 0.0175f };

    public:
        std::list<std::shared_ptr<geometry::triangle_mesh_object>>
            joint_objects,
            bone_objects;

    public:
        skeleton_object()
            : geometry_object_base{ geometry_object_type::skeleton }
        {}

        void translate(
            const vec3_f32& t,
            bool relative = false
        ) override;

        void rotate(
            const mat3_f32& R,
            bool relative = false
        ) override;

        void rotate(
            const quat_f32& Q,
            bool relative = false
        ) override;

        void transform(
            const mat4_f32& T,
            bool relative = false
        ) override;

        void add_joint(
            const vec3_f32& joint_pos,
            const quat_f32& joint_rot,
            const color3_f32& joint_color
        );
        
        void add_bone(
            const vec3_f32& from_joint_pos,
            const vec3_f32& to_joint_pos,
            const color3_f32& bone_color
        );

        void clear();

    private:
        void _update_objects_model();

    public:
        static std::shared_ptr<skeleton_object> create() {
            return std::make_shared<skeleton_object>();
        }

    }; // class

} // namespace