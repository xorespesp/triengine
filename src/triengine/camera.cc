#include "camera.hh"

#include <triengine/utility/logger.hh>
#include <algorithm>
#include <optional>

namespace triengine
{
    namespace
    {
        // Computes the smoothing factor for interpolation in the current frame
        // (how quickly the current values approach the target values).
        // The range is [0.0f, 1.0f], where values closer to 1.0f result in faster convergence.
        inline float compute_smoothing_factor(
            const float damping_factor,
            const float frame_delta_time)
        {
            // To resolve the issue where animation speed changes with frame rate fluctuations (`delta_time`),
            // a frame-rate-independent exponential decay formula is used. This ensures a consistent
            // animation speed regardless of the `delta_time` value.
            // Ref: https://www.rorydriscoll.com/2016/03/07/frame-rate-independent-damping-using-lerp/
            const float factor = 1.0f - std::exp(-damping_factor * frame_delta_time);
            return std::clamp(factor, 0.0f, 1.0f);
        }

        // Linear interpolation for scalars and Eigen vectors
        template<typename T>
        T lerp(const T& a, const T& b, float t) {
            return a * (1.0f - t) + b * t;
        }

    } // namespace

    //-------------------------------------------------------------------------------------------------
    // Abstract Camera Implementations
    //-------------------------------------------------------------------------------------------------

    abstract_camera::abstract_camera(
        camera_type type,
        const vec3_f32& position)
        : _type{ type }
        , _position{ position }
    { }

    void abstract_camera::set_viewport(const view_port& viewport)
    {
        if (_viewport != viewport) {
            TRIENGINE_TRACE("Update camera viewport: %d, %d, %d, %d"
                , viewport.x
                , viewport.y
                , viewport.width
                , viewport.height
            );
        }
        _viewport = viewport;
    }

    bool abstract_camera::project_to_ndc_space(
        const vec3_f32& target_world_pos,
        vec3_f32& ndc_pos_out/* out */) const
    {
        mat4_f32 view, proj;
        this->get_view_projection(view, proj);

        // world space -> view space -> clip space
        const vec4_f32 clip_pos = proj * view * target_world_pos.homogeneous(); // vec4(target_world_pos.xyz, 1.0f)
        if (clip_pos.w() <= 0.0f) {
            return false; // The target point is behind the camera or outside the view frustum
        }

        // clip space -> ndc space (perspective division)
        const vec3_f32 ndc_pos = clip_pos.hnormalized(); // clip_pos.xyz / clip_pos.w
        if (ndc_pos.x() < -1.0f || ndc_pos.x() > 1.0f ||
            ndc_pos.y() < -1.0f || ndc_pos.y() > 1.0f ||
            ndc_pos.z() < -1.0f || ndc_pos.z() > 1.0f) {
            return false; // The target point is outside the view frustum boundary
        }

        ndc_pos_out = ndc_pos;
        return true;
    }

    bool abstract_camera::project_to_viewport_space(
        const vec3_f32& target_world_pos,
        vec2_f32& projected_viewport_pos/* out */) const
    {
        vec3_f32 ndc_pos{};
        if (!this->project_to_ndc_space(target_world_pos, ndc_pos)) {
            return false; // The target point is outside the view frustum
        }

        // ndc space [-1, 1] -> screen space [0, viewport_dimension] (OpenGL convention)
        // Ref: https://www.gamedev.net/forums/topic/685104-ndc-to-pixel-space/
        //      https://msdn.microsoft.com/en-us/library/windows/desktop/bb205126(v=vs.85).aspx
        projected_viewport_pos.x() = ((ndc_pos.x() + 1.0f) * 0.5f * static_cast<float>(_viewport.width)) + _viewport.x;
        projected_viewport_pos.y() = ((ndc_pos.y() + 1.0f) * 0.5f * static_cast<float>(_viewport.height)) + _viewport.y;
        return true;
    }

    bool abstract_camera::unproject_from_viewport_space_with_ndc_z(
        const vec2_f32 target_viewport_pos,
        const float ndc_z,
        vec3_f32& unprojected_world_pos/* out */) const
    {
        if (!_viewport.contains(target_viewport_pos)) {
            return false; // Target position is outside the viewport
        }

        // screen space -> ndc space
        const vec4_f32 ndc_pos{
            /* ndc x */(target_viewport_pos.x() - _viewport.x) / _viewport.width * 2.0f - 1.0f,
            /* ndc y */(target_viewport_pos.y() - _viewport.y) / _viewport.height * 2.0f - 1.0f,
            /* ndc z */ndc_z,
            /* w */1.0f
        };

        mat4_f32 view, proj;
        this->get_view_projection(view, proj);
        const mat4_f32 inv_vp = (proj * view).inverse();

        // ndc space -> world space (homogeneous)
        const vec4_f32 world_pos_homog = inv_vp * ndc_pos;
        if (world_pos_homog.w() <= 0.0f) {
            return false; // The target point is behind the camera or outside the view frustum
        }

        // homogeneous world space -> cartesian world space (perspective division)
        unprojected_world_pos = world_pos_homog.hnormalized(); // world_pos_homog.xyz / world_pos_homog.w
        return true;
    }

