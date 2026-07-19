#pragma once
#include <triengine/geometry/geometry_object_base.hh>
#include <triengine/geometry/mesh_object.hh>

#include <list>
#include <memory>
#include <vector>

namespace triengine::geometry
{
    // A joint's id within a skeleton. Assigned 0, 1, 2, ... in joint-add order
    // and returned by `skeleton_object::builder::add_joint`. Bone endpoints and
    // `skeleton_pose_t` entries are addressed by this same id.
    using skeleton_joint_id_t = size_t/* index */;

    // Immutable visual description of a single joint (a sphere). (fixed at creation)
    struct skeleton_joint_desc_t
    {
        color3_f32 color{ color3_f32::all(0.5f) }; // RGB color painted on the joint mesh(sphere)
        float radius{ 0.020f }; // radius of the joint mesh(sphere)
        int resolution{ 20 }; // joint mesh(sphere) resolution
    };

    // Immutable visual description of a single bone (a bifrustum). (fixed at creation)
    // NOTE: Bones are never assumed to be rigid.
    struct skeleton_bone_desc_t
    {
        color3_f32 color{ color3_f32::all(0.4f) }; // RGB color painted on the bone surface
        float parent_cap_radius{ 0.001f }; // bone mesh(bifrustum) cap radius at the parent-side end
        float child_cap_radius{ 0.003f }; // bone mesh(bifrustum) cap radius at the child-side end
        float middle_radius{ 0.025f }; // bone mesh(bifrustum) radius at the mid junction
        float height_ratio_parent{ 0.1f }; // fraction of bone length occupied by the parent-side frustum [0..1]
        int resolution{ 4 }; // bone mesh(bifrustum) resolution
    };

    // Mutable world-space pose of a single joint. Updated in place every frame.
    struct skeleton_joint_pose_t
    {
        vec3_f32 position{ math::vec3_all(0.0f) }; // joint world position
        mat3_f32 rotation{ math::mat3_identity<float>() }; // joint world rotation
        // NOTE: `apply_model_in_place()` folds the skeleton's model matrix into this field. If that
        //       model matrix held non-uniform scale/shear, the value is no longer strictly
        //       orthonormal. Harmless for rendering (the composed mesh model matrix stays
        //       exact); only matters if this field is read back and assumed a pure rotation.
    };

    // World-space pose of a whole skeleton, one entry per joint, indexed by
    // `skeleton_joint_id_t` (the id space shared with bone endpoints).
    using skeleton_pose_t = std::vector<skeleton_joint_pose_t>;

    class skeleton_object
        : public geometry_object_base
    {
    private:
        // Per-bone topology: the two joint ids each bone connects.
        struct bone_topology_t {
            skeleton_joint_id_t parent_joint{};
            skeleton_joint_id_t child_joint{};
        };

    public:
        // Builds a `skeleton_object`: add all joints first, then connect bones
        // using the joint ids returned by `add_joint`. The built skeleton has a
        // fixed hierarchy; its pose is updated in place via `set_pose()`.
        class builder {
        private:
            friend skeleton_object;

            struct joint_entry_t {
                skeleton_joint_desc_t desc;
                skeleton_joint_pose_t initial_pose;
            };

            struct bone_entry_t {
                skeleton_joint_id_t parent_joint;
                skeleton_joint_id_t child_joint;
                skeleton_bone_desc_t desc;
            };

        public:
            // Adds a joint and returns its id. Ids are assigned 0, 1, 2, ... in
            // the order this is called.
            skeleton_joint_id_t add_joint(
                const skeleton_joint_desc_t& joint_desc,
                const skeleton_joint_pose_t& initial_pose
            );

            // Connects two joints with a bone. Both ids must come from earlier
            // `add_joint()` calls on this builder, and they must differ.
            void add_bone(
                skeleton_joint_id_t parent_joint,
                skeleton_joint_id_t child_joint,
                const skeleton_bone_desc_t& bone_desc
            );

            // Builds the skeleton from the joints and bones added so far.
            std::shared_ptr<skeleton_object> build() const;

        private:
            std::vector<joint_entry_t> _joint_entries;
            std::vector<bone_entry_t> _bone_entries;
        }; // class builder

    private:
        explicit skeleton_object(const builder& bld);

        void clone_impl(geometry_object_base& clone_dst) const override;

    public:
        const auto& get_joint_objects() const noexcept { return _joint_objects; }
        const auto& get_bone_objects() const noexcept { return _bone_objects; }

        size_t get_joints_count() const noexcept { return _joint_objects.size(); }
        size_t get_bones_count() const noexcept { return _bone_objects.size(); }

        // In-place pose update: recomputes child model matrices only, no GPU
        // re-upload. `joint_poses` is indexed by `skeleton_joint_id_t` and its
        // size must equal `get_joints_count()`. Bone lengths are re-derived from
        // joint positions on every call, so they may differ freely between
        // frames (non-rigid skeletons need no special handling).
        void set_pose(const skeleton_pose_t& joint_poses);

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

        void apply_model_in_place() override;

        vec3_f32 get_min_bound() const override;
        vec3_f32 get_max_bound() const override;
        vec3_f32 get_center() const override;

    private:
        // Recomputes every child mesh model matrix as
        // (skeleton's own model matrix) * (local transform from `_current_pose`).
        // The skeleton's own model matrix is only read as input here, never modified.
        // The single code path that drives the children.
        void _update_child_models();

        // Collects all child-mesh vertices in skeleton-local space (i.e. excluding
        // the skeleton's own model matrix). Used by the bound/center queries.
        std::vector<vec3_f32> _collect_skeleton_local_vertices() const;

    private:
        // Renderable child meshes. Their count is fixed at creation.
        // NOTE: In the current implementation, the number of `mesh_object`s
        //       internally added to a `skeleton_object` must NOT change after object creation.
        //       (The `gpu_resource_manager` does not automatically track these changes.)
        std::list<std::shared_ptr<geometry::mesh_object>>
            _joint_objects, // one unit sphere per joint
            _bone_objects;  // one unit-height bifrustum per bone

        // Bone topology, parallel to `_bone_objects`.
        std::vector<bone_topology_t> _bone_topologies;

        // Last applied pose. Single source of truth for all child model matrices.
        skeleton_pose_t _current_pose;

    }; // class skeleton_object

} // namespace
