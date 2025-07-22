#pragma once
#include <triengine/common.h>
#include <triengine/math/constants.hh>
#include <triengine/math/math3d.hh>
#include <triengine/utility/string_format.hh>
#include <triengine/utility/noncopyable.hh>

namespace triengine
{
    // OpenGL-compatible screen viewport
    // (NOTE: Viewport screen coordinate is relative to the lower-left corner of the window content area)
    struct view_port
    {
        int32_t x{}, y{}; // lower-left corner of the viewport area (Unit: [pixel])
        int32_t width{}, height{}; // viewport width, height (Unit: [pixel])

        view_port() = default;
        view_port(int32_t x, int32_t y, int32_t width, int32_t height)
            : x{ x }, y{ y }, width{ width }, height{ height }
        { }
        view_port(vec2_i32 pos, vec2_i32 size)
            : x{ pos.x() }, y{ pos.y() }, width{ size.x() }, height{ size.y() }
        { }

        bool contains(const vec2_f32 viewport_pos) const noexcept {
            const int32_t
                vx = static_cast<int32_t>(std::floor(viewport_pos.x())) - x,
                vy = static_cast<int32_t>(std::floor(viewport_pos.y())) - y;
            return
                0 <= vx && vx < width &&
                0 <= vy && vy < height;
        }

        float aspect_ratio() const noexcept {
            return (height) // Avoid zero division
                ? static_cast<float>(width) / static_cast<float>(height)
                : 0.0f;
        }

        bool operator==(const view_port& rhs) const noexcept {
            return
                x == rhs.x &&
                y == rhs.y &&
                width == rhs.width &&
                height == rhs.height;
        }

        bool operator!=(const view_port& rhs) const noexcept {
            return !(*this == rhs);
        }

    }; // struct

    /**
     * @brief Converts a 2D screen coordinate from win32 screen coordiate (upper-left origin)
     *        to a local OpenGL viewport coordinate (lower-left origin).
     *
     * @param screen_pos The input coordinate in the Win32 screen space.
     * @param screen_size The total size of the screen or window.
     * @param viewport The target OpenGL viewport.
     * @return The converted coordinate in the local viewport space.
     */
    static inline vec2_f32 win32_screen_pos_2_gl_viewport_pos(
        const vec2_f32 screen_pos,
        const vec2_i32 screen_size,
        const view_port& viewport)
    {
        // Flip the Y-coordinate from a top-left origin (Win32)
        // to a bottom-left origin (OpenGL).
        const float screen_pos_y_flipped = static_cast<float>(screen_size.y()) - screen_pos.y() - 1.0f;

        // Now that the coordinate systems are unified, perform a simple
        // origin translation to make it relative to the viewport.
        return vec2_f32{
            screen_pos.x() - static_cast<float>(viewport.x),
            screen_pos_y_flipped - static_cast<float>(viewport.y)
        };
    }

    namespace camera_constants
    {
        static const vec3_f32
            kWorldUp{ 0.0f, 1.0f, 0.0f };

        constexpr float
            kNearPlane{ 0.1f }, // near plane distance
            kFarPlane{ 100.0f }; // far plane distance

        constexpr float
            kMinFovy{ 1.0f },
            kMaxFovy{ 145.0f },
            kDefaultFovy{ 45.0f };

        // Pitch angle in degrees
        constexpr float
            kMinPitch{ -89.0f },
            kMaxPitch{ 89.0f },
            kDefaultPitch{ 15.0f };

        // Yaw angle in degrees
        constexpr float
            kDefaultYaw{ -90.0f };

        static const vec3_f32
            kDefaultPosition{ 0.0f, 1.0f, 1.5f };

        constexpr float
            kMinArcballZoomDistance{ 1.0f },
            kMaxArcballZoomDistance{ kFarPlane },
            kDefaultArcballZoomDistance{ 5.0f };

        static const vec3_f32
            kDefaultArcballPivotPoint{ 0.0f, 0.0f, 0.0f };

    } // namespace

    // camera coordinate frame vectors
    struct camera_vectors
    {
        vec3_f32 front{};
        vec3_f32 right{};
        vec3_f32 up{};

        camera_vectors() = default;