    //-------------------------------------------------------------------------------------------------
    // Fly Camera Implementations
    //-------------------------------------------------------------------------------------------------

    fly_camera::fly_camera(
        const vec3_f32& position,
        float yaw,
        float pitch)
        : abstract_camera{ camera_type::fly, position }
        , _yaw{ yaw }, _target_yaw{ yaw }
        , _pitch{ pitch }, _target_pitch{ pitch }
        , _target_position{ position }
    {
        this->update_camera_vectors();
    }

    void fly_camera::get_view_projection(mat4_f32& view, mat4_f32& proj) const
    {
        const auto& vectors = this->_get_camera_vectors();
        const auto& position = this->get_position();

        view = math::lookAt(
            position,
            (position + vectors.front).eval(),
            vectors.up
        );

        proj = math::perspective(
            math::deg2rad(this->get_fovy()),
            this->get_viewport().aspect_ratio(),
            camera_constants::kNearPlane,
            camera_constants::kFarPlane
        );
    }

    void fly_camera::process_keyboard_translation(const camera_movement_type move_dir, const float delta_time)
    {
        // NOTE: For smooth animation, this function modifies the "target value" instead of the actual camera parameters.
        // (The actual camera parameter updates are performed in the update_animation function)

        const float displacement = _opts.movement_speed * delta_time;

        auto& vectors = this->_get_camera_vectors();
        switch (move_dir) {
        case camera_movement_type::forward:  _target_position += vectors.front * displacement; break;
        case camera_movement_type::backward: _target_position -= vectors.front * displacement; break;
        case camera_movement_type::left:     _target_position -= vectors.right * displacement; break;
        case camera_movement_type::right:    _target_position += vectors.right * displacement; break;
        case camera_movement_type::up:       _target_position += camera_constants::kWorldUp * displacement; break;
        case camera_movement_type::down:     _target_position -= camera_constants::kWorldUp * displacement; break;
        default: // Unknown movement type, do nothing
            TRIENGINE_WARN("Unknown camera movement type: %d", static_cast<int>(move_dir));
            break;
        }
    }

    void fly_camera::process_mouse_translation(
        [[maybe_unused]] const vec2_f32 start_viewport_pos,
        [[maybe_unused]] const vec2_f32 end_viewport_pos)
    {
        // do nothing
    }

    void fly_camera::process_mouse_rotation(vec2_f32 move_offset)
    {
        // NOTE: For smooth animation, this function modifies the "target value" instead of the actual camera parameters.
        // (The actual camera parameter updates are performed in the update_animation function)

        // Apply mouse sensitivity to the movement offset
        move_offset *= _opts.mouse_sensitivity;

        _target_yaw += move_offset.x();
        _target_pitch += move_offset.y();

        // constraint pitch
        _target_pitch = std::clamp(
            _target_pitch,
            camera_constants::kMinPitch,
            camera_constants::kMaxPitch
        );
    }

    void fly_camera::process_mouse_zoom(const float zoom_offset)
    {
        // NOTE: For smooth animation, this function modifies the "target value" instead of the actual camera parameters.
        // (The actual camera parameter updates are performed in the update_animation function)

        this->process_mouse_perspective_zoom(zoom_offset);
    }

    void fly_camera::process_mouse_perspective_zoom(const float zoom_offset)
    {
        // NOTE: For smooth animation, this function modifies the "target value" instead of the actual camera parameters.
        // (The actual camera parameter updates are performed in the update_animation function)

        _target_fovy = std::clamp(
            _target_fovy - zoom_offset,
            camera_constants::kMinFovy,
            camera_constants::kMaxFovy
        );
    }

    void fly_camera::update_animation(const float delta_time)
    {
        //
        // Determine how much to interpolate in the current frame (how quickly to approach the target values)
        // and update the camera parameters (gradually and smoothly) based on that interpolation value.
        //

        // Value for how much to interpolate in the current frame.
        const float mixFactor = compute_smoothing_factor(_opts.damping_factor, delta_time);

        // Update position
        this->_set_position(lerp(this->get_position(), _target_position, mixFactor));

        // Update fovy
        this->_set_fovy(lerp(this->get_fovy(), _target_fovy, mixFactor));

        // Update camera vectors (based on yaw and pitch)
        _yaw = lerp(_yaw, _target_yaw, mixFactor);
        _pitch = lerp(_pitch, _target_pitch, mixFactor);
        this->update_camera_vectors();
    }

