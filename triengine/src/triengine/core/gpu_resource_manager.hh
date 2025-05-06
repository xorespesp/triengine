#pragma once
#include <triengine/geometry/geometry_object_base.hh>
#include <triengine/geometry/triangle_mesh_object.hh>
#include <triengine/geometry/pcd_object.hh>
#include <triengine/geometry/lineset_object.hh>
#include <triengine/utility/noncopyable.hh>

#include <glad/glad.h>
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <mutex>
#include <string>
#include <optional>

namespace triengine::core
{
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////
    
    template <typename _Derived>
    class geometry_gpu_rsrc_base
        : utility::noncopyable
    {
    private:
        const uint64_t _id;

    public:
        geometry_gpu_rsrc_base(uint64_t id) : _id{ id } {}
        /*virtual*/ ~geometry_gpu_rsrc_base() = default;

        uint64_t get_id() const noexcept { return _id; }

        bool is_valid() const {
            return static_cast<const _Derived*>(this)->is_valid_impl();
        }

        void update(const std::shared_ptr<geometry::geometry_object_base>& geometry_object) {
            static_cast<_Derived*>(this)->update_impl(geometry_object);
        }
    };

    class triangle_mesh_gpu_rsrc
        : public core::geometry_gpu_rsrc_base<triangle_mesh_gpu_rsrc>
    {
    public:
        GLuint vao{};
        GLuint vbo{};
        GLuint ibo{};

    public:
        triangle_mesh_gpu_rsrc(uint32_t id);
        ~triangle_mesh_gpu_rsrc();

        // CRTP methods
        bool is_valid_impl() const;
        void update_impl(const std::shared_ptr<geometry::geometry_object_base>& geometry_object);
        
    }; // class

    using triangle_mesh_gpu_rsrc_ptr = std::shared_ptr<triangle_mesh_gpu_rsrc>;
    
    class pcd_gpu_rsrc
        : public core::geometry_gpu_rsrc_base<pcd_gpu_rsrc>
    {
    public:
        GLuint vao{};
        GLuint vbo{};

    public:
        pcd_gpu_rsrc(uint32_t id);
        ~pcd_gpu_rsrc();

        // CRTP methods
        bool is_valid_impl() const;
        void update_impl(const std::shared_ptr<geometry::geometry_object_base>& geometry_object);
        
    }; // class

    using pcd_gpu_rsrc_ptr = std::shared_ptr<pcd_gpu_rsrc>;

    class lineset_gpu_rsrc
        : public core::geometry_gpu_rsrc_base<lineset_gpu_rsrc>
    {
    public:
        GLuint vao{};
        GLuint vbo{};
        GLuint ibo{};

    public:
        lineset_gpu_rsrc(uint32_t id);
        ~lineset_gpu_rsrc();

        // CRTP methods
        bool is_valid_impl() const;
        void update_impl(const std::shared_ptr<geometry::geometry_object_base>& geometry_object);
        
    }; // class

    using lineset_gpu_rsrc_ptr = std::shared_ptr<lineset_gpu_rsrc>;

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////
    
    class gpu_resource_manager
    {
    public:
        static constexpr size_t kMaxPoolSize{ 64 };

    private:
        template <typename _Ty>
        using geometry_resource_pool = std::deque<_Ty>;

        template <typename _Ty>
        using geometry_resource_map = std::unordered_map<geometry::geometry_object_id_t, _Ty>;

        enum class command_type { 
            create_geometry_resource, 
            delete_geometry_resource, 
        };

        struct command_data {
            command_type cmd_type{};
            geometry::geometry_object_type obj_type{};
            geometry::geometry_object_id_t obj_id{};
        };

    private:
        geometry_resource_pool<triangle_mesh_gpu_rsrc_ptr> _triangle_mesh_rsrc_pool;
        geometry_resource_pool<pcd_gpu_rsrc_ptr> _pcd_rsrc_pool;
        geometry_resource_pool<lineset_gpu_rsrc_ptr> _lineset_rsrc_pool;

        geometry_resource_map<triangle_mesh_gpu_rsrc_ptr> _triangle_mesh_rsrc_map;
        geometry_resource_map<pcd_gpu_rsrc_ptr> _pcd_rsrc_map;
        geometry_resource_map<lineset_gpu_rsrc_ptr> _lineset_rsrc_map;

        std::deque<command_data> _cmd_q;
        mutable std::mutex _cmd_q_mtx;

    public:
        gpu_resource_manager() = default;
        
        void request_create_geometry_resource(
            std::shared_ptr<geometry::geometry_object_base> object
        );

        void request_destroy_geometry_resource(
            std::shared_ptr<geometry::geometry_object_base> object
        );

        // NOTE: must be called in render thread
        void process_pending_requests();

        // NOTE: must be called in render thread
        triangle_mesh_gpu_rsrc_ptr get_triangle_mesh_resource(
            const std::shared_ptr<geometry::triangle_mesh_object>& object
        ) const;

        // NOTE: must be called in render thread
        pcd_gpu_rsrc_ptr get_pcd_resource(
            const std::shared_ptr<geometry::pcd_object>& object
        ) const;

        // NOTE: must be called in render thread
        lineset_gpu_rsrc_ptr get_lineset_resource(
            const std::shared_ptr<geometry::lineset_object>& object
        ) const;

    private:
        // NOTE: must be called in render thread
        void _create_geometry_resource(
            geometry::geometry_object_type obj_type,
            geometry::geometry_object_id_t obj_id
        );

        // NOTE: must be called in render thread
        void _destroy_geometry_resource(
            geometry::geometry_object_type obj_type,
            geometry::geometry_object_id_t obj_id
        );

    }; // class

} // namespace