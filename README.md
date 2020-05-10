# Nivalis Plotter

A simple expression evaluator + interactive function plotter in C++ supporting implicit functions.
The evaluator parses expressions into custom bytecode, which is optimized before evaluation, leading to quite good performance. 

## Dependencies
- C++ 17
- Boost 1.58+ (Optional, only need math)
- GUI Library
    - OpenGL3 and GLFW (and Dear ImGui which is included in repo), OR
    - Nana

## Installation
- Install OpenGL3 and GLFW, OR
- Install Nana <https://github.com/qPCR4vir/nana>
    - Windows: Use Visual Studio 17. A solution file is in the build directory of the repo
    - Linux: I used CMake to build. You may also try one of the other provided methods. Unfortunately CMake install doesn't appear to be set up correctly.
      Aftr build, you'll need to manually install `libnana.a` to `/usr/local/lib` and copy `include/nana` directory to `/usr/local/include` (on Linux)
        - Pitfall: if you decided to use CMake and build in the `build` directory, you must first rename the `build/makefile` directory in repo, or
          else CMake's build will break
- Configure project with CMake: `mkdir build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Release8`
    - You may need to set Nana's include path: `cmake .. -DNANA_INCLUDE_DIR='dir_containing_nana_headers'`
    - You may need to set Nana's library path: `cmake .. -DNANA_LIBRARY='nana_output_lib_name'`
- Built project: *Linux*: `make -j8` *MSVC*: `cmake --build . --config Release`, or open the solution
    - To force disable Boost add `-DUSE_BOOST_MATH=OFF` to this command (some functions like digamma, beta will become unavailable)
    - To disable OpenGL/Dear ImGui and force using Nana, add
      `-DUSE_OPENGL_IMGUI=OFF` to this command 

## Usage 
### Shell
- Run `./nivalis`
- Enter expressions to evaluate them
    - Exponentiation is `^`, other operators are standard: `+-*/%`
    - Comparison operators: `<`,`>`,`<=`,`>=`,`==` (or `=`, but can mean assignment statement in shell and equation in plotter)
    - Some functions available: `sqrt exp ln log10 log2 sin cos tan asin acos atan abs fact gamma digamma polygamma beta`
    - Piecewise functions aka. conditionals: `{x<0 : x, x>=0 : x^2}` or `{x<0 : x, x^2}` (last case is else by default)
    - Sum/prod special forms: `sum(x:1:10)[<expr>]` and `prod(x:1:10)[<expr>]` (inclusive indexing, upper index can be < lower)
    - Derivative special form: `diff(x)[<expr>]`

### GUI
- In Shell enter `plot` to show plotter GUI, or `plot <function_expr>` to plot a function expression
    - Alternatively (0.0.3), run the `nivplot` binary to open GUI directly
- After launch, `Ctrl`+`E` to edit function expression (or click the textbox)
    - Function expressions can be:
        - Functions parameterized by `x` e.g. `x^2` or `y=ln(x)` or `x^3=y`
        - Implicit function (slower, less detail):
          e.g. `x=3` or `abs(x)=abs(y)` or `cos(x)=sin(y)` or `cos(x*y) = 0`
    - Updates plot automatically
- `Up`/`Down` arrow keys in textbox (or use `<` `>` buttons below textbox) to switch functions or add new functions (by going past last defined function)
- `Delete` or click the x button to delete current function
- Drag mouse (or arrow keys) to move, scroll (or `=`/`-`)  to zoom
- `Ctrl`/`Alt` and `=`/`-` to zoom asymmetrically
- `Ctrl`+`H` or click the reset view button to reset to home view (around origin)
- Mouse over a marked point (minimum/maximum/intersection etc) to see label+coordinates
![Screenshot](https://github.com/sxyu/nivalis/blob/master/readme_img/screenshot.png?raw=true)
![Screenshot: implicit functions (older version)](https://github.com/sxyu/nivalis/blob/master/readme_img/implicit.png?raw=true)