        void update_vectors(const vec3_f32& new_front)
        {
            this->front = new_front.normalized();

            // Check gimbal lock condition (front vector nearly parallel to world up vector)
            if (std::abs(this->front.dot(camera_constants::kWorldUp)) >= 0.999f)
            {
                // --- Special handling when gimbal is locked ---
                // If the front vector is nearly vertical(nearly parallel to world up vector),
                // Instead of world up, calculate the right vector based on the world X-axis.
                // This way, even if front is (0,1,0), right will have a valid value like (0,0,-1).
                this->right = this->front.cross(vec3_f32(1.0f, 0.0f, 0.0f)).normalized();
                this->up = this->right.cross(this->front).normalized();
            }
            else
            {
                // Otherwise, calculate right and up vectors normally
                this->right = this->front.cross(camera_constants::kWorldUp).normalized();
                this->up = this->right.cross(this->front).normalized();
            }
        }
    };

    // Defines several possible options for camera movement.
    // Used as an abstraction to stay away from window-system specific input methods.
    enum class camera_movement_type {
        forward,
        backward,
        left,
        right,
        up,
        down
    };

    enum class camera_type {
        fly,
        arcball,
    };

    class fly_camera;
    class arcball_camera;

    //-------------------------------------------------------------------------------------------------
    // An abstract camera class that defines a common interface for all camera types.
    // NOTE: abstract camera class can't modify its own camera sates directly(except viewport),
    //       to modify camera state, you should cast it to a specific camera type.
    //-------------------------------------------------------------------------------------------------
    class abstract_camera
        : public utility::noncopyable
    {
    public:
        abstract_camera(camera_type type, const vec3_f32& position);
        virtual ~abstract_camera() = default;

    public:
        //
        // Pure Virtual Functions (must be implemented by derived classes)
        //

        virtual void get_view_projection(mat4_f32& view/* out */, mat4_f32& proj/* out */) const = 0;
        virtual void process_keyboard_translation(camera_movement_type move_dir, float delta_time) = 0;
        virtual void process_mouse_translation(vec2_f32 start_viewport_pos, vec2_f32 end_viewport_pos) = 0;
        virtual void process_mouse_rotation(vec2_f32 move_offset) = 0;
        virtual void process_mouse_zoom(float zoom_offset) = 0;
        virtual void process_mouse_perspective_zoom(float zoom_offset) = 0;

        /**
         * @brief Animation processing function for smooth camera control effects (MUST be called every frame).
         * The core idea of the animation is to separate the "Target State" and "Current State".
         * The `process_xxxxx` family of functions does not directly modify the "Current State" (camera parameters),
         * but instead modifies the "Target State".
         * Then, this function, called every frame, gradually updates the "Current State" camera parameters
         * to approach the "Target State" (linear interpolation).
         */
        virtual void update_animation(float delta_time) = 0;

    public:
        //
        // Common Camera Getters
        //

        camera_type get_type() const noexcept { return _type; }

        const vec3_f32& get_front() const noexcept { return _vectors.front; }
        const vec3_f32& get_right() const noexcept { return _vectors.right; }
        const vec3_f32& get_up() const noexcept { return _vectors.up; }

        const vec3_f32& get_position() const noexcept { return _position; }
        const vec3_f32& get_direction() const noexcept { return this->get_front(); } // syntactic sugar of `get_front()`

        float get_fovy() const noexcept { return _fovy; }

        const view_port& get_viewport() const noexcept { return _viewport; }
        void set_viewport(const view_port& viewport);

        /**
         * @brief Projects a 3D world space point to NDC space coordinates.
         *
         * @param target_world_pos The target 3D point in world space to project.
         * @param ndc_pos_out Output parameter for the calculated NDC coordinates.
         * @return true if the projection is successful, false otherwise
         */
        bool project_to_ndc_space(
            const vec3_f32& target_world_pos,
            vec3_f32& ndc_pos_out/* out */
        ) const;

        /**
         * @brief Projects a 3D world-space point to 2D screen viewport space coordinates.
         *
         * @param target_world_pos The target 3D point in world space to project.
         * @param projected_viewport_pos Output parameter for the calculated 2D screen viewport coordinates.
         * @return true if the projection is successful, false otherwise
         * @note Screen coordinates are relative to the lower-left corner of the window content area.
         */
        bool project_to_viewport_space(
            const vec3_f32& target_world_pos,
            vec2_f32& projected_viewport_pos/* out */
        ) const;

        /**
         * @brief Unprojects a 2D screen viewport space pos & depth value in NDC space to a 3D world space point.
         *
         * @param target_viewport_pos The 2D screen viewport coordinate to unproject.
         * @param unprojected_world_pos Output parameter for the calculated 3D world position.
         * @param ndc_z The depth value in NDC space, ranging from -1.0 (near plane) to 1.0 (far plane).
         * @return true if the unprojection is successful, false otherwise
         * @note Screen coordinates are relative to the lower-left corner of the window content area.
         */
        bool unproject_from_viewport_space_with_ndc_z(
            vec2_f32 target_viewport_pos,
            float ndc_z,
            vec3_f32& unprojected_world_pos/* out */
        ) const;

        // Type Casting Helpers
        template<typename _Ty>
        const _Ty* as() const {
            const bool is_castable = this->_is_castable_to<_Ty>();
            TRIENGINE_ASSERT(is_castable);
            return is_castable
                ? static_cast<const _Ty*>(this)
                : nullptr;
        }

        template<typename _Ty>
        _Ty* as() {
            const bool is_castable = this->_is_castable_to<_Ty>();
            TRIENGINE_ASSERT(is_castable);
            return is_castable
                ? static_cast<_Ty*>(this)
                : nullptr;
        }

    protected:
        //
        // Common Camera Setters (only provided to derived class)
        //

        const camera_vectors& _get_camera_vectors() const noexcept {
            return _vectors;
        }

        camera_vectors& _get_camera_vectors() noexcept {
            return _vectors;
        }

        void _set_position(const vec3_f32& position) noexcept {
            _position = position;
        }

        void _set_fovy(float fovy) noexcept {
            _fovy = std::clamp(fovy, camera_constants::kMinFovy, camera_constants::kMaxFovy);
        }

    private:
        template<typename _Ty>
        inline bool _is_castable_to() const noexcept {
            static_assert(std::is_base_of<abstract_camera, _Ty>::value, "!!");
            if constexpr (std::is_same_v<_Ty, fly_camera>) {
                return _type == camera_type::fly;
            } else if constexpr (std::is_same_v<_Ty, arcball_camera>) {
                return _type == camera_type::arcball;
            } else {
                return false; // Not castable to the requested type
            }
        }

    private:
        // Camera properties
        const camera_type _type;
        vec3_f32 _position{ camera_constants::kDefaultPosition };
        camera_vectors _vectors;
        float _fovy{ camera_constants::kDefaultFovy };

        // Viewport for the camera
        view_port _viewport;
    };