    void fly_camera::set_position(const vec3_f32& new_position, const bool smooth_update) noexcept
    {
        // Immediately update the current state only if animation is not applied
        if (!smooth_update) {
            this->_set_position(new_position);
        }

        // Always update the target state regardless of animation
        _target_position = new_position;
    }

    void fly_camera::set_direction(const vec3_f32& new_direction, const bool smooth_update) noexcept
    {
        // make sure the direction vector is normalized
        const vec3_f32 norm_dir = new_direction.normalized();

        // Inversely calculate yaw and pitch angles from the new direction vector
        const float new_yaw = math::rad2deg(std::atan2(norm_dir.z(), norm_dir.x()));
        const float new_pitch = std::clamp(math::rad2deg(std::asin(norm_dir.y())),
            camera_constants::kMinPitch,
            camera_constants::kMaxPitch
        );

        // Immediately update the current state only if animation is not applied
        if (!smooth_update) {
            _yaw = new_yaw;
            _pitch = new_pitch;
            this->update_camera_vectors();
        }

        // Always update the target state regardless of animation
        _target_yaw = new_yaw;
        _target_pitch = new_pitch;
    }

    void fly_camera::set_yaw(const float yaw, const bool smooth_update) noexcept
    {
        // Immediately update the current state only if animation is not applied
        if (!smooth_update) {
            _yaw = yaw;
            this->update_camera_vectors();
        }

        // Always update the target state regardless of animation
        _target_yaw = yaw;
    }

    void fly_camera::set_pitch(const float pitch, const bool smooth_update) noexcept
    {
        const float clamped_pitch = std::clamp(
            pitch,
            camera_constants::kMinPitch,
            camera_constants::kMaxPitch
        );

        // Immediately update the current state only if animation is not applied
        if (!smooth_update) {
            _pitch = clamped_pitch;
            this->update_camera_vectors();
        }

        // Always update the target state regardless of animation
        _target_pitch = clamped_pitch;
    }

    void fly_camera::set_fovy(const float fovy, const bool smooth_update) noexcept
    {
        const float clamped_fovy = std::clamp(
            fovy,
            camera_constants::kMinFovy,
            camera_constants::kMaxFovy
        );

        // Immediately update the current state only if animation is not applied
        if (!smooth_update) {
            this->_set_fovy(clamped_fovy);
        }

        // Always update the target state regardless of animation
        _target_fovy = clamped_fovy;
    }

    void fly_camera::update_camera_vectors()
    {
        const vec3_f32 new_front{
            /*x*/std::cos(math::deg2rad(_yaw)) * std::cos(math::deg2rad(_pitch)),
            /*y*/std::sin(math::deg2rad(_pitch)),
            /*z*/std::sin(math::deg2rad(_yaw)) * std::cos(math::deg2rad(_pitch))
        };
        this->_get_camera_vectors().update_vectors(new_front);
    }


    //-------------------------------------------------------------------------------------------------
    // Arcball Camera Implementations
    //-------------------------------------------------------------------------------------------------

    arcball_camera::arcball_camera(
        const vec3_f32& pivot_point,
        float zoom_distance)
        : abstract_camera{ camera_type::arcball, pivot_point + vec3_f32(0.0f, 0.0f, zoom_distance) }
        , _pivot_point{ pivot_point }, _target_pivot_point{ pivot_point }
        , _zoom_distance{ zoom_distance }, _target_zoom_distance{ zoom_distance }
    {
        this->update_camera_vectors();
    }

    void arcball_camera::get_view_projection(mat4_f32& view, mat4_f32& proj) const
    {
        // For arcball cameras, the view matrix is generated using a
        // fixed world-up vector(`kWorldUp`) instead of the camera's up vector to prevent rolling.
        view = math::lookAt(
            this->get_position(),
            _pivot_point,
            camera_constants::kWorldUp
        );

        proj = math::perspective(
            math::deg2rad(this->get_fovy()),
            this->get_viewport().aspect_ratio(),
            camera_constants::kNearPlane,
            camera_constants::kFarPlane
        );
    }

