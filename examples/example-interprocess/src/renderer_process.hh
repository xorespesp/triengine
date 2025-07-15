#pragma once
#include <triengine/visualization/offscreen_renderer_dx.hh>
#include <triengine/math/math3d.hh>
#include <triengine/io/file_obj_loader.hh>

#include <deque>
#include <cxlib/utils/spin_lock.hh>

#include "common.hh"
#include "ipc_proto.hh"
#include "ipc_service.hh"

class renderer_process
{
private:
    class impl;
    struct impl_deleter { void operator()(impl* p) const; };
    using impl_unique_ptr = std::unique_ptr<impl, impl_deleter>;

public:
    renderer_process();
    ~renderer_process();

    void run();
    void stop();

private:
    void _post_task(std::function<void()> task);
    void _process_pending_tasks(); // MUST be called in the main render thread

private:
    std::shared_ptr<void> _process_inst_handle;
    std::atomic_bool _run_flag{ false };

    std::shared_ptr<ipc_server> _ipc_srv;
    impl_unique_ptr _impl;

    std::deque<std::function<void()>> _task_q;
    mutable _CXLIB utils::spin_lock _task_q_lock;

}; // class