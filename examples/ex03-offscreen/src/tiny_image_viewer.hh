#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <functional> 
#include <stdexcept>
#include <cstdint>

// Platform detection
#if defined(_WIN32)
#  define TINY_VIEWER_PLATFORM_WINDOWS
#elif defined(__linux__)
#  define TINY_VIEWER_PLATFORM_LINUX
#else
#  error "Unsupported platform"
#endif

namespace tiny_viewer {

    enum class image_format {
        rgb,  ///< 3 bytes per pixel (Red, Green, Blue)
        bgr,  ///< 3 bytes per pixel (Blue, Green, Red)
        rgba, ///< 4 bytes per pixel (Red, Green, Blue, Alpha)
        bgra  ///< 4 bytes per pixel (Blue, Green, Red, Alpha) - Common for Windows DIB
    };

    enum class scale_mode {
        fit_window,        ///< Maintain aspect ratio, fit image within window (letterbox/pillarbox if necessary)
        stretch_to_fill,   ///< Ignore aspect ratio, stretch image to fill the entire window
        original_size      ///< Display image at its original resolution, clipping if larger than the window
    };

    enum class flip_axis {
        none,       ///< No flipping
        horizontal, ///< Flip horizontally
        vertical,   ///< Flip vertically
        both        ///< Flip both horizontally and vertically
    };

    enum class key_action {
        press,   ///< A key was pressed.
        release, ///< A key was released.
    };

    enum class special_key {
        none,          ///< No special key, check the character parameter in the callback.
        unknown,       ///< An unknown or unmapped special key.
        escape, enter, tab, backspace, insert, del,
        arrow_right, arrow_left, arrow_down, arrow_up,
        page_up, page_down, home, end,
        f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12,
        left_shift, right_shift, left_control, right_control, left_alt, right_alt
    };

    struct key_modifiers {
        bool shift = false; ///< True if a Shift key is pressed.
        bool ctrl = false; ///< True if a Control key is pressed.
        bool alt = false; ///< True if an Alt (Menu) key is pressed.
    };

    class platform_window;

    class tiny_image_viewer {
    public:
        using key_callback_type = std::function<void(key_action action, special_key skey, uint32_t character_code, key_modifiers modifiers)>;
        using resize_callback_type = std::function<void(int32_t new_client_width, int32_t new_client_height)>;

        tiny_image_viewer(std::string_view title, int32_t initial_width, int32_t initial_height);
        ~tiny_image_viewer();

        tiny_image_viewer(const tiny_image_viewer&) = delete;
        tiny_image_viewer& operator=(const tiny_image_viewer&) = delete;
        tiny_image_viewer(tiny_image_viewer&&) = delete;
        tiny_image_viewer& operator=(tiny_image_viewer&&) = delete;

        bool create_window();
        void show_window();
        void hide_window();
        bool is_open() const;
        void process_events();
        void close_window();

        void set_image(const uint8_t* pixel_data, int32_t image_width, int32_t image_height, image_format pixel_format);
        void clear_image();

        void set_scale_mode(scale_mode mode);
        scale_mode get_scale_mode() const;

        void set_flip_axis(flip_axis axis);
        flip_axis get_flip_axis() const;

        void run();
        void request_redraw();

        void set_key_callback(key_callback_type callback);
        void set_resize_callback(resize_callback_type callback);

        friend class windows_platform_window;

    protected:
        void _invoke_key_callback(key_action action, special_key skey, uint32_t character_code, key_modifiers modifiers);
        void _invoke_resize_callback(int32_t new_client_width, int32_t new_client_height);

    private:
        void _process_image_for_display();
        void _convert_to_bgra(
            const uint8_t* input_data,
            int32_t img_w, int32_t img_h,
            int32_t num_channels,
            image_format input_fmt,
            std::vector<uint8_t>& output_bgra_data
        );
        void _flip_image_data_inplace(
            std::vector<uint8_t>& image_data_to_flip,
            int32_t img_w, int32_t img_h,
            int32_t channels_per_pixel,
            flip_axis axis
        );

    private:
        struct image_buffer {
            std::vector<uint8_t> _source_data;
            int32_t _source_width = 0;
            int32_t _source_height = 0;
            image_format _source_format = image_format::bgra;
            int32_t _source_channels = 0;
            std::vector<uint8_t> _display_buffer_bgra;
            int32_t _display_buffer_width = 0;
            int32_t _display_buffer_height = 0;
            void reset();
            bool is_valid_source() const { return !_source_data.empty() && _source_width > 0 && _source_height > 0; }
        };