    //-------------------------------------------------------------------------------------------------
    // A fly-through camera that allows free movement in 3D space.
    //-------------------------------------------------------------------------------------------------
    class fly_camera : public abstract_camera
    {
    public:
        // Camera options
        struct options_t
        {
            float movement_speed{ 20.0f }; // Movement speed in units per second
            float mouse_sensitivity{ 0.25f }; // Mouse sensitivity factor
            float damping_factor{ 10.0f }; // Damping factor for smooth animation (higher = faster stop)
        };

    public:
        fly_camera(
            const vec3_f32& position = vec3_f32(0.0f, 1.0f, 5.0f),
            float yaw = camera_constants::kDefaultYaw,
            float pitch = camera_constants::kDefaultPitch
        );

        //
        // Interface Implementations
        //

        void get_view_projection(mat4_f32& view/* out */, mat4_f32& proj/* out */) const override;
        void process_keyboard_translation(camera_movement_type move_dir, float delta_time) override;
        void process_mouse_translation(vec2_f32 start_viewport_pos, vec2_f32 end_viewport_pos) override;
        void process_mouse_rotation(vec2_f32 move_offset) override;
        void process_mouse_zoom(float zoom_offset) override;
        void process_mouse_perspective_zoom(float zoom_offset) override;
        void update_animation(float delta_time) override;