    void arcball_camera::process_keyboard_translation(const camera_movement_type move_dir, const float delta_time)
    {
        // NOTE: For smooth animation, this function modifies the "target value" instead of the actual camera parameters.
        // (The actual camera parameter updates are performed in the update_animation function)

        // Implement panning feature that moves the pivot point.
        // The movement distance is proportional to the camera distance,
        // allowing for fine movement when zoomed in and faster movement when zoomed out.

        constexpr float kPanningSensitivity = 0.5f; // Ratio multiplied by zoom distance to determine panning distance. e.g., 0.5 -> move by 50% of zoom distance.
        const float displacement = std::max(
            (_zoom_distance * kPanningSensitivity) * delta_time,
            0.01f /* minimum guaranteed movement distance */
        );

        // When moving forward/backward, to ensure the target moves horizontally on the XZ plane
        // even if the camera is tilted, calculate the horizontal forward vector from the current camera's Yaw angle.
        const vec3_f32 forwardVectorOnGroundPlane = vec3_f32{
            std::cos(math::deg2rad(_target_yaw)),
            0.0f,
            std::sin(math::deg2rad(_target_yaw))
        }.normalized();

        auto& vectors = this->_get_camera_vectors();
        switch (move_dir) {
        case camera_movement_type::forward:  _target_pivot_point -= forwardVectorOnGroundPlane * displacement; break;
        case camera_movement_type::backward: _target_pivot_point += forwardVectorOnGroundPlane * displacement; break;
        case camera_movement_type::left:     _target_pivot_point -= vectors.right * displacement; break;
        case camera_movement_type::right:    _target_pivot_point += vectors.right * displacement; break;
        case camera_movement_type::up:       _target_pivot_point += camera_constants::kWorldUp * displacement; break;
        case camera_movement_type::down:     _target_pivot_point -= camera_constants::kWorldUp * displacement; break;
        default: // Unknown movement type, do nothing
            TRIENGINE_WARN("Unknown camera movement type: %d", static_cast<int>(move_dir));
            break;
        }
    }

    void arcball_camera::process_mouse_translation(const vec2_f32 start_viewport_pos, const vec2_f32 end_viewport_pos)
    {
        // NOTE: For smooth animation, this function modifies the "target value" instead of the actual camera parameters.
        // (The actual camera parameter updates are performed in the update_animation function)

        std::optional<float> panning_ref_ndc_z;
        if (vec3_f32 ndc_pos;
            this->project_to_ndc_space(
                _pivot_point,
                ndc_pos
            )) {
            panning_ref_ndc_z = std::clamp(ndc_pos.z(), -1.0f, 1.0f);
        }

        if (!panning_ref_ndc_z.has_value()) {
            TRIENGINE_ERROR("Failed to get panning_ref_ndc_z");
            return;
        }

        vec3_f32 start_world_pos;
        vec3_f32 end_world_pos;

        if (!this->unproject_from_viewport_space_with_ndc_z(
            start_viewport_pos,
            panning_ref_ndc_z.value(),
            start_world_pos
        )) {
            TRIENGINE_ERROR("Failed to get start_world_pos");
            return;
        }

        if (!this->unproject_from_viewport_space_with_ndc_z(
            end_viewport_pos,
            panning_ref_ndc_z.value(),
            end_world_pos
        )) {
            TRIENGINE_ERROR("Failed to get end_world_pos");
            return;
        }

        const vec3_f32 translation_offset = end_world_pos - start_world_pos;
        _target_pivot_point -= translation_offset;
    }

    void arcball_camera::process_mouse_rotation(vec2_f32 move_offset)
    {
        // NOTE: For smooth animation, this function modifies the "target value" instead of the actual camera parameters.
        // (The actual camera parameter updates are performed in the update_animation function)

        // Apply mouse sensitivity to the movement offset
        move_offset *= _opts.mouse_sensitivity;

        _target_yaw += move_offset.x();
        _target_pitch -= move_offset.y();

        // constraint pitch
        _target_pitch = std::clamp(
            _target_pitch,
            camera_constants::kMinPitch,
            camera_constants::kMaxPitch
        );
    }

    void arcball_camera::process_mouse_zoom(const float zoom_offset)
    {
        // NOTE: For smooth animation, this function modifies the "target value" instead of the actual camera parameters.
        // (The actual camera parameter updates are performed in the update_animation function)

        _target_zoom_distance = std::clamp(
            _target_zoom_distance - zoom_offset,
            camera_constants::kMinArcballZoomDistance,
            camera_constants::kMaxArcballZoomDistance
        );
    }

    void arcball_camera::process_mouse_perspective_zoom(const float zoom_offset)
    {
        // NOTE: For smooth animation, this function modifies the "target value" instead of the actual camera parameters.
        // (The actual camera parameter updates are performed in the update_animation function)

        _target_fovy = std::clamp(
            _target_fovy - zoom_offset,
            camera_constants::kMinFovy,
            camera_constants::kMaxFovy
        );
    }

