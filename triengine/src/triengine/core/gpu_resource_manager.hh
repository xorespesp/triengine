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
#include <chrono>
#include <optional>

namespace triengine::core
{
    using gpu_resource_id_t = uint64_t;

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////
    
    template <typename _Derived>
    class geometry_gpu_rsrc_base
        : utility::noncopyable
    {
    private:
        const gpu_resource_id_t _id;

    protected:
        geometry_gpu_rsrc_base(gpu_resource_id_t id) : _id{ id } {}
    
    public:
        /*virtual*/ ~geometry_gpu_rsrc_base() = default;

        gpu_resource_id_t get_id() const noexcept { return _id; }

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
        triangle_mesh_gpu_rsrc();
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
        pcd_gpu_rsrc();
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
        lineset_gpu_rsrc();
        ~lineset_gpu_rsrc();

        // CRTP methods
        bool is_valid_impl() const;
        void update_impl(const std::shared_ptr<geometry::geometry_object_base>& geometry_object);
        
    }; // class

    using lineset_gpu_rsrc_ptr = std::shared_ptr<lineset_gpu_rsrc>;

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////
    
    template <class _Ty, size_t MaxPoolSize>
    class gpu_resource_pooled_allocator final
        : utility::noncopyable
    {
    public:
        using resource_object_type = _Ty;
        using resource_object_ptr = std::shared_ptr<resource_object_type>;

        static_assert(
            std::is_default_constructible_v<resource_object_type>,
            "gpu resource object must be default-constructible"
        );

    private:
        using pending_resource = std::pair<GLsync/* fence */, resource_object_ptr>;

    private:
        std::deque<resource_object_ptr> _avail_objects_pool; // object pool(cache) for allocate performance
        std::list<pending_resource> _pending_objects; // for GPU processing synchronization 

    public:
        gpu_resource_pooled_allocator() {
            TRIENGINE_TRACE("%s() ENTER", __func__);
            // ...
            TRIENGINE_TRACE("%s() LEAVE", __func__);
        }

        ~gpu_resource_pooled_allocator() {
            TRIENGINE_TRACE("%s() ENTER", __func__);
            for (const auto& [fence, _] : _pending_objects) {
                if (fence) {
                    // Wait for GPU to finish commands associated with this fence before deleting the resource.
                    // GL_SYNC_FLUSH_COMMANDS_BIT is used here to ensure that all previously issued GL commands
                    // are flushed to the GPU for execution before checking the fence status. This is crucial
                    // at application shutdown to ensure all resources are truly finished with before their
                    // GPU counterparts are deleted (implicitly by _Ty's destructor via shared_ptr).
                    // A long timeout is used as a safeguard, but ideally, all rendering should have
                    // ceased and flushed naturally before the pool destructor is called.
                    using namespace std::chrono_literals;
                    const GLuint64 wait_max_timeout = static_cast<GLuint64>(std::chrono::duration_cast<std::chrono::nanoseconds>(5s).count());
                    const GLenum wait_res = ::glClientWaitSync(fence, GL_SYNC_FLUSH_COMMANDS_BIT, wait_max_timeout);
                    if (wait_res == GL_WAIT_FAILED) {
                        TRIENGINE_WARN("%s(): Failed to wait for fence %p", __func__, fence);
                    } else if (wait_res == GL_TIMEOUT_EXPIRED) {
                        TRIENGINE_WARN(
                            "%s(): Timeout expired for fence %p: "
                            "Associated resource might not be properly released by GPU before context destruction."
                            , __func__
                            , fence
                        );
                    }

                    ::glDeleteSync(fence);
                }
            }
            TRIENGINE_TRACE("%s() LEAVE", __func__);
        }

        resource_object_ptr allocate()
        {
            // Calling `collect_available_objects()` here (before performing an allocation) 
            // makes recently released objects to be used more quickly.
            // However, if `allocate()` is called extremely frequently within a frame, 
            // and `collect_available_objects()` iterates a long list, this could add up.
            this->collect_available_objects();
            
            // Try reuse object from pool ...
            if (!_avail_objects_pool.empty()) {
                // TODO: Advanced searching logic for resource compatibility. (e.g., matching buffer size, format, ...)
                resource_object_ptr rsrc_ptr = _avail_objects_pool.front();
                _avail_objects_pool.pop_front();
                return rsrc_ptr;
            }

            // Create new one
            return std::make_shared<resource_object_type>();
        }

        void deallocate(resource_object_ptr rsrc_ptr)
        {
            TRIENGINE_ASSERT(rsrc_ptr != nullptr);
            if (rsrc_ptr) {
                // Clean up resource ...

                // Try make reusable ...
                if (_avail_objects_pool.size() + _pending_objects.size() < MaxPoolSize) {
                    // Create a new fence object. This fence is inserted into the GL command stream.
                    // It will be signaled by the GPU after all previously issued GL commands
                    // (up to this point in the command stream) have completed.
                    // This ensures that the GPU is finished using the `rsrc_ptr` before we consider
                    // it safe to reuse or reconfigure its underlying GPU object.
                    GLsync new_fence = ::glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0/* flags (reserved) */);
                    if (new_fence) {
                        _pending_objects.emplace_back(new_fence, std::move(rsrc_ptr));
                    } else {
                        // Failed to create fence object, so this resource cannot be safely tracked for reuse...
                        TRIENGINE_ERROR("Failed to create fence object. resource cannot be reused..");
                    }
                }
            }
        }

