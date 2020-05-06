#include "parser.hpp"
#include "plotgui.hpp"
#include "util.hpp"
#include <string>
#include <vector>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cmath>
// #include <fstream>
// #include <map>
// #include "opencv2/core.hpp"
// #include "opencv2/imgproc.hpp"
// #include "opencv2/highgui.hpp"

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

int parse_value(const std::string& cmd, size_t data_size) {
    int idx;
    if (cmd[0] == 'T' || cmd[0] == 'F') {
        idx = 0;
        for (size_t i = 0; i < cmd.size(); ++i) {
            idx <<= 1;
            if (cmd[i] == 'T') idx |= 1;
        }
    } else if (cmd[0] >= '0' && cmd[0] <= '9') {
        idx = std::atoi(cmd.c_str());
        if (idx < 0) idx = data_size + idx - 1;
    } else {
        std::cout << "Error: Invalid command or value " << cmd << "\n";
        return -1;
    }
    return idx;
}
}

int main(int argc, char ** argv) {
    using namespace nivalis;
    std::cout << "Nivalis 0.0.1 (c) Alex Yu 2020\n";

    std::string orig_line, line, var;
    int N;
    Environment env;
    Parser parse;

    std::cout << std::setprecision(16);
    while(true) {
        std::cout << ">>> " << std::flush;
        std::getline(std::cin, orig_line);
        if (!std::cin) break;
        line.clear();
        for (char c : orig_line) {
            if (!std::isspace(c)) line.push_back(c);
        }

        std::string cmd = get_word(orig_line);
        if (cmd == "exit") break;
        else if (cmd == "plot") {
            // Plot function
            util::trim(orig_line);
            PlotGUI gui(env, orig_line);
        }
        else if (cmd == "del") {
            // Delete variable
            util::trim(orig_line);
            if (env.del(orig_line)) {
                std::cout << "del " << orig_line << std::endl;
            } else {
                std::cout << "Undefined variable " << orig_line << "\n";
            }
        } else {
            // Evaluate
            var.clear();

            if (util::is_varname_first(line[0])) {
                size_t pos = 0;
                while(pos < line.size() && util::is_literal(line[pos]))
                    ++pos;
                if (pos < line.size() && line[pos] == '=' &&
                        (pos == line.size()-1 || line[pos+1] != '=')) {
                    // Assignment
                    var = line.substr(0, pos);
                    line = line.substr(pos + 1);
                }
            }

            auto expr = parse(line, env);
            double output;
            if (expr.ast[0] != OpCode::dead &&
                !std::isnan(output = expr(env))) {
                expr.optimize();
                if (var.size()) {
                    env.set(var, output);
                    std::cout << var << " <- " << output << std::endl;
                } else {
                    std::cout << output << std::endl;
                }
            }
        }
    }
    return 0;
}
