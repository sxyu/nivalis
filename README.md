# Nivalis Plotter

## Installation
- Install Nana <https://github.com/qPCR4vir/nana>
    - Use CMake. Install `libnana.a` to `/usr/local/lib' and
      copy `include/nana` directory to `/usr/local/include` (on Linux) 
        - Have to rename `build/makefile` included in repo,
          else CMake build will break
- Build with CMake: `mkdir build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Release && make -j8`
    - You may need to set Nana's include path: `cmake .. -DNANA_INCLUDE_DIR='dir_containing_nana_headers'`

## Usage 
- `./nivalis`
- Enter expressions to evaluate them
    - Exponentiation is `^`, other operators are standard
    - Some functions available: `exp ln log10 log2 sin cos tan asin acos atan abs fact gamma`
    - Piecewise: `{x<0 : x, x>=0 : x^2}`
### GUI
- Enter `plot` to show plotter GUI
    - `E` to edit function expression (or click the textbox)
        - Functions parameterized by `x` e.g. `x^2` or `y=ln(x)` or `x^3=y`
        - Implicit function (slower, less detail):
          e.g. `x=3` or `abs(x)=abs(y)` or `cos(x)=sin(y)` or `cos(x*y) = 0`
        - Updates plot automatically
    - `Up`/`Down` arrow keys (or use `<` `>` buttons below textbox) to switch functions or add new functions (by going past last defined function)
    - `Delete` or click the x button to delete current function
    - Drag mouse to move, scroll to zoom
    - `Ctrl`/`Alt` `+``-` to zoom asymmetrically
    - Ctrl+H or click the reset view button to reset to home view (around origin)
![Screenshot](https://github.com/sxyu/nivalis/blob/master/readme_img/screenshot.png?raw=true)
![Screenshot: implicit functions](https://github.com/sxyu/nivalis/blob/master/readme_img/implicit.png?raw=true)
