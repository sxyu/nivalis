# Nivalis Plotter

A simple expression evaluator + interactive function plotter in C++, supporting implicit functions.
The evaluator parses expressions into custom bytecode, which is optimized before evaluation, leading to quite good performance. 

## Dependencies
- C++ 17
- Nana (GUI library)

## Installation
- Install Nana <https://github.com/qPCR4vir/nana>
    - I used CMake to build. You may also try one of the other provided methods. Unfortunately CMake install doesn't appear to be set up correctly.
      Aftr build, you'll need to manually install `libnana.a` to `/usr/local/lib` and copy `include/nana` directory to `/usr/local/include` (on Linux) 
        - A danger: if you decided to use CMake and build in the `build` directory, you must first rename the `build/makefile` directory in repo, or
          else CMake's build will break
- Build with CMake: `mkdir build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Release && make -j8`
    - You may need to set Nana's include path: `cmake .. -DNANA_INCLUDE_DIR='dir_containing_nana_headers'`

## Usage 
- `./nivalis`
- Enter expressions to evaluate them
    - Exponentiation is `^`, other operators are standard
    - Some functions available: `exp ln log10 log2 sin cos tan asin acos atan abs fact gamma`
    - Piecewise: `{x<0 : x, x>=0 : x^2}`
### GUI
- Enter `plot` to show plotter GUI, or `plot <function_expr>` to plot a function expression
    - After launch, `E` to edit function expression (or click the textbox)
        - Function expressions can be:
            - Functions parameterized by `x` e.g. `x^2` or `y=ln(x)` or `x^3=y`
            - Implicit function (slower, less detail):
              e.g. `x=3` or `abs(x)=abs(y)` or `cos(x)=sin(y)` or `cos(x*y) = 0`
        - Updates plot automatically
    - `Up`/`Down` arrow keys in textbox (or use `<` `>` buttons below textbox) to switch functions or add new functions (by going past last defined function)
    - `Delete` or click the x button to delete current function
    - Drag mouse (or arrow keys) to move, scroll (or `=`/`-`)  to zoom
    - `Ctrl`/`Alt` and `=`/`-` to zoom asymmetrically
    - Ctrl+H or click the reset view button to reset to home view (around origin)
![Screenshot](https://github.com/sxyu/nivalis/blob/master/readme_img/screenshot.png?raw=true)
![Screenshot: implicit functions](https://github.com/sxyu/nivalis/blob/master/readme_img/implicit.png?raw=true)
