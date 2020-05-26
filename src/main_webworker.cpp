#include "plotter/common.hpp"
#include <emscripten/emscripten.h>
#include <sstream>

namespace {
using namespace nivalis;
Plotter plot;
bool updating = false;
std::string result;
}

extern "C" {

// Syncrhonize functions/environment
void webworker_sync(char* data, int size) {
    {
        std::stringstream ss; ss.write(data, size);
        plot.import_binary_func_and_env(ss);
    }

    plot.render();

    std::ostringstream os;
    plot.export_binary_render_result(os);
    result = os.str();
    emscripten_worker_respond(&result[0], result.size());
}
}