    void arcball_camera::update_animation(const float delta_time)
    {
        //
        // Determine how much to interpolate in the current frame (how quickly to approach the target values)
        // and update the camera parameters (gradually and smoothly) based on that interpolation value.
        //

        // Value for how much to interpolate in the current frame.
        const float mixFactor = compute_smoothing_factor(_opts.damping_factor, delta_time);

        // Update fovy
        this->_set_fovy(lerp(this->get_fovy(), _target_fovy, mixFactor));

        _pivot_point = lerp(_pivot_point, _target_pivot_point, mixFactor);
        _zoom_distance = lerp(_zoom_distance, _target_zoom_distance, mixFactor);
        _yaw = lerp(_yaw, _target_yaw, mixFactor);
        _pitch = lerp(_pitch, _target_pitch, mixFactor);
        this->update_camera_vectors();
    }

    void arcball_camera::set_pivot_point(const vec3_f32& new_pivot_point, const bool smooth_update)
    {
        // Immediately update the current state only if animation is not applied
        if (!smooth_update) {
            _pivot_point = new_pivot_point;
            this->update_camera_vectors();
        }

        // Always update the target state regardless of animation
        _target_pivot_point = new_pivot_point;
    }

    void arcball_camera::set_zoom_distance(const float zoom_distance, const bool smooth_update) noexcept
    {
        const float clamped_zoom_distance = std::clamp(
            zoom_distance,
            camera_constants::kMinArcballZoomDistance,
            camera_constants::kMaxArcballZoomDistance
        );

        // Immediately update the current state only if animation is not applied
        if (!smooth_update) {
            _zoom_distance = clamped_zoom_distance;
            this->update_camera_vectors();
        }

        // Always update the target state regardless of animation
        _target_zoom_distance = clamped_zoom_distance;
    }

    void arcball_camera::set_yaw(const float yaw, const bool smooth_update) noexcept
    {
        // Immediately update the current state only if animation is not applied
        if (!smooth_update) {
            _yaw = yaw;
            this->update_camera_vectors();
        }

        // Always update the target state regardless of animation
        _target_yaw = yaw;
    }

    void arcball_camera::set_pitch(const float pitch, const bool smooth_update) noexcept
    {
        const float clamped_pitch = std::clamp(
            pitch,
            camera_constants::kMinPitch,
            camera_constants::kMaxPitch
        );

        // Immediately update the current state only if animation is not applied
        if (!smooth_update) {
            _pitch = clamped_pitch;
            this->update_camera_vectors();
        }

        // Always update the target state regardless of animation
        _target_pitch = clamped_pitch;
    }

    void arcball_camera::set_fovy(const float fovy, const bool smooth_update) noexcept
    {
        const float clamped_fovy = std::clamp(
            fovy,
            camera_constants::kMinFovy,
            camera_constants::kMaxFovy
        );

        // Immediately update the current state only if animation is not applied
        if (!smooth_update) {
            this->_set_fovy(clamped_fovy);
        }

        // Always update the target state regardless of animation
        _target_fovy = clamped_fovy;
    }

    void arcball_camera::update_camera_vectors()
    {
        // Calculate new camera position using spherical coordinates
        const vec3_f32 new_position{
            _pivot_point.x() + _zoom_distance * std::cos(math::deg2rad(_pitch)) * std::cos(math::deg2rad(_yaw)),
            _pivot_point.y() + _zoom_distance * std::sin(math::deg2rad(_pitch)),
            _pivot_point.z() + _zoom_distance * std::cos(math::deg2rad(_pitch)) * std::sin(math::deg2rad(_yaw))
        };

        // Calculate new camera front vector
        const vec3_f32 new_front{ (_pivot_point - new_position).normalized() };

        this->_set_position(new_position);
        this->_get_camera_vectors().update_vectors(new_front);
    }


    //-------------------------------------------------------------------------------------------------
    // Ortho Camera Implementations
    //
    // The orbit controls (rotation, pan, keyboard translation, vector update) mirror arcball_camera.
    // The differences are the orthographic projection in get_view_projection and the zoom semantics
    // (scroll adjusts the view height instead of dollying the eye).
    //-------------------------------------------------------------------------------------------------

    ortho_camera::ortho_camera(
        const vec3_f32& pivot_point,
        float zoom_distance,
        float ortho_view_height)
        : abstract_camera{ camera_type::ortho, pivot_point + vec3_f32(0.0f, 0.0f, zoom_distance) }
        , _pivot_point{ pivot_point }, _target_pivot_point{ pivot_point }
        , _zoom_distance{ zoom_distance }, _target_zoom_distance{ zoom_distance }
        , _ortho_view_height{ ortho_view_height }, _target_ortho_view_height{ ortho_view_height }
    {
        this->update_camera_vectors();
    }

