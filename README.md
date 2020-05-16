# Nivalis Plotter

A simple expression evaluator + interactive function plotter in C++ supporting implicit functions.
The evaluator parses expressions into custom bytecode, which is optimized before evaluation, leading to quite good performance.

- **Live online demo** using WebAssembly:
<https://www.ocf.berkeley.edu/~sxyu/plot/>

- **Pre-built binaries** for Windows 10 and Ubuntu 16/18 LTS (x86-64) in 
<https://github.com/sxyu/nivalis/releases>

![Screenshot of plotter GUI](https://github.com/sxyu/nivalis/blob/master/readme_img/screenshot.png?raw=true)
![Screenshot of plotter GUI's virtual shell](https://github.com/sxyu/nivalis/blob/master/readme_img/shell.png?raw=true)

## Dependencies
- C++ 17
- Boost 1.58+ (Optional, only need math)
- OpenGL3
    - and GLEW, glfw3, Dear ImGui which are included in repo

## Installation

- I offer instructions for Ubuntu and Windows (since I only have access to these systems) and Webasm (through emscripten)

### Ubuntu 16+
- Install CMake from <https://cmake.org/download/>
    - If you already have an older version: I do not guarantee version <3.14 will work, but you may try
- Install a modern version of GCC which supports C++17 (GCC 7 will work)
- Install OpenGL3
    - `sudo apt update && sudo apt install -y pkg-config mesa-common-dev freeglut3 freeglut3-dev`
- Configure project with CMake:
    - `mkdir build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Release`
    - CMake options:
        - To force disable Boost math add `-DUSE_BOOST_MATH=OFF` to this command (some functions like digamma, beta will become unavailable)
        - To force using glfw3 in the repo (as opposed to the one you installed) use `-DUSE_SYSTEM_GLFW=OFF`
        - To disable OpenGL/Dear ImGui and force using Nana, add
          `-DUSE_OPENGL_IMGUI=OFF` to this command
- Build project: `make -j8`
- Optionally: install by `sudo make install`

### Windows 10
- Install CMake from <https://cmake.org/download/>
- Install Visual Studio 2017+, if not already present. I used 2017
- Configure project with CMake:
    - `mkdir build && cd build && cmake .. -G"Visual Studio 15 2017 Win64" -DCMAKE_BUILD_TYPE=Release`
    - CMake options:
        - To force disable Boost math add `-DUSE_BOOST_MATH=OFF` to this command (some functions like digamma, beta will become unavailable)
        - To force using glfw3 in the repo (as opposed to the one you installed) use `-DUSE_SYSTEM_GLFW=OFF`
- Build project: `cmake --build . --config Release`, or open the solution in VS and build in "Release" configuration manually

### Webasm (building on Ubuntu)
- Configure project with CMake:
    - `cd build-emcc && emcmake cmake ..`
- Build: `make -j8`
- Host `build-emcc/` on a server and open index in a browser.
  - Using Python 3: in build-emcc/, run: `python3 -http.server` then run `firefox localhost:8000`
- To deploy, simply upload `index.html`, `nivplot.js`, `nivplot.wasm`

### Testing
- Tests are built by default. To disable, add `-DBUILD_TESTS=OFF` to cmake command line
- Linux: `make test` to run tests (with ctest), or just manually run test/test_*
- Alternatively, Windows/Linux: `ctest` to run tests
- `ctest --verbose` to get more information (error line number etc.)

## Usage
### Shell
- Run `./nivalis`
    - Or use the shell built into the GUI; see next section
- Enter expressions to evaluate them
    - Exponentiation is `^`, other operators are standard: `+-*/%`
    - Comparison operators: `<`,`>`,`<=`,`>=`,`==` (or `=`, but can mean assignment statement in shell and equation in plotter)
    - Some functions available: `sqrt exp ln log10 log2 sin cos tan asin acos atan abs fact gamma digamma polygamma beta`
    - Piecewise functions aka. conditionals: `{x<0 : x, x>=0 : x^2}` or `{x<0 : x, x^2}` (last case is else by default)
    - Sum/prod special forms: `sum(x:1:10)[<expr>]` and `prod(x:1:10)[<expr>]` (inclusive indexing, upper index can be < lower)
    - Derivative special form: `diff(x)[<expr>]`
- Define variable: for example, `a = 3+4`, then you can use `a` anywhere. Variables may contain: `0-9a-zA-Z_'` but cannot start with a number, e.g. `x3'` is valid.
    - Operator assignment: `a+=3`, `a*=3`, etc., as in usual languages
- Define custom function: `<name>(<args>) = <expr>` e.g. `sec(x) = 1/cos(x)` or `f(x,y,z) = x+y+z`
- Symbolic operations
    - Differentiate a function: `diff <var> <expr>` e.g. `diff x sin(x)*cos(2*x)`
    - Simplify expression (not super reliable): `opt <expr>` e.g. `opt (1+x)^2 + 2*(x+1)^2`, `opt exp(x)*exp(2*x)`

### Plotter GUI
- Run the `./nivplot` binary to open GUI
- Background window (plotter)
    - Drag mouse (or arrow keys) to move, scroll (or `=`/`-`)  to zoom
    - `Ctrl`/`Alt` and `=`/`-` to zoom asymmetrically
    - Mouse over a marked point (minimum/maximum/intersection etc) to see label+coordinates
    - Click any point on an explicit function to see the x-value and function value
- Function window (editor)
    - `Ctrl`+`E` to edit function expressions (or click the textbox)
        - Function expressions can be:
            - Functions parameterized by `x` e.g. `x^2` or `y=ln(x)` or `x^3=y`. Syntax is same as in shel
            - Implicit function (less detail, no subpixel render):
              e.g. `x=3` or `abs(x)=abs(y)` or `cos(x)=sin(y)` or `cos(x*y) = 0`
            - *Polylines*: draws a series of points and lines e.g. (1,1) e.g. (1,1) (2,2) (3,2)
                - If size 1, like (a,b), it draws a single
                - If size >1, like (a,b)(c,d), draws all points and connects them in order
        - Updates plot automatically
    - Click `+ New function` to add a function. Click textbox to highlight functions. Alternatively, use `Up`/`Down` arrow keys in textbox to switch between functions or add a new one (by going beyond the bottomost existing function)
    - Click the x button to delete the current function
    - **Reference**: Click the `? Help` button for a reference documenting functions/operators etc.
    - **Shell**: Click the `# Shell` button to get a virtual shell popup. See previous section for usage.
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
