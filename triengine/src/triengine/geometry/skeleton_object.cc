#include "skeleton_object.hh"
#include <triengine/utility/debug_utils.hh>

namespace triengine::geometry
{
    namespace
    {
        // Computes the minimal rotation that maps the direction of `v0` onto the
        // direction of `v1` and writes it into `rotation` as a 4x4 matrix.
        // Returns identity when the two directions are nearly collinear.
        inline void _compute_rotation_between_vectors(
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
                rotation = math::mat4_identity<float>();
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

            rotation = math::mat4_identity<float>() + vx + vx2Scaled;
            rotation(3, 3) = 1.0f;
        }

        // Builds the skeleton-local model matrix that places a unit-height bifrustum
        // (height 1, spanning z in [-0.5, 0.5]) as the bone between its two joints.
        // The unit mesh's +Z axis is aligned to point from the parent joint toward
        // the child joint. The bone length is re-derived from the joint positions on
        // every call; it is never assumed to be rigid, so per-frame varying lengths
        // render correctly.
        inline mat4_f32 _compute_bone_local_model(
            const vec3_f32& child_joint_pos,
            const vec3_f32& parent_joint_pos)
        {
            const vec3_f32 axis = child_joint_pos - parent_joint_pos;
            const float    height = axis.norm();
            const vec3_f32 center = (child_joint_pos + parent_joint_pos) * 0.5f;

            mat4_f32 translation{ math::mat4_identity<float>() };
            translation.block<3, 1>(0, 3) = center;

            // Guard the degenerate (near-zero length) bone: a normalized zero axis
            // would yield NaN. The Z-scale below collapses it to a flat disk anyway.
            mat4_f32 rotation{ math::mat4_identity<float>() };
            if (height >= 0.00001f) {
                _compute_rotation_between_vectors(rotation, vec3_f32(0.0f, 0.0f, 1.0f), axis);
            }

            mat4_f32 scale{ math::mat4_identity<float>() };
            scale(2, 2) = height; // stretch the unit-height mesh to the bone length

            return translation * rotation * scale;
        }

    } // namespace

    // --------------------------------------------------------------------------------
    // skeleton_object::builder
    // --------------------------------------------------------------------------------

    skeleton_joint_id_t skeleton_object::builder::add_joint(
        const skeleton_joint_desc_t& joint_desc,
        const skeleton_joint_pose_t& initial_pose)
    {
        const skeleton_joint_id_t id = _joint_entries.size();
        _joint_entries.push_back(joint_entry_t{ joint_desc, initial_pose });
        return id;
    }

    void skeleton_object::builder::add_bone(
        const skeleton_joint_id_t parent_joint,
        const skeleton_joint_id_t child_joint,
        const skeleton_bone_desc_t& bone_desc)
    {
        if (parent_joint >= _joint_entries.size()) {
            TRIENGINE_PANIC("skeleton_object::builder::add_bone: parent_joint is not an id of a previously added joint");
        }

        if (child_joint >= _joint_entries.size()) {
            TRIENGINE_PANIC("skeleton_object::builder::add_bone: child_joint is not an id of a previously added joint");
        }

        // A bone connecting a joint to itself is a topology error.
        if (parent_joint == child_joint) {
            TRIENGINE_PANIC("skeleton_object::builder::add_bone: parent_joint and child_joint must differ");
        }

        _bone_entries.push_back(bone_entry_t{ parent_joint, child_joint, bone_desc });
    }

    std::shared_ptr<skeleton_object> skeleton_object::builder::build() const
    {
        return std::shared_ptr<skeleton_object>{ new skeleton_object{ *this } };
    }

    // --------------------------------------------------------------------------------
    // skeleton_object
    // --------------------------------------------------------------------------------

    skeleton_object::skeleton_object(const builder& bld)
        : geometry_object_base{ geometry_object_type::skeleton }
    {
        // Each joint is a sphere with its radius baked in; the pose only moves it.
        // The initial pose is collected here and applied via `set_pose()` below.
        skeleton_pose_t initial_pose;
        initial_pose.reserve(bld._joint_entries.size());
        for (const auto& j_entry : bld._joint_entries)
        {
            auto j_mesh = geometry::mesh_object::create_sphere(
                j_entry.desc.radius,
                j_entry.desc.resolution
            );
            j_mesh->paint_uniform_color(j_entry.desc.color);
            _joint_objects.emplace_back(j_mesh);
            initial_pose.push_back(j_entry.initial_pose);
        }

        // Each bone is a unit-height bifrustum; set_pose() stretches it along Z via
        // the model matrix, so any bone length (even per-frame varying) is exact.
        _bone_topologies.reserve(bld._bone_entries.size());
        for (const auto& b_entry : bld._bone_entries)
        {
            auto b_mesh = geometry::mesh_object::create_bifrustum(
                b_entry.desc.middle_radius,
                b_entry.desc.parent_cap_radius,
                b_entry.desc.child_cap_radius,
                1.0f/* unit height */,
                b_entry.desc.height_ratio_parent,
                b_entry.desc.resolution
            );
            b_mesh->paint_uniform_color(b_entry.desc.color);
            _bone_objects.emplace_back(b_mesh);
            _bone_topologies.push_back(bone_topology_t{ b_entry.parent_joint, b_entry.child_joint });
        }

        this->set_pose(initial_pose);
    }

