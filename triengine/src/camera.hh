#pragma once
#include "common.h"
#include "math.hh"
#include "misc/string_utils.hh"

namespace triengine
{
    // OpenGL-compatible screen viewport
    // NOTE: Viewport placement is relative to the lower-left corner of the window content area.
    struct view_port
    {
        int x, y; // lower left corner of the viewport rectangle (Unit: [pixel])
        int width, height; // width and height of the viewport. (Unit: [pixel])

        // Screen coordinates are relative to the lower-left corner of the window content area.
        bool contains(vec2_f32 screen_pos) const noexcept {
            const int
                vx = static_cast<int>(std::floor(screen_pos.x())) - x,
                vy = static_cast<int>(std::floor(screen_pos.y())) - y;
            return 0 <= vx && vx < width && 0 <= vy && vy < height;
        }

    }; // struct

    // Camera parameters
    struct camera_parameters
    {
        // Camera Vectors
        vec3_f32
            camera_front{}, // Camera Front
            camera_up{}, // Camera Up
            camera_right{}; // Camera Right

        vec3_f32
            lookat_center{}, // Target position (location of the camera points to)
            world_up{}; // World Up

        // Euler Angles (relative to forward direction)
        float yaw{}, pitch{};

        // Zoom
        float zoom{};

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
            return misc::string::c_format(""
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

    namespace 
    {
        static constexpr float
            kDefaultMouseSensitivity = 0.2f;

        static constexpr float
            kMinFOV = 45.0f,
            kMaxFOV = 145.0f,
            kDefaultFOV = 65.0f;

        static constexpr float
            kMinZoom = 0.1f,
            kMaxZoom = 15.0f,
            kDefaultZoom = kMinZoom + (kMaxZoom - kMinZoom) * 0.15f;

        static constexpr float
            kMinPerspectiveFactor = 0.1f, // Orthographic projection
            kMaxPerspectiveFactor = 1.0f, // Perspective projection
            kDefaulPerspectiveFactor = kMaxPerspectiveFactor;

        static const vec3_f32
            kDefaultLookAtCenter{ 0.0f, 0.0f, 1.5f },
            kDefaultWorldUp{ 0.0f, -1.0f, 0.0f };

        static const camera_parameters
            kDefaultView(
                kDefaultLookAtCenter,
                kDefaultWorldUp,
                0.0f, 0.0f, // yaw, pitch
                kDefaultZoom
            );
    } // namespace

    // Abstract camera class
    // https://learnopengl.com/Getting-started/Coordinate-Systems
    // https://learnopengl.com/Getting-started/Camera
    class camera
    {
    private:

    private:
        camera_parameters _view_param;
        view_port _view_port;
        bool _flag_mirror_mode;

        // Camera options
        float _mouse_sensitivity;
        float _vertical_fov; // Unit: [degree]
        float _perspective_factor; // 0: Orthographic; 1: Full perspective.

    public:
        camera()
            : _view_param{ kDefaultView }
            , _view_port{}
            , _flag_mirror_mode{ false }
            , _mouse_sensitivity{ kDefaultMouseSensitivity }
            , _vertical_fov{ kDefaultFOV }
            , _perspective_factor{ kDefaulPerspectiveFactor }
        { }

        void reset() {
            _view_param = kDefaultView;
            _perspective_factor = kDefaulPerspectiveFactor;
        }

        void set_view_port(view_port viewport) { _view_port = viewport; }
        const view_port& get_view_port() const { return _view_port; }

        void set_vertical_fov(const float fovy_deg) {
            _vertical_fov = fovy_deg;
        }

        void set_mirror_mode(const bool enable) {
            _flag_mirror_mode = enable;
        }

        void set_lookat_center(const vec3_f32& lookat_center) {
            _view_param.lookat_center = lookat_center;
        }

        void get_lookat_center(vec3_f32& lookat_center/* out */) const {
            lookat_center = _view_param.lookat_center;
        }
        
        void get_camera_position(vec3_f32& eye_pos/* out */) const;

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
        float _get_perspective_scaled_zoom() const;
        float _get_perspective_scaled_fovy() const;

    }; // class

} // namespace