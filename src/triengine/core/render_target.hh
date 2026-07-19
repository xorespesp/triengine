#pragma once
#include <stdint.h>
#include <glad/gl.h>
#include <GLFW/glfw3.h>

namespace triengine::core
{
    class render_target {
    public:
        render_target() = default;
        virtual ~render_target() = default;

        // Pre-render function (FBO binding or setting window viewport)
        virtual void begin_frame() = 0;

        // Post-render function
        virtual void end_frame() = 0;

        // Viewport size
        virtual int32_t get_width() const = 0;
        virtual int32_t get_height() const = 0;
    };

    class onscreen_render_target
        : public render_target
    {
    public:
        onscreen_render_target(GLFWwindow* windowHandle)
            : _window{ windowHandle }
        { }

        void begin_frame() override {}
        void end_frame() override {}

        int32_t get_width() const override { return -1; }
        int32_t get_height() const override { return -1; }

    private:
        GLFWwindow* _window;
    };

    class offscreen_render_target
        : public render_target
    {
    public:
        offscreen_render_target(int32_t width, int32_t height)
            : _width{ width }
            , _height{ height }
        { }

        void begin_frame() override {}
        void end_frame() override {}

        int32_t get_width() const override { return _width; }
        int32_t get_height() const override { return _height; }

        //unsigned int getColorTextureID() const { return _colorTextureID; }

    private:
        int32_t _width, _height;
    };

} // namespace