#pragma once
#include <triengine/geometry/geometry_object_base.hh>
#include <triengine/geometry/mesh_object.hh>

#include <list>
#include <memory>

namespace triengine::geometry
{
    struct skeleton_joint_info_t
    {
        vec3_f32 position{ math::vec3_all(0.0f) };
        mat3_f32 rotation{ math::mat3_all(0.0f) };
        color3_f32 color{ color3_f32::all(0.0f) };
        float radius{ 0.020f };
        int resolution{ 20 };

        skeleton_joint_info_t() = default;
        skeleton_joint_info_t(
            const vec3_f32& position_,
            const mat3_f32& rotation_,
            const color3_f32& color_,
            float radius_ = 0.020f,
            int resolution_ = 20)
            : position{ position_ }
            , rotation{ rotation_ }
            , color{ color_ }
            , radius{ radius_ }
            , resolution{ resolution_ }
        { }
    };

    struct skeleton_bone_info_t
    {
        const skeleton_joint_info_t* from_joint{ nullptr };
        const skeleton_joint_info_t* to_joint{ nullptr };
        color3_f32 color{ color3_f32::all(0.0f) };
        float parent_cap_radius{ 0.001f };
        float child_cap_radius{ 0.003f };
        float middle_radius{ 0.025f };
        float height_ratio_parent{ 0.1f };
        int resolution{ 4 };

        skeleton_bone_info_t() = default;
        skeleton_bone_info_t(
            const skeleton_joint_info_t* from_joint_,
            const skeleton_joint_info_t* to_joint_,
            const color3_f32& color_,
            float parent_cap_radius_ = 0.001f,
            float child_cap_radius_ = 0.003f,
            float middle_radius_ = 0.025f,
            float height_ratio_parent_ = 0.1f,
            int resolution_ = 4)
            : from_joint{ from_joint_ }
            , to_joint{ to_joint_ }
            , color{ color_ }
            , parent_cap_radius{ parent_cap_radius_ }
            , child_cap_radius{ child_cap_radius_ }
            , middle_radius{ middle_radius_ }
            , height_ratio_parent{ height_ratio_parent_ }
            , resolution{ resolution_ }
        { }
    };

    class skeleton_object
        : public geometry_object_base
    {
    private:
        std::list<std::shared_ptr<geometry::mesh_object>>
            _joint_objects,
            _bone_objects;

    private:
        void clone_impl(geometry_object_base& clone_dst) const override
        {
            auto& clone_to = dynamic_cast<std::decay_t<decltype(*this)>&>(clone_dst);

            // Perform deep copy of joint_objects and bone_objects ...
            clone_to._joint_objects.clear();
            for (const auto& joint : _joint_objects) {
                auto cloned_joint = joint->clone();
                TRIENGINE_ASSERT(cloned_joint != nullptr);
                clone_to._joint_objects.push_back(cloned_joint);
            }

            clone_to._bone_objects.clear();
            for (const auto& bone : _bone_objects) {
                auto cloned_bone = bone->clone();
                TRIENGINE_ASSERT(cloned_bone != nullptr);
                clone_to._bone_objects.push_back(cloned_bone);
            }

            clone_to.set_visible(this->is_visible());
            clone_to.set_model(this->get_model());
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

        void clone_to(skeleton_object& clone_dst) const {
            this->clone_impl(clone_dst);
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
            //return math::vec3_all(0.0f);
        }

        vec3_f32 get_max_bound() const override {
            TRIENGINE_PANIC("not implemented");
            //return math::vec3_all(0.0f);
        }
        
        vec3_f32 get_center() const override {
            TRIENGINE_PANIC("not implemented");
            //return math::vec3_all(0.0f);
        }

    private:
        // NOTE: In the current implementation, the number of `mesh_object`s 
        // internally added to a `skeleton_object` must NOT change after object creation.
        // (The `gpu_resource_manager` does not automatically track these changes.)

        void _add_joint_object(
            const vec3_f32& joint_pos,
            const mat3_f32& joint_rot,
            const color3_f32& joint_color,
            float joint_radius,
            int joint_resolution
        );

        void _add_bone_object(
            const vec3_f32& from_joint_pos,
            const vec3_f32& to_joint_pos,
            const color3_f32& bone_color,
            float bone_parent_cap_radius,
            float bone_child_cap_radius,
            float bone_middle_radius,
            float bone_height_ratio_parent,
            int bone_resolution
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