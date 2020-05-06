# Nivalis Plotter

## Installation
- Install nana <https://github.com/qPCR4vir/nana>
- Compile with CMake: `cd build && cmake .. -DCMAKE_BUILD_TYPE=Release`

## Usage 
- `./nivalis`
- Enter expressions to evaluate them
    - Exponentiation is `^`, other operators are standard
    - Some functions available: `exp ln log10 log2 sin cos tan asin acos atan abs fact gamma`
    - Piecewise: `{x<0 : x, x>=0 : x^2}`
- Enter `plot` to show plotter GUI
    - E to edit expression (or click the textbox)
    - Enter in textbox to update
    - Drag mouse to move, scroll to zoom
    - Ctrl/Alt +- to zoom asymmetrically
    - Ctrl+H or click the home button (right of update) to reset to home view (around origin)
![Screenshot](https://github.com/sxyu/nivalis/blob/master/readme_img/screenshot.png?raw=true)
