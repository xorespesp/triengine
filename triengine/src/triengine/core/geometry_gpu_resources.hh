#pragma once
#include <triengine/geometry/geometry_object_base.hh>
#include <triengine/geometry/mesh_object.hh>
#include <triengine/geometry/pcd_object.hh>
#include <triengine/geometry/lineset_object.hh>
#include <triengine/utility/noncopyable.hh>

namespace triengine::core
{
    using geometry_gpu_resource_id_t = uint64_t;

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////

    template <typename _Derived>
    class geometry_gpu_rsrc_base
        : utility::noncopyable
    {
    private:
        const geometry_gpu_resource_id_t _id;

    protected:
        geometry_gpu_rsrc_base(geometry_gpu_resource_id_t id) : _id{ id } {}

    public:
        /*virtual*/ ~geometry_gpu_rsrc_base() = default;

        geometry_gpu_resource_id_t get_id() const noexcept { return _id; }

        bool is_valid() const {
            return static_cast<const _Derived*>(this)->is_valid_impl();
        }

        void update(const std::shared_ptr<geometry::geometry_object_base>& geometry_object) {
            static_cast<_Derived*>(this)->update_impl(geometry_object);
        }
    };

    class mesh_geometry_gpu_rsrc
        : public core::geometry_gpu_rsrc_base<mesh_geometry_gpu_rsrc>
    {
    public:
        GLuint vao{};
        GLuint vbo{};
        GLuint ibo{};

    public:
        mesh_geometry_gpu_rsrc();
        ~mesh_geometry_gpu_rsrc();

        // CRTP methods
        bool is_valid_impl() const;
        void update_impl(const std::shared_ptr<geometry::geometry_object_base>& geometry_object);

    }; // class

    using mesh_geometry_gpu_rsrc_ptr = std::shared_ptr<mesh_geometry_gpu_rsrc>;

    class pcd_geometry_gpu_rsrc
        : public core::geometry_gpu_rsrc_base<pcd_geometry_gpu_rsrc>
    {
    public:
        GLuint vao{};
        GLuint vbo{};

    public:
        pcd_geometry_gpu_rsrc();
        ~pcd_geometry_gpu_rsrc();

        // CRTP methods
        bool is_valid_impl() const;
        void update_impl(const std::shared_ptr<geometry::geometry_object_base>& geometry_object);

    }; // class

    using pcd_geometry_gpu_rsrc_ptr = std::shared_ptr<pcd_geometry_gpu_rsrc>;

    class lineset_geometry_gpu_rsrc
        : public core::geometry_gpu_rsrc_base<lineset_geometry_gpu_rsrc>
    {
    public:
        GLuint vao{};
        GLuint vbo{};
        GLuint ibo{};

    public:
        lineset_geometry_gpu_rsrc();
        ~lineset_geometry_gpu_rsrc();

        // CRTP methods
        bool is_valid_impl() const;
        void update_impl(const std::shared_ptr<geometry::geometry_object_base>& geometry_object);

    }; // class

    using lineset_geometry_gpu_rsrc_ptr = std::shared_ptr<lineset_geometry_gpu_rsrc>;

} // namespace