#pragma once
#include <triengine/common.h>
#include <triengine/utility/noncopyable.hh>
#include <triengine/utility/debug_utils.hh>

#include <glad/gl.h>
#include <type_traits>
#include <memory>
#include <deque>
#include <list>

namespace triengine::core
{
    template <class _Ty, size_t MaxPoolSize>
    class geometry_gpu_resource_pooled_allocator final
        : utility::noncopyable
    {
    public:
        using resource_object_type = _Ty;
        using resource_object_ptr = std::shared_ptr<resource_object_type>;

        static_assert(
            std::is_default_constructible_v<resource_object_type>,
            "geometry gpu resource object must be default-constructible"
            );

    private:
        using pending_resource = std::pair<GLsync/* fence */, resource_object_ptr>;

    private:
        std::deque<resource_object_ptr> _avail_objects_pool; // object pool(cache) for allocate performance
        std::list<pending_resource> _pending_objects; // for GPU processing synchronization 

    public:
        geometry_gpu_resource_pooled_allocator() {
            TRIENGINE_TRACE("%s() ENTER", __func__);
            // ...
            TRIENGINE_TRACE("%s() LEAVE", __func__);
        }

        ~geometry_gpu_resource_pooled_allocator() {
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
                    }
                    else if (wait_res == GL_TIMEOUT_EXPIRED) {
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
                    }
                    else {
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
            [[maybe_unused]] const size_t pending_count = _pending_objects.size();
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

} // namespace