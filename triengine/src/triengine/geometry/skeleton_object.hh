#pragma once
#include <triengine/geometry/geometry_object_base.hh>
#include <triengine/geometry/triangle_mesh_object.hh>

#include <list>
#include <memory>

namespace triengine::geometry
{
    // NOTE: 
    // In the current implementation, the vertex data of the internal `triangle_mesh_object`s
    // inside the `skeleton_object` must NOT be modified after initialization.
    // (The `gpu_resource_manager` does not automatically track these changes.)

    class skeleton_object
        : public geometry_object_base
    {
        static constexpr float
            kDefaultBoneRadiusRatio{ 0.09f },
            kDefaultJointRadius{ 0.0175f };

    private:
        std::list<std::shared_ptr<geometry::triangle_mesh_object>>
            _joint_objects,
            _bone_objects;

    public:
        skeleton_object()
            : geometry_object_base{ geometry_object_type::skeleton }
        {}

        const auto& get_joint_objects() const noexcept {
            return _joint_objects;
        }
        
        const auto& get_bone_objects() const noexcept {
            return _bone_objects;
        }

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