    void ortho_camera::get_view_projection(mat4_f32& view, mat4_f32& proj) const
    {
        // For orbit cameras, the view matrix is generated using a
        // fixed world-up vector(`kWorldUp`) instead of the camera's up vector to prevent rolling.
        view = math::lookAt(
            this->get_position(),
            _pivot_point,
            camera_constants::kWorldUp
        );

        // Build a symmetric orthographic frustum from the current view height and viewport aspect.
        const float aspect = this->get_viewport().aspect_ratio();
        const float half_h = _ortho_view_height * 0.5f;
        const float half_w = half_h * aspect;
        proj = math::ortho(
            -half_w, half_w,
            -half_h, half_h,
            camera_constants::kNearPlane,
            camera_constants::kFarPlane
        );
    }

    void ortho_camera::process_keyboard_translation(const camera_movement_type move_dir, const float delta_time)
    {
        // NOTE: For smooth animation, this function modifies the "target value" instead of the actual camera parameters.
        // (The actual camera parameter updates are performed in the update_animation function)

        // Implement panning feature that moves the pivot point.
        // The movement distance is proportional to the view height,
        // allowing for fine movement when zoomed in and faster movement when zoomed out.

        constexpr float kPanningSensitivity = 0.5f; // Ratio multiplied by view height to determine panning distance.
        const float displacement = std::max(
            (_ortho_view_height * kPanningSensitivity) * delta_time,
            0.01f /* minimum guaranteed movement distance */
        );

        // When moving forward/backward, to ensure the target moves horizontally on the XZ plane
        // even if the camera is tilted, calculate the horizontal forward vector from the current camera's Yaw angle.
        const vec3_f32 forwardVectorOnGroundPlane = vec3_f32{
            std::cos(math::deg2rad(_target_yaw)),
            0.0f,
            std::sin(math::deg2rad(_target_yaw))
        }.normalized();

        auto& vectors = this->_get_camera_vectors();
        switch (move_dir) {
        case camera_movement_type::forward:  _target_pivot_point -= forwardVectorOnGroundPlane * displacement; break;
        case camera_movement_type::backward: _target_pivot_point += forwardVectorOnGroundPlane * displacement; break;
        case camera_movement_type::left:     _target_pivot_point -= vectors.right * displacement; break;
        case camera_movement_type::right:    _target_pivot_point += vectors.right * displacement; break;
        case camera_movement_type::up:       _target_pivot_point += camera_constants::kWorldUp * displacement; break;
        case camera_movement_type::down:     _target_pivot_point -= camera_constants::kWorldUp * displacement; break;
        default: // Unknown movement type, do nothing
            TRIENGINE_WARN("Unknown camera movement type: %d", static_cast<int>(move_dir));
            break;
        }
    }

    void ortho_camera::process_mouse_translation(const vec2_f32 start_viewport_pos, const vec2_f32 end_viewport_pos)
    {
        // NOTE: For smooth animation, this function modifies the "target value" instead of the actual camera parameters.
        // (The actual camera parameter updates are performed in the update_animation function)

        std::optional<float> panning_ref_ndc_z;
        if (vec3_f32 ndc_pos;
            this->project_to_ndc_space(
                _pivot_point,
                ndc_pos
            )) {
            panning_ref_ndc_z = std::clamp(ndc_pos.z(), -1.0f, 1.0f);
        }

        if (!panning_ref_ndc_z.has_value()) {
            TRIENGINE_ERROR("Failed to get panning_ref_ndc_z");
            return;
        }

        vec3_f32 start_world_pos;
        vec3_f32 end_world_pos;

        if (!this->unproject_from_viewport_space_with_ndc_z(
            start_viewport_pos,
            panning_ref_ndc_z.value(),
            start_world_pos
        )) {
            TRIENGINE_ERROR("Failed to get start_world_pos");
            return;
        }

        if (!this->unproject_from_viewport_space_with_ndc_z(
            end_viewport_pos,
            panning_ref_ndc_z.value(),
            end_world_pos
        )) {
            TRIENGINE_ERROR("Failed to get end_world_pos");
            return;
        }

        const vec3_f32 translation_offset = end_world_pos - start_world_pos;
        _target_pivot_point -= translation_offset;
    }

