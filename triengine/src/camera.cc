#include "camera.hh"

namespace triengine
{
    vec3_f32 camera::get_position() const
    {
        return _camera_params.lookat_center - (_camera_params.camera_front * this->_get_perspective_scaled_zoom());
    }

    void camera::set_position(const vec3_f32& position)
    {
        TRIENGINE_TRACE("set camera position: [%f, %f, %f]"
            , position.x(), position.y(), position.z()
        );

        _camera_params.lookat_center = position + (_camera_params.camera_front * this->_get_perspective_scaled_zoom());
    }

    vec3_f32 camera::get_direction() const
    {
        return _camera_params.camera_front;
    }

    void camera::set_direction(const vec3_f32& direction)
    {
        TRIENGINE_TRACE("set camera direction: [%f, %f, %f]"
            , direction.x(), direction.y(), direction.z()
        );

        // update camera parameters manually
        _camera_params.camera_front = direction.normalized();
        _camera_params.camera_right = _camera_params.camera_front.cross(_camera_params.world_up).normalized();
        _camera_params.camera_up = _camera_params.camera_right.cross(_camera_params.camera_front).normalized();
        _camera_params.yaw = math::rad2deg(std::atan2(_camera_params.camera_front.x(), _camera_params.camera_front.z()));
        _camera_params.pitch = math::rad2deg(std::asin(_camera_params.camera_front.y()));
    }

    void camera::get_view_projection(
        mat4_f32& view_matrix/* out */,
        mat4_f32& projection_matrix/* out */) const
    {
        view_matrix = math::lookAt(
            this->get_position(),
            _camera_params.lookat_center,
            _camera_params.camera_up
        );

        projection_matrix = math::perspective(
            math::deg2rad(this->_get_perspective_scaled_fovy()),
            static_cast<float>(_viewport.width) / static_cast<float>(_viewport.height),
            0.01f,
            200.0f
        );

        if (_flag_mirror_mode) {
            projection_matrix(0, 0) *= -1.0f;
        }
    }

    bool camera::project_to_screen(
        vec2_f32& screen_pos/* out */,
        const vec3_f32& target_world_pos) const
    {
        mat4_f32 view, projection;
        this->get_view_projection(view, projection);

        // world space to clip space
        const vec4_f32 clip_pos = projection * view * vec4_f32{ target_world_pos.x(), target_world_pos.y(), target_world_pos.z(), 1.0f/* w */ };

        // clip space to ndc space
        const vec3_f32 ndc_pos = vec3_f32(clip_pos.head<3>()) / clip_pos.w();
        if (ndc_pos.z() == 0) {
            return false;
        }

        // ndc space to screen space
        screen_pos.x() = (1.0f + ndc_pos.x() / ndc_pos.z()) / 2.0f * _viewport.width + _viewport.x + 0.5f;
        screen_pos.y() = (1.0f + ndc_pos.y() / ndc_pos.z()) / 2.0f * _viewport.height + _viewport.y + 0.5f;
        return true;
    }

    void camera::unproject_from_screen(
        vec3_f32& ray/* out */,
        const vec2_f32 target_screen_pos,
        const float zDepth) const
    {
        mat4_f32 view, projection;
        this->get_view_projection(view, projection);

        const mat4_f32 M = projection * view;
        const mat4_f32 invM = M.inverse();

        const vec4_f32 s{
            /* x */(target_screen_pos.x() - 0.5f - _viewport.x) / _viewport.width * 2.f - 1.f,
            /* y */(target_screen_pos.y() - 0.5f - _viewport.y) / _viewport.height * 2.f - 1.f,
            /* z */0.0f,
            /* w */1.0f
        };

        const vec4_f32 r = s * zDepth;

        ray = vec4_f32(invM * r).head<3>();
    }

    void camera::process_mouse_move_for_rotation(
        const vec2_f32 move_offset)
    {
        float
            xoffset = move_offset.x(),
            yoffset = move_offset.y();

        if (_flag_mirror_mode) {
            xoffset = -xoffset;
        }

        _camera_params.yaw += xoffset * _mouse_sensitivity;
        _camera_params.pitch -= yoffset * _mouse_sensitivity;

        // Make sure that when pitch is out of bounds, screen doesn't get flipped
        _camera_params.pitch = std::clamp(_camera_params.pitch, -89.5f, 89.5f);

        // Update camera vectors(Front, Right, Up) using the updated Euler angles
        _camera_params.update_camera_vectors();
    }

    void camera::process_mouse_move_for_translation(
        const vec2_f32 start_screen_pos,
        const vec2_f32 end_screen_pos)
    {
        vec3_f32 start_ray;
        this->unproject_from_screen(
            start_ray,
            start_screen_pos,
            this->_get_perspective_scaled_zoom()
        );

        vec3_f32 end_ray;
        this->unproject_from_screen(
            end_ray,
            end_screen_pos,
            this->_get_perspective_scaled_zoom()
        );

        const vec3_f32 translation_offset = end_ray - start_ray;
        _camera_params.lookat_center -= translation_offset;
    }

    void camera::process_mouse_scroll_for_zoom(
        const float scroll_yoffset)
    {
        _camera_params.zoom = std::clamp(
            _camera_params.zoom + (scroll_yoffset * _mouse_sensitivity),
            kMinZoom,
            kMaxZoom
        );
        //TRIENGINE_TRACE("update zoom: %f", _view_param.zoom);
    }

    void camera::process_mouse_scroll_for_perspective(
        const float scroll_yoffset)
    {
        _perspective_scale_factor = std::clamp(
            _perspective_scale_factor + (scroll_yoffset * _mouse_sensitivity),
            kMinPerspectiveScaleFactor,
            kMaxPerspectiveScaleFactor
        );
        TRIENGINE_TRACE("update perspective_factor: %f", _perspective_scale_factor);
    }

} // namespace