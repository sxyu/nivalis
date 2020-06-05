from jinja2 import Template, Environment, FileSystemLoader
from shutil import copyfile
import os, random
import distutils.dir_util

copy_files = ["worker.js", "worker.wasm"]
html_fname = "index.html"
main_js = "js/main.js"
worker_js = "worker.js"
third_party_css = "css/3rdparty.css"
main_css = "css/main.css"

# Random version identifier; will append ?ver_num to js/wasm file names
ver_base = 10**15
ver_num = str(random.randint(ver_base, 10 * ver_base - 1))

dir_path = os.path.dirname(os.path.realpath(__file__))
os.makedirs(os.path.join(dir_path, 'out', 'css'), exist_ok = True)

with open(os.path.join(dir_path, html_fname), 'r') as f:
    template = Environment(loader=FileSystemLoader(dir_path)).from_string(f.read())
render_html = template.render(ver = ver_num)
with open(os.path.join(dir_path, 'out', html_fname), 'w') as f:
    f.write(render_html)

for fname in copy_files:
    copyfile(os.path.join(dir_path, fname), os.path.join(dir_path, "out", fname))
copyfile(os.path.join(dir_path, "nivplot.wasm"), os.path.join(dir_path, "out/js/nivplot.wasm"))

with open(os.path.join(dir_path, 'out', main_js), 'r') as f:
    data = f.read()
with open(os.path.join(dir_path, 'out', main_js), 'w') as f:
    ver_prefix = 'var nivalis_ver_num = '
    ver_text = ver_prefix + ver_num + ";"
    if data[:len(ver_prefix)] == ver_prefix:
        data = data[len(ver_text):]
    data = ver_text + data
    data = data.replace('"nivplot.wasm"', '"nivplot.wasm?' + ver_num + '"')
    #  data = js_minify(data) # fails due to ES6/7 code
    f.write(data)
del data

with open(os.path.join(dir_path, 'out', worker_js), 'r') as f:
    worker_data = f.read()
with open(os.path.join(dir_path, 'out', worker_js), 'w') as f:
    worker_data = worker_data.replace('"worker.wasm"', '"worker.wasm?' + ver_num + '"')
    f.write(worker_data)

with open(os.path.join(dir_path, third_party_css), 'r') as f:
    third_party_css_data = f.read()
with open(os.path.join(dir_path, main_css), 'r') as f:
    main_css_data = f.read()
with open(os.path.join(dir_path, 'out', main_css), 'w') as f:
    f.write(third_party_css_data + '\n' + main_css_data)
    
distutils.dir_util.copy_tree('css/fonts', 'out/css/fonts')