    void ortho_camera::process_mouse_rotation(vec2_f32 move_offset)
    {
        // NOTE: For smooth animation, this function modifies the "target value" instead of the actual camera parameters.
        // (The actual camera parameter updates are performed in the update_animation function)

        // Apply mouse sensitivity to the movement offset
        move_offset *= _opts.mouse_sensitivity;

        _target_yaw += move_offset.x();
        _target_pitch -= move_offset.y();

        // constraint pitch
        _target_pitch = std::clamp(
            _target_pitch,
            camera_constants::kMinPitch,
            camera_constants::kMaxPitch
        );
    }

    void ortho_camera::process_mouse_zoom(const float zoom_offset)
    {
        // NOTE: For smooth animation, this function modifies the "target value" instead of the actual camera parameters.
        // (The actual camera parameter updates are performed in the update_animation function)

        // An orthographic projection is invariant to translation along the view axis, so dollying the
        // eye would not change the apparent size. Instead, "zoom" scales the view volume by adjusting
        // the view height.
        _target_ortho_view_height = std::clamp(
            _target_ortho_view_height - zoom_offset,
            camera_constants::kMinOrthoViewHeight,
            camera_constants::kMaxOrthoViewHeight
        );
    }

    void ortho_camera::process_mouse_perspective_zoom([[maybe_unused]] const float zoom_offset)
    {
        // No-op: an orthographic camera has no field of view to adjust.
    }

    void ortho_camera::update_animation(const float delta_time)
    {
        //
        // Determine how much to interpolate in the current frame (how quickly to approach the target values)
        // and update the camera parameters (gradually and smoothly) based on that interpolation value.
        //

        // Value for how much to interpolate in the current frame.
        const float mixFactor = compute_smoothing_factor(_opts.damping_factor, delta_time);

        // Update view height (the orthographic analogue of fovy/zoom)
        _ortho_view_height = lerp(_ortho_view_height, _target_ortho_view_height, mixFactor);

        _pivot_point = lerp(_pivot_point, _target_pivot_point, mixFactor);
        _zoom_distance = lerp(_zoom_distance, _target_zoom_distance, mixFactor);
        _yaw = lerp(_yaw, _target_yaw, mixFactor);
        _pitch = lerp(_pitch, _target_pitch, mixFactor);
        this->update_camera_vectors();
    }

    void ortho_camera::set_pivot_point(const vec3_f32& new_pivot_point, const bool smooth_update)
    {
        // Immediately update the current state only if animation is not applied
        if (!smooth_update) {
            _pivot_point = new_pivot_point;
            this->update_camera_vectors();
        }

        // Always update the target state regardless of animation
        _target_pivot_point = new_pivot_point;
    }

    void ortho_camera::set_zoom_distance(const float zoom_distance, const bool smooth_update) noexcept
    {
        const float clamped_zoom_distance = std::clamp(
            zoom_distance,
            camera_constants::kMinArcballZoomDistance,
            camera_constants::kMaxArcballZoomDistance
        );

        // Immediately update the current state only if animation is not applied
        if (!smooth_update) {
            _zoom_distance = clamped_zoom_distance;
            this->update_camera_vectors();
        }

        // Always update the target state regardless of animation
        _target_zoom_distance = clamped_zoom_distance;
    }

    void ortho_camera::set_yaw(const float yaw, const bool smooth_update) noexcept
    {
        // Immediately update the current state only if animation is not applied
        if (!smooth_update) {
            _yaw = yaw;
            this->update_camera_vectors();
        }

        // Always update the target state regardless of animation
        _target_yaw = yaw;
    }

    void ortho_camera::set_pitch(const float pitch, const bool smooth_update) noexcept
    {
        const float clamped_pitch = std::clamp(
            pitch,
            camera_constants::kMinPitch,
            camera_constants::kMaxPitch
        );

        // Immediately update the current state only if animation is not applied
        if (!smooth_update) {
            _pitch = clamped_pitch;
            this->update_camera_vectors();
        }

        // Always update the target state regardless of animation
        _target_pitch = clamped_pitch;
    }

    void ortho_camera::set_ortho_view_height(const float ortho_view_height, const bool smooth_update) noexcept
    {
        const float clamped_view_height = std::clamp(
            ortho_view_height,
            camera_constants::kMinOrthoViewHeight,
            camera_constants::kMaxOrthoViewHeight
        );

        // Immediately update the current state only if animation is not applied
        if (!smooth_update) {
            _ortho_view_height = clamped_view_height;
        }

        // Always update the target state regardless of animation
        _target_ortho_view_height = clamped_view_height;
    }