        // NOTE: This method MUST be called periodically (e.g., typically at the every frame boundaries)
        //       to check for GPU resources that have finished their work and can be moved back to the available pool.
        size_t collect_available_objects()
        {
            const size_t pending_count = _pending_objects.size();
            size_t collect_count{};

            for (
                auto pend_it = _pending_objects.begin();
                pend_it != _pending_objects.end();
                )
            {
                auto [fence, rsrc_ptr] = *pend_it;

                // Check the status of the current fence.
                const GLenum wait_res = ::glClientWaitSync(fence,
                    0/* flags (NOTE: We do not use `GL_SYNC_FLUSH_COMMANDS_BIT` here, for the performance) */,
                    0/* timeout (NOTE: `0` means non-blocking poll; it checks the status and returns immediately) */
                );

                if (wait_res == GL_ALREADY_SIGNALED ||
                    wait_res == GL_CONDITION_SATISFIED)
                {
                    // The GPU has finished all commands placed in the queue before this fence.
                    // The current `rsrc_ptr` is now safe to be reused.

                    _avail_objects_pool.emplace_back(std::move(rsrc_ptr));
                    ::glDeleteSync(fence);

                    pend_it = _pending_objects.erase(pend_it);
                    ++collect_count;
                }
                else if (wait_res == GL_TIMEOUT_EXPIRED)
                {
                    // The GPU has not yet finished commands up to this fence.
                    // Leave it in the pending queue to be checked again later.

                    //TRIENGINE_TRACE("COLLECT: GPU has not yet finished commands up to this fence. leave it..");
                    ++pend_it;
                }
                else //if (wait_res == GL_WAIT_FAILED)
                {
                    // An error occurred while waiting for the sync object. (this is unusual)
                    // It's safest to remove the problematic fence and its associated resource
                    // from the pending queue to prevent further errors or infinite loops.

                    //TRIENGINE_TRACE("COLLECT: Waiting for fence %p failed with %d. removing from pending list.."
                    //    , fence
                    //    , static_cast<int>(wait_res)
                    //);

                    if (fence) {
                        ::glDeleteSync(fence); // Attempt to delete the problematic fence object.
                    }

                    pend_it = _pending_objects.erase(pend_it); // Remove from pending list.
                }

            } // for

            //if (pending_count) {
            //    TRIENGINE_TRACE("COLLECT: %zu/%zu GPU resource(s) have been recycled.", collect_count, pending_count);
            //}

            return collect_count;
        }

    }; // class

    class gpu_resource_manager
    {
    public:
        static constexpr size_t kMaxPoolSize{ 256 };

    private:
        template <typename _Ty>
        using geometry_resource_map = std::unordered_map<geometry::geometry_object_id_t, _Ty>;

        enum class command_type { 
            acquire_geometry_resource, 
            release_geometry_resource, 
        };

        struct command_data {
            command_type cmd_type{};
            geometry::geometry_object_type obj_type{};
            geometry::geometry_object_id_t obj_id{};
        };

    private:
        gpu_resource_pooled_allocator<triangle_mesh_gpu_rsrc, kMaxPoolSize> _triangle_mesh_rsrc_pool_alloc;
        gpu_resource_pooled_allocator<pcd_gpu_rsrc, kMaxPoolSize> _pcd_rsrc_pool_alloc;
        gpu_resource_pooled_allocator<lineset_gpu_rsrc, kMaxPoolSize> _lineset_rsrc_pool_alloc;

        geometry_resource_map<triangle_mesh_gpu_rsrc_ptr> _triangle_mesh_rsrc_map;
        geometry_resource_map<pcd_gpu_rsrc_ptr> _pcd_rsrc_map;
        geometry_resource_map<lineset_gpu_rsrc_ptr> _lineset_rsrc_map;

        std::deque<command_data> _cmd_q;
        mutable std::mutex _cmd_q_mtx;

    public:
        gpu_resource_manager() = default;
        
        // thread-safe
        void request_acquire_geometry_resource(
            std::shared_ptr<geometry::geometry_object_base> object
        );

        // thread-safe
        void request_release_geometry_resource(
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
        void _acquire_geometry_resource(
            geometry::geometry_object_type obj_type,
            geometry::geometry_object_id_t obj_id
        );

        // NOTE: must be called in render thread
        void _release_geometry_resource(
            geometry::geometry_object_type obj_type,
            geometry::geometry_object_id_t obj_id
        );

    }; // class

} // namespace