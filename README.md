# Nivalis Plotter

A Desmos-like interactive function plotter in C++ supporting implicit functions and real-time critical point/intersection finding.
Under the hood: features an expression parser, expression (AST) evaluator, symbolic differentiator and expression simplifier, which can be used directly through the shell.

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
  - Using Python 3: in build-emcc/, run: `python3 -m http.server` then run `firefox localhost:8000`
- To deploy, simply upload `index.html`, `nivplot.js`, `nivplot.wasm`

### Testing
- Tests are built by default. To disable, add `-DBUILD_TESTS=OFF` to cmake command line
- Linux: `make test` to run tests (with ctest), or just manually run test/test_*
- Alternatively, Windows/Linux: `ctest` to run tests
- `ctest --verbose` to get more information (error line number etc.)

## Usage
### Plotter GUI
- Web app: <https://www.ocf.berkeley.edu/~sxyu/plot/>
- Alternitively, run the `./nivplot` binary to open GUI
- Background window (plotter)
    - Drag mouse (or arrow keys) to move, scroll (or `=`/`-`)  to zoom
    - `Ctrl`/`Alt` and `=`/`-` to zoom asymmetrically
    - Mouse over a marked point (minimum/maximum/intersection etc) to see label+coordinates
    - Click any point on an explicit function to see the x-value and function value
- Function window (editor)
    - `E` to edit function expressions (or click the textbox)
        - Function expressions can be:
            - Functions parameterized by `x` e.g. `x^2` or `y=ln(x)` or `x^3=y`. Syntax is same as in shel
            - Implicit function (less detail, no subpixel render):
              e.g. `x=3` or `abs(x)=abs(y)` or `cos(x)=sin(y)` or `cos(x*y) = 0`
            - Inequalities (implicit): e.g. `x<y`, `cos(y)<sin(y)`, `x^2>y`
            - Parametric: `(<x-expr>, <y-expr>)`, where expressions should be in terms of `t` e.g. `(4*sin(4*t), 3*sin(3*t))`
            - Polar: `r=<expr>`, where `<expr>` should be in terms of angle `t` e.g. `r = 1-cos(t)`
                 -After entering a parametric/polar function,
                  inputs will appear to allow adjusting bounds on `t`
                  (you can directly set the value or drag *t min*, *t max* to change)
           - *Polylines*: draws a series of points and lines e.g. `(5,1)`, or `(1,1) (2,2) (a,b)`
                - If size 1, e.g. `(a,b)`, it draws a single point
                - If size >1, e.g. `(a,b)(c,d)`, draws all points and connects them in order
          - The expression syntax is standard and mostly Python-like.
            For details, see the next section (shell).
        - Updates plot automatically
    - Click `+ New function` to add a function. Click textbox to highlight functions. Alternatively, use `Up`/`Down` arrow keys in textbox to switch between functions or add a new one (by going beyond the bottomost existing function)
    - Click the x button to delete the current function
    - Desktop only: **Reference**: Click the `? Help` button for a reference documenting functions/operators etc.
        - It is similar to this page
- **Shell**: Click the `Shell` button to get a virtual shell popup; this is in the function editor in the desktop GUI, top navbar in the web app. See next section for usage. Use cases:
        - Evaluate a function you entered at some point. For example, if you entered `gamma(x)*digamma(x)` in the textbox labelled `f0`, you can enter `f0(5)` in the shell to evaluate `gamma(5)*digamma(5)`
        - Define custom functions to use in the function editor. For example,
define `sec(x) = 1/cos(x)`, or event function with multiple arguments
        - Set variables manually, e.g. `C = 3.5`
        - Use the symbolic differentiation/expression simplification engine
            - `s <expr>` e.g. `s diff(x)[x]`
- View window
    - `Ctrl`+`H` or click the reset view button (View window) to reset to home view (around origin)
    - Modify the bounds numbers on the View window to change view (minx maxx miny maxy) manually
- Sliders window (attached below function window in web GUI)
    - Click `+ New slider` to add slider
    - Once added, change first textbox to change variable to modify when moving slider
    - Second/third boxes are lower/upper bounds for the slider
    - Click x button to delete slider
    - Click on slider below to change the variable value
    - E.g. if variable is `a`, you can write `a*x` in some function (in the Function window) and then drag the slider to see the function change smoothly

### Shell
- Run `./nivalis`
    - Or use the shell built into the desktop GUI/web app; see previous section
- Enter expressions to evaluate them
    - Exponentiation is `^`, other operators are standard: `+-*/%`
        - Note `*` is required: e.g. `2sin(0.5x)` is not valid, you need to write `2*sin(0.5*x)`
    - Comparison operators: `<`,`>`,`<=`,`>=`,`==`/`=` (but `=` can mean assignment statement in shell; all mean equality/inequality in plotter)
    - Logical operators: `&`, `|` for and/or (e.g. `x<0 & x>-1`)
        - Related functions: `not(_)`, `xor(_, _)`
    - Some mathematical functions available: `sqrt exp exp2 ln log10 log2 log sin cos tan asin acos atan sinh cosh tanh abs sgn max min floor ceil round fact gamma digamma trigamma polygamma erf zeta beta N` where `N(x)` is standard Gaussian pdf
        - erf, zeta, beta, polygamma, digamma only available with Boost math (included in web version)
        - Note all functions take a fixed number of arguments, except `log(x, base)`, which takes 1 OR 2 arguments (default base `e`)
    - Some integer functions: `gcd lcm choose rifact fafact ifact` (rifact/fafact are rising/falling factorial)
    - Some utility functions: `max(_, _)`, `min(_, _)`,
    - Piecewise functions aka. conditionals: `{<if-pred>:<if-expr>,<elif-pred>:<elif-expr>,...,<else-expr>}` e.g. `{x<0 : x, x>=0 : x^2}` or `{x<0 : x, x^2}` (last case is else by default)
    - Sum/prod special forms: `sum(x:1:10)[<expr>]` and `prod(x:1:10)[<expr>]` (inclusive indexing, upper index can be < lower)
    - Derivative special form: `diff(<var>)[<expr>]` takes the derivative of `<expr>` wrt `<var>` and evaluates it at current value of `<var>`. E.g. `diff(x)[sin(x)]`
    - Higher-order derivative special forms: `diff<ord>(<var>)[<expr>]`, where ord should be a number in 0...5
- Define variable: for example, `a = 3+4`, then you can use `a` anywhere. Variables may contain: `0-9a-zA-Z_'` but cannot start with a number, e.g. `x3'` is valid.
    - Operator assignment: `a+=3`, `a*=3`, etc., as in usual languages
- Define custom function: `<name>(<args>) = <expr>` e.g. `sec(x) = 1/cos(x)` or `f(x,y,z) = x+y+z`
- Symbolic operations
    - Symbolic simplification: `s <expr>` e.g. `s (1+x)^2 + 2*(x+1)^2`, `s exp(x)*exp(2*x)`
        - Disclaimer: should be correct (modulo removalble discontinuities), but does not simplify super reliably
    - Differentiate a function: `diff <var> <expr>` e.g. `diff x sin(x)*cos(2*x)`; outputs the derivative expression
        - Alternatively, use `s diff(x)[<expr>]`, which is more flexible since you can use `s diff2(x)[<expr>]`, etc.