        std::unique_ptr<platform_window> _platform_window;
        image_buffer _current_image_buffer;
        scale_mode _current_scale_mode = scale_mode::fit_window;
        flip_axis _current_flip_axis = flip_axis::none;
        std::string _window_title; // Already std::string, good for ANSI
        int32_t _initial_window_width;
        int32_t _initial_window_height;
        bool _is_window_actually_created = false;
        key_callback_type _key_callback;
        resize_callback_type _resize_callback;
    };

} // namespace tiny_viewer

/*
int main()
{
    static const auto create_test_image_data =
        [](int32_t image_width, int32_t image_height, float time_elapsed) -> std::vector<uint8_t>
        {
            size_t buffer_size = static_cast<size_t>(image_width) * static_cast<size_t>(image_height) * 3;
            std::vector<uint8_t> image_pixel_data(buffer_size);
            for (int32_t y_coord = 0; y_coord < image_height; ++y_coord) {
                for (int32_t x_coord = 0; x_coord < image_width; ++x_coord) {
                    size_t pixel_index = (static_cast<size_t>(y_coord) * image_width + x_coord) * 3;
                    image_pixel_data[pixel_index + 0] = static_cast<uint8_t>(128 + 127 * std::sin(time_elapsed + x_coord * 0.1f + y_coord * 0.05f));
                    image_pixel_data[pixel_index + 1] = static_cast<uint8_t>(128 + 127 * std::cos(time_elapsed + y_coord * 0.1f));
                    image_pixel_data[pixel_index + 2] = static_cast<uint8_t>(50 + 50 * std::sin(time_elapsed));
                }
            }
            return image_pixel_data;
        };

    try {
        tiny_viewer::tiny_image_viewer viewer("Tiny Viewer Test", 700, 500);

        if (!viewer.create_window()) {
            throw std::runtime_error{ "Failed to create window for non-blocking example." };
        }
        viewer.show_window();
        viewer.set_scale_mode(tiny_viewer::scale_mode::stretch_to_fill);
        viewer.set_flip_axis(tiny_viewer::flip_axis::vertical);

        viewer.set_resize_callback(
            [&viewer](const int32_t new_width, const int32_t new_height)
            {
                TRIENGINE_TRACE("viewer window resize: %dx%d", new_width, new_height);
            }
        );

        viewer.set_key_callback(
            [&viewer](
                const tiny_viewer::key_action action,
                const tiny_viewer::special_key skey,
                const uint32_t character_code,
                const tiny_viewer::key_modifiers mods)
            {
                std::stringstream dump;

                dump << "Action: " << (action == tiny_viewer::key_action::press) ? "Pressed" : "Released";

                if (skey != tiny_viewer::special_key::none) {
                    // For a real application, you'd map special_key enum to string
                    dump << " Special KeyCode: " << static_cast<int>(skey);
                }

                if (character_code != 0 && skey == tiny_viewer::special_key::none) { // Character is primary if not a special key mapped
                    dump << " Character: '" << static_cast<char>(character_code) << "' (code: " << character_code << ")";
                }

                dump << " Modifiers: ";
                if (mods.shift) dump << "[Shift] ";
                if (mods.ctrl)  dump << "[Ctrl] ";
                if (mods.alt)   dump << "[Alt] ";

                TRIENGINE_TRACE("%s", dump.str().c_str());

                if (action == tiny_viewer::key_action::press && skey == tiny_viewer::special_key::escape) {
                    TRIENGINE_INFO("Escape pressed!");
                    viewer.close_window();
                }
            }
        );

        float time_val = 0.0f;
        auto last_update_time = std::chrono::steady_clock::now();

        std::cout << "Running non-blocking example. Close the window to exit this loop." << std::endl;
        while (viewer.is_open())
        {
            viewer.process_events();

            // Your application logic here (e.g., game updates, simulations, ...)

            const auto current_time = std::chrono::steady_clock::now();
            const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - last_update_time).count();
            if (elapsed_ms > 33) { // Update at ~30 FPS
                time_val += 0.033f * (elapsed_ms / 33.0f); // Simple time update
                std::vector<uint8_t> frame_pixel_data = create_test_image_data(150, 100, time_val);
                viewer.set_image(frame_pixel_data.data(), 150, 100, tiny_viewer::image_format::rgb);
                last_update_time = current_time;
            }

            // Add a small sleep to prevent busy-waiting if your app logic is very fast
            // and no vsync or other frame limiting is in place.
            // For a pure event-driven app, this might not be needed if process_events handles yielding.
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        std::cout << "Non-blocking example finished." << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
*/