    void ortho_camera::update_camera_vectors()
    {
        // Calculate new camera position using spherical coordinates
        const vec3_f32 new_position{
            _pivot_point.x() + _zoom_distance * std::cos(math::deg2rad(_pitch)) * std::cos(math::deg2rad(_yaw)),
            _pivot_point.y() + _zoom_distance * std::sin(math::deg2rad(_pitch)),
            _pivot_point.z() + _zoom_distance * std::cos(math::deg2rad(_pitch)) * std::sin(math::deg2rad(_yaw))
        };

        // Calculate new camera front vector
        const vec3_f32 new_front{ (_pivot_point - new_position).normalized() };

        this->_set_position(new_position);
        this->_get_camera_vectors().update_vectors(new_front);
    }

    //-------------------------------------------------------------------------------------------------
    // Pinhole Camera Implementations
    //
    // A fixed, calibrated camera at the world origin looking down the camera-frame +z axis. 
    // Geometry is expected to already be in this frame, so the view matrix is the identity 
    // and the projection is built entirely from the intrinsics.
    //-------------------------------------------------------------------------------------------------

    pinhole_camera::pinhole_camera(const intrinsics_t& intrinsics)
        : abstract_camera{ camera_type::pinhole, vec3_f32{ 0.0f, 0.0f, 0.0f } }
        , _intrinsics{ intrinsics }
    {
        // The camera looks straight down the camera-frame +z axis from the origin.
        this->_get_camera_vectors().update_vectors(vec3_f32{ 0.0f, 0.0f, 1.0f });
    }

    void pinhole_camera::get_view_projection(mat4_f32& view, mat4_f32& proj) const
    {
        // The view matrix is the camera extrinsic (the world-to-camera transform).
        view = _extrinsic;

        proj = math::perspective_from_intrinsics(
            static_cast<float>(_intrinsics.fx),
            static_cast<float>(_intrinsics.fy),
            static_cast<float>(_intrinsics.cx),
            static_cast<float>(_intrinsics.cy),
            static_cast<float>(_intrinsics.image_width),
            static_cast<float>(_intrinsics.image_height),
            _near_plane,
            _far_plane
        );

        // The intrinsic projection maps the whole image to NDC [-1, 1]^2, which fills the entire
        // viewport. If the viewport aspect ratio differs from the image aspect ratio, that would
        // stretch the rendered geometry. Instead, scale clip-space x/y so the image keeps its
        // aspect ratio and the unfilled axis is letterboxed (contain fit). When the viewport
        // already matches the image aspect (e.g. an offscreen target at the sensor resolution),
        // this is a no-op.
        const float viewport_aspect = this->get_viewport().aspect_ratio();
        if (viewport_aspect > 0.0f && _intrinsics.image_height > 0) {
            const float image_aspect =
                static_cast<float>(_intrinsics.image_width) / static_cast<float>(_intrinsics.image_height);

            float scale_x = 1.0f, scale_y = 1.0f;
            if (viewport_aspect > image_aspect) {
                scale_x = image_aspect / viewport_aspect; // pillarbox: bars on left/right
            } else {
                scale_y = viewport_aspect / image_aspect; // letterbox: bars on top/bottom
            }

            // Diagonal clip-space scale matrix `diag(scale_x, scale_y, 1, 1)`. 
            // Pre-multiplying the projection by it shrinks the projected x or y 
            // so the image occupies a centered, image-aspect region of the viewport.
            // (the letterbox/pillarbox transform)
            mat4_f32 aspect_fit = math::mat4_identity<float>();
            aspect_fit(0, 0) = scale_x;
            aspect_fit(1, 1) = scale_y;

            // scale the projected clip-space x, y
            proj = aspect_fit * proj;
        }
    }

    void pinhole_camera::set_extrinsic(const mat4_f32& world_to_camera)
    {
        _extrinsic = world_to_camera;

        // NOTE: get_position() must report where the camera is in world space; without this sync it would
        // return a stale (or wrong) value after the pose changes. The extrinsic does not hold that
        // position directly (its translation t is not it), so derive it from [R | t]:
        //   [R | t] maps a world point p to camera space as R*p + t, and the camera position is the
        //   world point mapping to the camera-frame origin: R*c + t = 0  =>  c = -R^T * t (R^-1 = R^T).
        const mat3_f32 R = world_to_camera.block<3, 3>(0, 0);
        const vec3_f32 t = world_to_camera.block<3, 1>(0, 3);
        const vec3_f32 new_camera_position_world = -(R.transpose() * t);
        this->_set_position(new_camera_position_world);

        // The camera looks down the camera-frame +z axis; R^T (camera-to-world rotation) maps it to world space.
        const vec3_f32 new_front_world = R.transpose() * vec3_f32::UnitZ();
        this->_get_camera_vectors().update_vectors(new_front_world);
    }

} // namespace