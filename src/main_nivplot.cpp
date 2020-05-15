#include "parser.hpp"
#include "plotter/plot_gui.hpp"
#include "util.hpp"
#include <string>
#include <vector>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <cctype>

namespace {
const char * WIND_NAME = "Nivalis";

std::string get_word(std::string& line) {
    size_t i = 0;
    for (; i < line.size(); ++i) if (!std::isspace(line[i])) break;
    for (; i < line.size(); ++i) if (std::isspace(line[i])) break;
    std::string word = line.substr(0, i);
    line = line.substr(i);
    return word;
}

std::vector<std::string> get_args(std::string& line) {
    std::vector<std::string> args;
    while (line.size()) {
        args.push_back(get_word(line));
    }
    return args;
}

}  // namespace

int main(int argc, char ** argv) {
    using namespace nivalis;
    Environment env;
    PlotGUI gui(env);

    return 0;
}