        //
        // Fly camera specific methods
        //

        const options_t& get_options() const noexcept { return _opts; }
        options_t& get_options() noexcept { return _opts; }

        void set_position(const vec3_f32& new_position, bool smooth_update = false) noexcept;
        void set_direction(const vec3_f32& new_direction, bool smooth_update = false) noexcept;

        float get_yaw() const noexcept { return _yaw; }
        void set_yaw(float yaw, bool smooth_update = false) noexcept;

        float get_pitch() const noexcept { return _pitch; }
        void set_pitch(float pitch, bool smooth_update = false) noexcept;

        void set_fovy(float fovy, bool smooth_update = false) noexcept;

    private:
        void update_camera_vectors();

    private:
        // Current state attributes
        float _yaw{ camera_constants::kDefaultYaw }; // Euler angle
        float _pitch{ camera_constants::kDefaultPitch }; // Euler angle

        // Target state attributes for smoothing
        float _target_yaw{ camera_constants::kDefaultYaw };
        float _target_pitch{ camera_constants::kDefaultPitch };
        float _target_fovy{ camera_constants::kDefaultFovy };
        vec3_f32 _target_position{ camera_constants::kDefaultPosition };

        // Camera options
        options_t _opts;
    };


    //-------------------------------------------------------------------------------------------------
    // An arcball camera that orbits around a target point.
    //-------------------------------------------------------------------------------------------------
    class arcball_camera : public abstract_camera
    {
    public:
        // Camera options
        struct options_t
        {
            float mouse_sensitivity{ 0.25f }; // Mouse sensitivity factor
            float damping_factor{ 10.0f }; // Damping factor for smooth animation (higher = faster stop)
        };

    public:
        arcball_camera(
            const vec3_f32& pivot_point = camera_constants::kDefaultArcballPivotPoint,
            float zoom_distance = camera_constants::kDefaultArcballZoomDistance
        );

        //
        // Interface Implementations
        //

        void get_view_projection(mat4_f32& view/* out */, mat4_f32& proj/* out */) const override;
        void process_keyboard_translation(camera_movement_type move_dir, float delta_time) override;
        void process_mouse_translation(vec2_f32 start_viewport_pos, vec2_f32 end_viewport_pos) override;
        void process_mouse_rotation(vec2_f32 move_offset) override;
        void process_mouse_zoom(float zoom_offset) override;
        void process_mouse_perspective_zoom(float zoom_offset) override;
        void update_animation(float delta_time) override;

        //
        // Arcball camera specific methods
        //

        const options_t& get_options() const noexcept { return _opts; }
        options_t& get_options() noexcept { return _opts; }

        const vec3_f32& get_pivot_point() const { return _pivot_point; }
        void set_pivot_point(const vec3_f32& pivot_point, bool smooth_update = false);

        float get_zoom_distance() const noexcept { return _zoom_distance; }
        void set_zoom_distance(float zoom_distance, bool smooth_update = false) noexcept;

        float get_yaw() const noexcept { return _yaw; }
        void set_yaw(float yaw, bool smooth_update = false) noexcept;

        float get_pitch() const noexcept { return _pitch; }
        void set_pitch(float pitch, bool smooth_update = false) noexcept;

        void set_fovy(float fovy, bool smooth_update = false) noexcept;

    private:
        void update_camera_vectors();

    private:
        // Current state attributes
        float _yaw{ camera_constants::kDefaultYaw }; // Euler angle
        float _pitch{ camera_constants::kDefaultPitch }; // Euler angle
        vec3_f32 _pivot_point{ camera_constants::kDefaultArcballPivotPoint };
        float _zoom_distance{ camera_constants::kDefaultArcballZoomDistance };

        // Target state attributes for smoothing
        float _target_yaw{ camera_constants::kDefaultYaw };
        float _target_pitch{ camera_constants::kDefaultPitch };
        vec3_f32 _target_pivot_point{ camera_constants::kDefaultArcballPivotPoint };
        float _target_zoom_distance{ camera_constants::kDefaultArcballZoomDistance };
        float _target_fovy{ camera_constants::kDefaultFovy };

        // Camera options
        options_t _opts;
    };

} // namespace
