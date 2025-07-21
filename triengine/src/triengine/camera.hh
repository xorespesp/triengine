#pragma once
#include <triengine/common.h>
#include <triengine/math/constants.hh>
#include <triengine/math/math3d.hh>
#include <triengine/utility/string_format.hh>

namespace triengine
{
    // OpenGL-compatible screen viewport
    // (NOTE: Viewport placement is relative to the lower-left corner of the window content area)
    struct view_port
    {
        int32_t x{}, y{}; // lower-left corner of the viewport area (Unit: [pixel])
        int32_t width{}, height{}; // viewport width, height (Unit: [pixel])

        view_port() = default;
        view_port(int32_t x, int32_t y, int32_t width, int32_t height)
            : x{ x }, y{ y }, width{ width }, height{ height }
        { }

        // Viewport screen coordinates are relative to the lower-left corner of the window content area.
        bool contains(vec2_f32 viewport_screen_pos) const noexcept {
            const int32_t
                vx = static_cast<int32_t>(std::floor(viewport_screen_pos.x())) - x,
                vy = static_cast<int32_t>(std::floor(viewport_screen_pos.y())) - y;
            return
                0 <= vx && vx < width &&
                0 <= vy && vy < height;
        }

        float aspect_ratio() const noexcept {
            return static_cast<float>(width) / static_cast<float>(height);
        }

        bool operator==(const view_port& rhs) const noexcept {
            return
                x == rhs.x && y == rhs.y &&
                width == rhs.width && height == rhs.height;
        }

        bool operator!=(const view_port& rhs) const noexcept {
            return !(*this == rhs);
        }

    }; // struct

    namespace
    {
        static const vec3_f32
            kWorldUp{ 0.0f, 1.0f, 0.0f };

        static constexpr float
            kMinFovy = 45.0f,
            kMaxFvoy = 145.0f,
            kDefaultFovy = 65.0f;

        static constexpr float
            kMinZoom = 0.01f,
            kMaxZoom = 50.0f,
            kDefaultZoom = 1.0f;

        static constexpr float // 0: Orthographic projection; 1: Perspective projection
            kMinPerspectiveScaleFactor = 0.1f,
            kMaxPerspectiveScaleFactor = 1.0f,
            kDefaulPerspectiveScaleFactor = 0.7f;

        static const vec3_f32
            kDefaultLookAtCenter{ 0.0f, 0.0f, 1.5f };

        static constexpr float
            kDefaultMouseSensitivity = 0.2f;

    } // namespace

    // Camera parameters
    struct camera_parameters
    {
        // Camera Vectors
        vec3_f32
            camera_front{}, // Camera Front
            camera_up{}, // Camera Up
            camera_right{}; // Camera Right

        vec3_f32
            lookat_center{ kDefaultLookAtCenter }, // Target position (location of the camera points to)
            world_up{ kWorldUp }; // World Up

        // Euler Angles (relative to forward direction)
        float yaw{ 0.0f }, pitch{ -10.0f };

        // Zoom
        float zoom{ kDefaultZoom };

        camera_parameters() { this->update_camera_vectors(); }
        camera_parameters(
            const vec3_f32& lookat_center_,
            const vec3_f32& world_up_,
            float yaw_,
            float pitch_,
            float zoom_)
            : lookat_center{ lookat_center_ }
            , world_up{ world_up_ }
            , yaw{ yaw_ }
            , pitch{ pitch_ }
            , zoom{ zoom_ }
        {
            this->update_camera_vectors();
        }

        // Update the rotation vectors based on the updated yaw and pitch values.
        // It needs to be called every time after updating the yaw and pitch values.
        void update_camera_vectors()
        {
            // calculate the new front vector (relative to +Z)
            vec3_f32 new_front;
            new_front.x() = std::sin(math::deg2rad(yaw)) * std::cos(math::deg2rad(pitch));
            new_front.y() = std::sin(math::deg2rad(pitch));
            new_front.z() = std::cos(math::deg2rad(yaw)) * cos(math::deg2rad(pitch));
            camera_front = new_front.normalized();

            // also re-calculate the Right and Up vector
            camera_right = camera_front.cross(world_up).normalized(); // normalize the vectors, because their length gets closer to 0 the more you look up or down which results in slower movement.
            camera_up = camera_right.cross(camera_front).normalized();

            //this->dump();
        }

        std::string dump() const
        {
            return utility::string::c_format(""
                "lookat_center: [%f, %f, %f]\n"
                "camera_front: [%f, %f, %f]\n"
                "camera_right: [%f, %f, %f]\n"
                "camera_up: [%f, %f, %f]\n"
                "world_up: [%f, %f, %f]\n"
                "yaw: %f\n"
                "pitch: %f\n"
                "zoom: %f"
                , lookat_center.x(), lookat_center.y(), lookat_center.z()
                , camera_front.x(), camera_front.y(), camera_front.z()
                , camera_right.x(), camera_right.y(), camera_right.z()
                , camera_up.x(), camera_up.y(), camera_up.z()
                , world_up.x(), world_up.y(), world_up.z()
                , yaw
                , pitch
                , zoom
            );
        }

    }; // struct

