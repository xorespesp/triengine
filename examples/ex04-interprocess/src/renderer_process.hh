#pragma once
#include <triengine/visualization/offscreen_renderer_dx.hh>
#include <triengine/math/math3d.hh>
#include <triengine/io/file_obj_loader.hh>

#include <triengine_interop/surface/proto/surface_proto.hh>
#include <triengine_interop/surface/surface_producer.hh>

#include "common.hh"

class renderer_process
{
public:
    renderer_process();
    ~renderer_process();

    void run();
    void stop();

private:
    std::shared_ptr<void> _process_inst_handle;
    std::atomic_bool _run_flag{ false };

    // `impl` implements the producer's session_interface; the producer shares ownership of it
    // (passed to start()). It is declared before the producer so the producer is destroyed
    // first, releasing its reference before this owner drops the last one.
    class impl;
    std::shared_ptr<impl> _imp;
    triengine_interop::surface::surface_producer _producer;

}; // class