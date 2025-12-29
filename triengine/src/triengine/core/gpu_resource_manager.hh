#pragma once
#include <triengine/utility/noncopyable.hh>
#include <triengine/core/geometry_gpu_resource_pool_alloc.hh>
#include <triengine/core/geometry_gpu_resources.hh>
#include <triengine/core/texture.hh>

#include <glad/gl.h>
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <mutex>
#include <string>
#include <chrono>
#include <optional>
#include <variant>

namespace triengine::core
{
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////
    
    class gpu_resource_manager final
        : public utility::noncopyable
    {
    public:
        static constexpr size_t kMaxPoolSize{ 256 };

    private:
        template <typename _Ty>
        using geometry_resource_map = std::unordered_map<geometry::geometry_object_id_t, _Ty>;

        enum class command_type { 
            acquire_geometry_resource, 
            release_geometry_resource, 
            create_texture_resource,
            destroy_texture_resource
        };

        struct acquire_geometry_resource_command_data {
            geometry::geometry_object_type obj_type{};
            geometry::geometry_object_id_t obj_id{};
        };

        struct release_geometry_resource_command_data {
            geometry::geometry_object_type obj_type{};
            geometry::geometry_object_id_t obj_id{};
        };

        struct create_texture_resource_command_data {
            texture_handle_t tex_handle{};
            std::shared_ptr<image_buffer> tex_image;
            texture_params_t tex_params{};
        };

        struct destroy_texture_resource_command_data {
            texture_handle_t tex_handle{};
        };

        struct command_data {
            command_type cmd_type{};
            std::variant<
                acquire_geometry_resource_command_data,
                release_geometry_resource_command_data,
                create_texture_resource_command_data,
                destroy_texture_resource_command_data
            > cmd_data;
        };

    private:
        geometry_gpu_resource_pooled_allocator<mesh_geometry_gpu_rsrc, kMaxPoolSize> _mesh_geometry_rsrc_pool_alloc;
        geometry_gpu_resource_pooled_allocator<pcd_geometry_gpu_rsrc, kMaxPoolSize> _pcd_geometry_rsrc_pool_alloc;
        geometry_gpu_resource_pooled_allocator<lineset_geometry_gpu_rsrc, kMaxPoolSize> _lineset_geometry_rsrc_pool_alloc;

        geometry_resource_map<mesh_geometry_gpu_rsrc_ptr> _mesh_geometry_rsrc_map;
        geometry_resource_map<pcd_geometry_gpu_rsrc_ptr> _pcd_geometry_rsrc_map;
        geometry_resource_map<lineset_geometry_gpu_rsrc_ptr> _lineset_geometry_rsrc_map;

        std::unordered_map<texture_handle_t, texture_2d_ptr> _tex2d_rsrc_map;

        std::deque<command_data> _cmd_q;
        mutable std::mutex _cmd_q_mtx;

    public:
        gpu_resource_manager() = default;
        ~gpu_resource_manager();
        
        // thread-safe
        void request_acquire_geometry_resource(
            std::shared_ptr<geometry::geometry_object_base> geometry_object
        );

        // thread-safe
        void request_release_geometry_resource(
            std::shared_ptr<geometry::geometry_object_base> geometry_object
        );

        // thread-safe
        texture_handle_t request_create_texture_resource(
            const std::shared_ptr<image_buffer>& tex_image,
            const texture_params_t& tex_params
        );

        // thread-safe
        void request_destroy_texture_resource(
            texture_handle_t tex_handle
        );

        // NOTE: must be called in render thread
        void process_pending_requests();

        // NOTE: must be called in render thread
        mesh_geometry_gpu_rsrc_ptr get_mesh_geometry_resource(
            const std::shared_ptr<geometry::mesh_object>& geometry_object
        ) const;

        // NOTE: must be called in render thread
        pcd_geometry_gpu_rsrc_ptr get_pcd_geometry_resource(
            const std::shared_ptr<geometry::pcd_object>& geometry_object
        ) const;

        // NOTE: must be called in render thread
        lineset_geometry_gpu_rsrc_ptr get_lineset_geometry_resource(
            const std::shared_ptr<geometry::lineset_object>& geometry_object
        ) const;

        // NOTE: must be called in render thread
        texture_2d_ptr get_texture_2d_resource(
            texture_handle_t tex_handle
        ) const;

    private:
        // NOTE: must be called in render thread
        void _handle_acquire_geometry_resource_command(
            const acquire_geometry_resource_command_data& cmd_data
        );

        // NOTE: must be called in render thread
        void _handle_release_geometry_resource_command(
            const release_geometry_resource_command_data& cmd_data
        );

        // NOTE: must be called in render thread
        void _handle_create_texture_resource_command(
            const create_texture_resource_command_data& cmd_data
        );

        // NOTE: must be called in render thread
        void _handle_destroy_texture_resource_command(
            const destroy_texture_resource_command_data& cmd_data
        );

    }; // class

} // namespace