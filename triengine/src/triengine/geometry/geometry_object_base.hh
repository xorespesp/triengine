#pragma once
#include <triengine/common.h>
#include <triengine/math/math3d.hh>

#include <string>

namespace triengine::geometry
{
    enum class geometry_object_type {
        light_source,
        lineset,
        pointcloud,
        triangle_mesh,
        skeleton,
    };

    class object_base
    {
    public:
        static std::string create_unique_name() {
            static std::atomic_uint32_t cnt_ = 0;
            return utility::string::c_format("object #%lu", cnt_++);
        }

    public:
        object_base() : _name{ create_unique_name() } {}
        object_base(std::string name) : _name{ std::move(name) } {}

        const std::string& get_name() const { return _name; }
        void set_name(std::string name) { 
            _name = std::move(name);
        }

    private:
        std::string _name;
    };

    // Refs:
    // https://github.com/isl-org/Open3D/blob/main/cpp/open3d/geometry/Geometry.h
    // https://github.com/isl-org/Open3D/blob/main/cpp/open3d/geometry/Geometry3D.h

    class geometry_object_base
        : public object_base
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

        /// NOTE: Derived classes are responsible for implementing it.
        virtual void apply_model_in_place() {
            TRIENGINE_PANIC("not implemented");
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