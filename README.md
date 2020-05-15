# Nivalis Plotter

A simple expression evaluator + interactive function plotter in C++ supporting implicit functions.
The evaluator parses expressions into custom bytecode, which is optimized before evaluation, leading to quite good performance. 

## Dependencies
- C++ 17
- Boost 1.58+ (Optional, only need math)
- OpenGL3 and GLEW
    - and glfw3, Dear ImGui which are included in repo

## Installation

- I offer instructions for Ubuntu and Windows, since I only have access to these systems

### Ubuntu 
- Install a modern version of GCC which supports C++17
- Install OpenGL3, GLEW, and glfw3 (more features, recommended)
    - `sudo apt update && sudo apt install -y pkg-config mesa-utils libglew-dev libglfw3-dev`
- Configure project with CMake:
    - `mkdir build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Release`
    - CMake options:
        - To force disable Boost math add `-DUSE_BOOST_MATH=OFF` to this command (some functions like digamma, beta will become unavailable)
        - To force using glfw3 in the repo (as opposed to the one you installed) use `-DUSE_SYSTEM_GLFW=OFF`
        - To disable OpenGL/Dear ImGui and force using Nana, add
          `-DUSE_OPENGL_IMGUI=OFF` to this command 
- Built project: `make -j8`
- Optionally: install by `sudo make install`

### Windows
- Install Visual Studio 2017+, if not already present. I used 2017
- Install GLEW, and glfw3 (more features, recommended)
    - Go to  <http://glew.sourceforge.net/> and click "Binaries Windows 32-bit and 64-bit"; extract it somewhere. Note the directory
        `.../glew-x.x.x` containing `include` and `lib/Release/x64/glew32s.lib`
- Configure project with CMake:
    - `mkdir build && cd build && cmake .. -G"Visual Studio 15 2017 Win64" -DCMAKE_BUILD_TYPE=Release`
    - CMake options:
        - To force disable Boost math add `-DUSE_BOOST_MATH=OFF` to this command (some functions like digamma, beta will become unavailable)
        - To force using glfw3 in the repo (as opposed to the one you installed) use `-DUSE_SYSTEM_GLFW=OFF`
- Built project: `cmake --build . --config Release`, or open the solution in VS and build in "Release" configuration manually

### Testing
- Tests are built by default. To disable, add `-DBUILD_TESTS=OFF` to cmake command line
- Linux: `make test` to run tests (with ctest), or just manually run test/test_*
- Alternatively, Windows/Linux: `ctest` to run tests
- `ctest --verbose` to get more information (error line number etc.)

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

## Usage 
### OpenGL GUI
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
 
![Screenshot of New GUI](https://github.com/sxyu/nivalis/blob/master/readme_img/screenshot2.png?raw=true)