    void skeleton_object::clone_impl(geometry_object_base& clone_dst) const
    {
        auto& clone_to = dynamic_cast<skeleton_object&>(clone_dst);

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

        clone_to._bone_topologies = _bone_topologies;
        clone_to._current_pose = _current_pose;

        clone_to.set_visible(this->is_visible());
        clone_to.set_model(this->get_model());
        clone_to._update_child_models(); // re-derive child model matrices for the clone
        clone_to.mark_dirty();
    }

    void skeleton_object::set_pose(const skeleton_pose_t& joint_poses)
    {
        if (joint_poses.size() != _joint_objects.size()) {
            TRIENGINE_PANIC("joint_poses.size() must equal get_joints_count()");
        }
        _current_pose = joint_poses;
        this->_update_child_models();
    }

    void skeleton_object::_update_child_models()
    {
        const mat4_f32& skel_model = this->get_model();

        // Joints: child model = skeleton model * [rotation | position].
        size_t joint_idx = 0;
        for (auto& joint : _joint_objects) {
            const skeleton_joint_pose_t& jp = _current_pose[joint_idx++];
            mat4_f32 local{ math::mat4_identity<float>() };
            local.block<3, 3>(0, 0) = jp.rotation;
            local.block<3, 1>(0, 3) = jp.position;
            joint->set_model(skel_model * local);
        }

        // Bones: child model = skeleton model * bone-local matrix. The bone-local
        // matrix is derived from this bone's two joint positions in `_current_pose`:
        // it translates to their midpoint, rotates the unit mesh's Z axis onto the
        // bone axis, and scales Z by the current bone length.
        // A pose update only rewrites the child meshes' model matrices; it never
        // modifies their GPU vertex data. The renderer applies each model matrix as
        // a per-draw shader uniform, so a new pose takes effect on the next frame
        // with no GPU vertex buffer upload.
        size_t bone_idx = 0;
        for (auto& bone : _bone_objects) {
            const bone_topology_t& topology = _bone_topologies[bone_idx++];
            const vec3_f32& child_pos = _current_pose[topology.child_joint].position;
            const vec3_f32& parent_pos = _current_pose[topology.parent_joint].position;
            bone->set_model(skel_model * _compute_bone_local_model(child_pos, parent_pos));
        }
    }

    void skeleton_object::translate(
        const vec3_f32& t,
        const bool relative)
    {
        geometry_object_base::translate(t, relative);
        this->_update_child_models();
    }

    void skeleton_object::rotate(
        const mat3_f32& R,
        const bool relative)
    {
        geometry_object_base::rotate(R, relative);
        this->_update_child_models();
    }

    void skeleton_object::rotate(
        const quat_f32& Q,
        const bool relative)
    {
        geometry_object_base::rotate(Q, relative);
        this->_update_child_models();
    }

    void skeleton_object::transform(
        const mat4_f32& T,
        const bool relative)
    {
        geometry_object_base::transform(T, relative);
        this->_update_child_models();
    }

    // Folds the skeleton's model matrix into `_current_pose`, then resets model matrix to identity.
    // Joint mesh model matrices are reproduced bit-identically: 
    // with `m = [[r,t],[0,1]]` and joint local `[[R_j,p_j],[0,1]]`, 
    // both `m * local` and `identity * (folded local)` equal `[[r*R_j, r*p_j+t],[0,1]]`.
    // 
    // NOTE:
    // - if model matrix held non-uniform scale/shear, each folded pose rotation (r * R_j)
    //   is no longer strictly orthonormal. Rendering stays correct because it uses
    //   the composed mesh model matrix, not the pose rotation field directly.
    // - bones keep exact position/length/taper, but their roll (cross-section
    //   orientation) may shift: it is re-derived from the transformed bone
    //   direction via the minimal Z-to-axis rotation, which does not commute with
    //   model matrix. Roll is a direction-dependent derived value in this design anyway,
    //   so this is benign and self-consistent.
    void skeleton_object::apply_model_in_place()
    {
        const mat4_f32 m = this->get_model();
        const mat3_f32 r = m.block<3, 3>(0, 0);
        for (auto& jp : _current_pose) {
            jp.position = (m * jp.position.homogeneous()).head<3>();
            jp.rotation = r * jp.rotation;
        }
        this->set_model(math::mat4_identity<float>());
        this->_update_child_models();
    }

    std::vector<vec3_f32> skeleton_object::_collect_skeleton_local_vertices() const
    {
        // Each child mesh model matrix equals:
        //   (skeleton's own model matrix) * (that child's local transform within the skeleton)
        // 
        // Multiplying by the inverse skeleton model matrix cancels the skeleton's own model matrix,
        // leaving every child vertex expressed in skeleton-local space.
        const mat4_f32 inv_skel_model = this->get_model().inverse();

        std::vector<vec3_f32> points;
        const auto append = [&points, &inv_skel_model](
            const std::list<std::shared_ptr<geometry::mesh_object>>& meshes) {
            for (const auto& mesh : meshes) {
                const mat4_f32 child_local = inv_skel_model * mesh->get_model();
                for (const vec3_f32& vpos : mesh->vertex_positions) {
                    points.push_back((child_local * vpos.homogeneous()).head<3>());
                }
            }
        };
        append(_joint_objects);
        append(_bone_objects);
        return points;
    }

    vec3_f32 skeleton_object::get_min_bound() const
    {
        return this->compute_min_bound(this->_collect_skeleton_local_vertices());
    }

    vec3_f32 skeleton_object::get_max_bound() const
    {
        return this->compute_max_bound(this->_collect_skeleton_local_vertices());
    }

    vec3_f32 skeleton_object::get_center() const
    {
        return this->compute_center(this->_collect_skeleton_local_vertices());
    }

} // namespace
