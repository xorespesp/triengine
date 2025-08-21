#pragma once
#include <triengine/visualization/offscreen_renderer_dx.hh>
#include <triengine/math/math3d.hh>
#include <triengine/io/file_obj_loader.hh>

#include "common.hh"
#include "ipc_proto.hh"
#include "ipc_service.hh"
#include "task_dispatcher.hh"

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
    // Used to submit tasks that MUST be executed in the main render thread
    std::shared_ptr<task_dispatcher> _main_task_dispatcher;

    std::shared_ptr<void> _process_inst_handle;
    std::atomic_bool _run_flag{ false };

    std::shared_ptr<ipc_server> _ipc_srv;
    impl_unique_ptr _impl;

}; // class