    // Abstract camera class
    // https://learnopengl.com/Getting-started/Coordinate-Systems
    // https://learnopengl.com/Getting-started/Camera
    class camera
    {
    private:
        view_port _viewport{};
        camera_parameters _camera_params{};

        float _mouse_sensitivity{ kDefaultMouseSensitivity };
        float _fovy{ kDefaultFovy }; // Vertical FoV. Unit: [degree]
        float _perspective_scale_factor{ kDefaulPerspectiveScaleFactor }; // 0: Orthographic; 1: Full perspective.
        bool _flag_mirror_mode{ false };

    public:
        camera() = default;

        void reset() {
            _camera_params = camera_parameters{};
            _mouse_sensitivity = kDefaultMouseSensitivity;
            _fovy = kDefaultFovy;
            _perspective_scale_factor = kDefaulPerspectiveScaleFactor;
        }

        view_port get_view_port() const noexcept { return _viewport; }
        void set_view_port(view_port viewport) {
            if (_viewport != viewport) {
                TRIENGINE_TRACE("set camera viewport: %d, %d, %d, %d"
                    , viewport.x
                    , viewport.y
                    , viewport.width
                    , viewport.height
                );
            }
            _viewport = viewport;
        }

        const camera_parameters& get_parameters() const noexcept { return _camera_params; }

        float get_mouse_sensitivity() const noexcept { return _mouse_sensitivity; }
        void set_mouse_sensitivity(float sensitivity) noexcept {
            _mouse_sensitivity = sensitivity;
        }

        float get_fovy() const noexcept { return _fovy; }
        void set_fovy(float fovy_deg) noexcept {
            _fovy = fovy_deg;
        }

        bool mirror_mode_enabled() const noexcept { return _flag_mirror_mode; }
        void set_mirror_mode(bool enable) noexcept {
            _flag_mirror_mode = enable;
        }

        float get_perspective_scale_factor() const noexcept { return _perspective_scale_factor; }
        void set_perspective_scale_factor(float scale_factor) noexcept {
            _perspective_scale_factor = scale_factor;
        }

        // Get/Set camera zoom factor
        float get_zoom() const noexcept { return _camera_params.zoom; }
        void set_zoom(float zoom) noexcept {
            _camera_params.zoom = zoom;
        }

        // Get/Set location of the camera points to (lookat target vector)
        const vec3_f32& get_lookat_center() const noexcept { return _camera_params.lookat_center; }
        void set_lookat_center(const vec3_f32& lookat_center) noexcept {
            _camera_params.lookat_center = lookat_center;
        }

        const vec3_f32& get_front() const noexcept { return _camera_params.camera_front; }
        void set_front(const vec3_f32& front) noexcept {
            _camera_params.camera_front = front;
        }

        const vec3_f32& get_right() const noexcept { return _camera_params.camera_right; }
        void set_right(const vec3_f32& right) noexcept {
            _camera_params.camera_right = right;
        }

        const vec3_f32& get_up() const noexcept { return _camera_params.camera_up; }
        void set_up(const vec3_f32& up) noexcept {
            _camera_params.camera_up = up;
        }

        void set_yaw(float deg) noexcept {
            _camera_params.yaw = deg;
        }

        void set_pitch(float deg) noexcept {
            _camera_params.pitch = deg;
        }

        // Get/Set camera world position
        vec3_f32 get_position() const;
        void set_position(const vec3_f32& position);

        // Get/Set camera direction (camera front vector)
        vec3_f32 get_direction() const;
        void set_direction(const vec3_f32& direction);

        // Get/Set world up vector
        void get_view_projection(
            mat4_f32& view_matrix/* out */,
            mat4_f32& projection_matrix/* out */
        ) const;

        // Project 3D point to screen coordinates.
        // Screen coordinates are relative to the lower-left corner of the window content area.
        bool project_to_screen(
            vec2_f32& screen_pos/* out */,
            const vec3_f32& target_world_pos
        ) const;

        // Convert 2D screen coordinate to 3D camera ray.
        void unproject_from_screen(
            vec3_f32& ray/* out */,
            vec2_f32 target_screen_pos,
            float zDepth
        ) const;

        // Processes input received from a mouse input system. 
        // Expects the offset value in both the x and y direction.
        void process_mouse_move_for_rotation(
            vec2_f32 move_offset
        );

        // Processes input received from a mouse input system for camera translation.
        void process_mouse_move_for_translation(
            vec2_f32 start_screen_pos,
            vec2_f32 end_screen_pos
        );

        // Processes input received from a mouse scroll-wheel event.
        void process_mouse_scroll_for_zoom(
            float scroll_yoffset
        );

        // Processes input received from a mouse scroll-wheel event.
        void process_mouse_scroll_for_perspective(
            float scroll_yoffset
        );

    private:
        float _get_perspective_scaled_zoom() const noexcept {
            return _camera_params.zoom / _perspective_scale_factor;
        }

        float _get_perspective_scaled_fovy() const noexcept {
            return _fovy * _perspective_scale_factor;
        }

    }; // class

} // namespace