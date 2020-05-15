#include "version.hpp"

#include <string>
#include <vector>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <cctype>

#ifdef ENABLE_NIVALIS_READLINE_SHELL
#include <readline/readline.h>
#include <readline/history.h>
#endif

#include "version.hpp"
#include "util.hpp"
#include "shell.hpp"

int main(int argc, char ** argv) {
    using namespace nivalis;
    Environment env;
    Shell shell(env, std::cout);

    std::cout << std::setprecision(16);
#ifdef ENABLE_NIVALIS_READLINE_SHELL
    char* buf;
    while(!shell.closed && (buf = readline(">> ")) != nullptr) {
        if (strlen(buf) > 0) {
            add_history(buf);
        }
        shell.eval_line(buf);
        free(buf);
    }
#else
    std::string line;
    while(!shell.closed) {
        std::cout << ">>> " << std::flush;
        std::getline(std::cin, line);
        shell.eval_line(line);
    }
#endif
    return 0;
}
