#pragma once
#include <triengine/geometry/geometry_object_base.hh>
#include <triengine/geometry/triangle_mesh_object.hh>

#include <list>
#include <memory>

namespace triengine::geometry
{
    struct skeleton_joint_info_t
    {
        vec3_f32 position{ math::vec3_all(0.0f) };
        mat3_f32 rotation{ math::mat3_all(0.0f) };
        color3_f32 color{ color3_f32::all(0.0f) };

        skeleton_joint_info_t() = default;
        skeleton_joint_info_t(
            const vec3_f32& position_,
            const mat3_f32& rotation_,
            const color3_f32& color_)
            : position{ position_ }
            , rotation{ rotation_ }
            , color{ color_ }
        { }
    };

    struct skeleton_bone_info_t
    {
        const skeleton_joint_info_t* from_joint{ nullptr };
        const skeleton_joint_info_t* to_joint{ nullptr };
        color3_f32 color{ color3_f32::all(0.0f) };

        skeleton_bone_info_t() = default;
        skeleton_bone_info_t(
            const skeleton_joint_info_t* from_joint_,
            const skeleton_joint_info_t* to_joint_,
            const color3_f32& color_)
            : from_joint{ from_joint_ }
            , to_joint{ to_joint_ }
            , color{ color_ }
        { }
    };

    class skeleton_object
        : public geometry_object_base
    {
    private:
        std::list<std::shared_ptr<geometry::triangle_mesh_object>>
            _joint_objects,
            _bone_objects;

    private:
        std::shared_ptr<geometry_object_base> clone_impl() const override {
            TRIENGINE_PANIC("Not implemented");
            return nullptr;
        }

    public:
        skeleton_object(
            const std::vector<skeleton_joint_info_t>& skeleton_joints,
            const std::vector<skeleton_bone_info_t>& skeleton_bones
        );

        const auto& get_joint_objects() const noexcept {
            return _joint_objects;
        }

        const auto& get_bone_objects() const noexcept {
            return _bone_objects;
        }

        std::shared_ptr<skeleton_object> clone() const {
            return std::static_pointer_cast<skeleton_object>(this->clone_impl());
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

        void apply_model_in_place() override {
            TRIENGINE_PANIC("not implemented");
        }

        vec3_f32 get_min_bound() const override {
            TRIENGINE_PANIC("not implemented");
            return math::vec3_all(0.0f);
        }

        vec3_f32 get_max_bound() const override {
            TRIENGINE_PANIC("not implemented");
            return math::vec3_all(0.0f);
        }
        
        vec3_f32 get_center() const override {
            TRIENGINE_PANIC("not implemented");
            return math::vec3_all(0.0f);
        }

    private:
        // NOTE: In the current implementation, the number of `triangle_mesh_object`s 
        // internally added to a `skeleton_object` must NOT change after object creation.
        // (The `gpu_resource_manager` does not automatically track these changes.)

        void _add_joint_object(
            const vec3_f32& joint_pos,
            const mat3_f32& joint_rot,
            const color3_f32& joint_color
        );

        void _add_bone_object(
            const vec3_f32& from_joint_pos,
            const vec3_f32& to_joint_pos,
            const color3_f32& bone_color
        );

        void _update_objects_model();

    public:
        static std::shared_ptr<skeleton_object> create(
            const std::vector<skeleton_joint_info_t>& skeleton_joints,
            const std::vector<skeleton_bone_info_t>& skeleton_bones)
        {
            return std::make_shared<skeleton_object>(
                skeleton_joints, 
                skeleton_bones
            );
        }

    }; // class

} // namespace