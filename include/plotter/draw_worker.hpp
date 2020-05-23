// For internal use only!!
#pragma once

#ifndef _DRAW_WORKER_H_A7F9901D_6F0F_4BC8_AEEA_4C6B7952453D
#define _DRAW_WORKER_H_A7F9901D_6F0F_4BC8_AEEA_4C6B7952453D
#ifndef NIVALIS_EMSCRIPTEN

#include "version.hpp"
#include <condition_variable>
#include <mutex>
#include <atomic>
#include "plotter/common.hpp"

namespace nivalis {
namespace {

bool worker_req_update;
nivalis::Plotter::View worker_view;
bool run_worker_flag;
std::condition_variable worker_cv;
std::mutex worker_mtx;

// Draw worker thread entry point
void draw_worker(nivalis::Plotter& plot) {
    while (true) {
        Plotter::View view;
        {
            std::unique_lock<std::mutex> lock(worker_mtx);
            worker_cv.wait(lock, []{return run_worker_flag;});
            view = worker_view;
            run_worker_flag = false;
        }
        plot.recalc(view);
        std::lock_guard<std::mutex> lock(worker_mtx);
        plot.swap();
        plot.require_update = true;
        worker_req_update = true;
    }
}
// Run worker if not already running AND either:
// this update was not from the worker or
// view has changed since last worker run
void maybe_run_worker(nivalis::Plotter& plot) {
    using namespace nivalis;
    {
        std::lock_guard<std::mutex> lock(worker_mtx);
        run_worker_flag = !(worker_req_update &&
                worker_view == plot.view);
        worker_view = plot.view;
        worker_req_update = false;
    }
    worker_cv.notify_one();
}
}  // namespace
}  // namespace nivalis
#endif // ifndef NIVALIS_ENSCRIPTEN
#endif // ifndef _DRAW_WORKER_H_A7F9901D_6F0F_4BC8_AEEA_4C6B7952453D
