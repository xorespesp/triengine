#pragma once
#include "../common.h"
#include "../math.hh"

namespace triengine::geometry
{
    enum class geometry_object_type {
        light_source,
        lineset,
        pointcloud,
        triangle_mesh,
        skeleton,
    };

    // Refs:
    // https://github.com/isl-org/Open3D/blob/main/cpp/open3d/geometry/Geometry.h
    // https://github.com/isl-org/Open3D/blob/main/cpp/open3d/geometry/Geometry3D.h

    class geometry_object_base
    {
    private:
        const geometry_object_type _type;
        bool _flag_visible = true;
        mat4_f32 _model = mat4_f32::Identity();

    public:
        geometry_object_base(geometry_object_type type) : _type{ type } {}
        virtual ~geometry_object_base() = default;

        geometry_object_base(const geometry_object_base&) = delete;
        geometry_object_base& operator= (const geometry_object_base&) = delete;

        geometry_object_type get_type() const noexcept {
            return _type;
        }

        bool is_visible() const noexcept {
            return _flag_visible;
        }

        void set_visible(bool visible) noexcept {
            _flag_visible = visible;
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
                mat4_f32 T{ mat4_f32::Identity() }; // translation matrix
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
                mat4_f32 T{ mat4_f32::Identity() };
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
                mat4_f32 T{ mat4_f32::Identity() };
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

    }; // class

} // namespace