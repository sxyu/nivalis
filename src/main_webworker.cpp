#include "plotter/common.hpp"
#include <emscripten/emscripten.h>
#include <sstream>
#include <iostream>

namespace {
using namespace nivalis;
Plotter plot;
bool updating = false;
std::string result;
size_t queue_sz;
std::ostringstream os;
}  // namespace

extern "C" {
// Syncrhonize functions/environment
void webworker_sync(char* data, int size) {
    {
        std::stringstream ss; ss.write(data, size);
        plot.import_binary_func_and_env(ss);
    }

    plot.render();

    os.str("");
    plot.export_binary_render_result(os);
    result = os.str();
    emscripten_worker_respond(&result[0], result.size());
}
}
