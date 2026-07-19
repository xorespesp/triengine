#pragma once
#include <triengine/common.h>
#include <triengine/math/math3d.hh>
#include <triengine/utility/noncopyable.hh>

#include <string>

// Refs:
// https://github.com/isl-org/Open3D/blob/main/cpp/open3d/geometry/Geometry.h
// https://github.com/isl-org/Open3D/blob/main/cpp/open3d/geometry/Geometry3D.h

namespace triengine::geometry
{
    using geometry_object_id_t = uint64_t; // unique id

    enum class geometry_object_type {
        mesh,
        pointcloud,
        lineset,
        skeleton,
    };
    
    class geometry_object_base
    {
    private:
        const geometry_object_id_t _id;
        const geometry_object_type _type;
        
        std::string _name;
        mat4_f32 _model{ math::mat4_identity<float>() };

        bool _flag_visible = true;
        mutable bool _flag_dirty = true; // upload to gpu?

    public:
        explicit geometry_object_base(geometry_object_type type);
        virtual ~geometry_object_base() = default;

        geometry_object_id_t get_id() const noexcept {
            return _id;
        }

        geometry_object_type get_type() const noexcept {
            return _type;
        }
        
        bool has_name() const noexcept {
            return !_name.empty();
        }
        
        const std::string& get_name() const {
            return _name;
        }

        void set_name(std::string name) {
            _name = std::move(name);
        }

        bool is_visible() const noexcept {
            return _flag_visible;
        }

        void set_visible(bool visible) noexcept {
            _flag_visible = visible;
        }

        bool is_dirty() const noexcept {
            return _flag_dirty;
        }

        void mark_dirty() const noexcept {
            _flag_dirty = true;
        }

        void clear_dirty() const noexcept {
            _flag_dirty = false;
        }

        const mat4_f32& get_model() const noexcept {
            return _model;
        }

        void set_model(const mat4_f32& model) {
            _model = model;
        }

        virtual void translate(
            const vec3_f32& t,
            const bool relative = false)
        {
            if (relative) {
                mat4_f32 T{ math::mat4_identity<float>() }; // translation matrix
                T.block<3, 1>(0, 3) = t;
                _model = T * _model; // Apply translation to the model matrix
            } else {
                _model.block<3, 1>(0, 3) = t;
            }
        }

        virtual void rotate(
            const mat3_f32& R,
            const bool relative = false)
        {
            if (relative) {
                mat4_f32 T{ math::mat4_identity<float>() };
                T.block<3, 3>(0, 0) = R;
                _model = T * _model; // Apply rotation to the model matrix
            } else {
                _model.block<3, 3>(0, 0) = R;
            }
        }

        virtual void rotate(
            const quat_f32& Q,
            const bool relative = false)
        {
            if (relative) {
                mat4_f32 T{ math::mat4_identity<float>() };
                T.block<3, 3>(0, 0) = Q.toRotationMatrix();
                _model = T * _model; // Apply rotation to the model matrix
            } else {
                _model.block<3, 3>(0, 0) = Q.toRotationMatrix();
            }
        }

        virtual void transform(
            const mat4_f32& T,
            const bool relative = false)
        {
            if (relative) {
                _model = T * _model; // Combine the two model matrices
            } else {
                _model = T;
            }
        }

        /// NOTE: Derived classes are responsible for implementing it.
        virtual void apply_model_in_place() {
            TRIENGINE_PANIC("not implemented");
        }

        /// Returns min bounds for geometry coordinates.
        /// NOTE: Derived classes are responsible for implementing it.
        virtual vec3_f32 get_min_bound() const {
            TRIENGINE_PANIC("not implemented");
            //return math::vec3_all(0.0f);
        }

        /// Returns max bounds for geometry coordinates.
        /// NOTE: Derived classes are responsible for implementing it.
        virtual vec3_f32 get_max_bound() const {
            TRIENGINE_PANIC("not implemented");
            //return math::vec3_all(0.0f);
        }
        
        /// Returns the center of the geometry coordinates.
        /// NOTE: Derived classes are responsible for implementing it.
        virtual vec3_f32 get_center() const {
            TRIENGINE_PANIC("not implemented");
            //return math::vec3_all(0.0f);
        }

    protected:
        /// Compute min bound of a list points.
        vec3_f32 compute_min_bound(
                const std::vector<vec3_f32>& points) const;
        
        /// Compute max bound of a list points.
        vec3_f32 compute_max_bound(
                const std::vector<vec3_f32>& points) const;
        
        /// Computer center of a list of points.
        vec3_f32 compute_center(
                const std::vector<vec3_f32>& points) const;

    protected:
        // NOTE: 
        // - C.67, C.130
        // - https://stackoverflow.com/q/43586090

        geometry_object_base(const geometry_object_base&) = delete;
        geometry_object_base& operator=(const geometry_object_base&) = delete;
        geometry_object_base(geometry_object_base&&) = delete;
        geometry_object_base& operator=(geometry_object_base&&) = delete;

        virtual void clone_impl(geometry_object_base& clone_dst) const = 0;

    }; // class

} // namespace