# Nivalis Plotter

A simple expression evaluator + interactive function plotter in C++ supporting implicit functions.
The evaluator parses expressions into custom bytecode, which is optimized before evaluation, leading to quite good performance. 

## Dependencies
- C++ 17
- Boost 1.58+ (Optional, only need math)
- GUI Library; one of
    - OpenGL3 and GLEW
        - and glfw3, Dear ImGui which are included in repo
    - Nana (limited 

## Installation

- I offer instructions for Ubuntu and Windows, since I only have access to these systems

### Ubuntu 
- Install a modern version of GCC which supports C++17
- Choose a GUI backend option:
    - Install OpenGL3, GLEW, and glfw3 (more features, recommended)
        - `sudo apt update && sudo apt install -y pkg-config mesa-utils libglew-dev libglfw3-dev`
    - Install Nana: clone and follow the instructions in <https://github.com/qPCR4vir/nana>. You can use CMake or just GCC. Good luck!
- Configure project with CMake:
    - `mkdir build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Release`
    - CMake options:
        - To force disable Boost math add `-DUSE_BOOST_MATH=OFF` to this command (some functions like digamma, beta will become unavailable)
        - To force using glfw3 in the repo (as opposed to the one you installed) use `-DUSE_SYSTEM_GLFW=OFF`
        - To disable OpenGL/Dear ImGui and force using Nana, add
          `-DUSE_OPENGL_IMGUI=OFF` to this command 
        - If using Nana: you may need to set Nana's include path: `cmake .. -DNANA_INCLUDE_DIR='dir_containing_nana_headers'`
          and library path: `cmake .. -DNANA_LIBRARY='nana_output_lib_name'`
- Built project: `make -j8`

### Windows
- Install Visual Studio 2017+, if not already present. I used 2017
- Choose a GUI backend option:
    - Install GLEW, and glfw3 (more features, recommended)
        - Go to  <http://glew.sourceforge.net/> and click "Binaries Windows 32-bit and 64-bit"; extract it somewhere. Note the directory
            `.../glew-x.x.x` containing `include` and `lib/Release/x64/glew32s.lib`
    - Install Nana <https://github.com/qPCR4vir/nana>
        - Clone <https://github.com/qPCR4vir/nana>. VS solution files are in the build directory of this repo.
- Configure project with CMake:
    - `mkdir build && cd build && cmake .. -G"Visual Studio 15 2017 Win64" -DCMAKE_BUILD_TYPE=Release`
    - CMake options:
        - To force disable Boost math add `-DUSE_BOOST_MATH=OFF` to this command (some functions like digamma, beta will become unavailable)
        - To force using glfw3 in the repo (as opposed to the one you installed) use `-DUSE_SYSTEM_GLFW=OFF`
        - To disable OpenGL/Dear ImGui and force using Nana, add
          `-DUSE_OPENGL_IMGUI=OFF` to this command 
        - If using Nana: you may need to set Nana's include path: `cmake .. -DNANA_INCLUDE_DIR='dir_containing_nana_headers'`
          and library path: `cmake .. -DNANA_LIBRARY='nana_output_lib_name'`
- Built project: `cmake --build . --config Release`, or open the solution in VS and build in "Release" configuration manually

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

### OpenGL GUI
- Run the `./nivplot` binary to open GUI
- Background window (plotter)
    - Drag mouse (or arrow keys) to move, scroll (or `=`/`-`)  to zoom
    - `Ctrl`/`Alt` and `=`/`-` to zoom asymmetrically
    - Mouse over a marked point (minimum/maximum/intersection etc) to see label+coordinates
- Function window (editor)
    - `Ctrl`+`E` to edit function expressions (or click the textbox)
        - Function expressions can be:
            - Functions parameterized by `x` e.g. `x^2` or `y=ln(x)` or `x^3=y`
            - Implicit function (slower, less detail):
              e.g. `x=3` or `abs(x)=abs(y)` or `cos(x)=sin(y)` or `cos(x*y) = 0`
        - Updates plot automatically
    - Click `+ New function` to add a function. Click textbox to highlight functions. Alternatively, use `Up`/`Down` arrow keys in textbox to switch between functions or add a new one (by going beyond the bottomost existing function)
    - Click the x button to delete the current function
    - Click the `? Help` button for a reference documenting functions/operators etc.
- View window
    - `Ctrl`+`H` or click the reset view button (View window) to reset to home view (around origin)
    - Modify the bounds numbers on the View window to change view (minx maxx miny maxy) manually
- Sliders window
    - Click `+ New slider` to add slider
    - Once added, change first textbox to change variable to modify when moving slider
    - Second/third boxes are lower/upper bounds for the slider
    - Click x button to delete slider
    - Click on slider below to change the variable value
    - E.g. if variable is `a`, you can write `a*x` in some function (in the Function window) and then drag the slider to see the function change smoothly

### Nana GUI
- Run the `./nivplot` binary to open GUI
- After launch, `Ctrl`+`E` to edit function expression (or click the textbox)
    - Function expressions can be:
        - Functions parameterized by `x` e.g. `x^2` or `y=ln(x)` or `x^3=y`
        - Implicit function (slower, less detail):
          e.g. `x=3` or `abs(x)=abs(y)` or `cos(x)=sin(y)` or `cos(x*y) = 0`
    - Updates plot automatically
- `Up`/`Down` arrow keys in textbox (or use `<` `>` buttons below textbox) to switch functions or add new functions (by going past last defined function)
- Click the x button or press `Ctrl`+`Del` to delete current function
- Drag mouse (or arrow keys) to move, scroll (or `=`/`-`)  to zoom
- `Ctrl`/`Alt` and `=`/`-` to zoom asymmetrically
- `Ctrl`+`H` or click the reset view button to reset to home view (around origin)
- Mouse over a marked point (minimum/maximum/intersection etc) to see label+coordinates
![Screenshot](https://github.com/sxyu/nivalis/blob/master/readme_img/screenshot.png?raw=true)
![Screenshot: implicit functions (older version)](https://github.com/sxyu/nivalis/blob/master/readme_img/implicit.png?raw=true)
