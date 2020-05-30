from jinja2 import Template, Environment, FileSystemLoader
from shutil import copyfile
import os, random
dir_path = os.path.dirname(os.path.realpath(__file__))
os.makedirs(os.path.join(dir_path, 'out', 'css'), exist_ok = True)

ver_base = 10**15
ver_num = str(random.randint(ver_base, 10 * ver_base - 1))

html_fname = "index.html"
with open(os.path.join(dir_path, html_fname), 'r') as f:
    template = Environment(loader=FileSystemLoader(dir_path)).from_string(f.read())
render_html = template.render(ver = ver_num)
with open(os.path.join(dir_path, 'out', html_fname), 'w') as f:
    f.write(render_html)

copy_files = ["worker.js", "worker.wasm", "css/3rdparty.css", "css/main.css"]
for fname in copy_files:
    copyfile(os.path.join(dir_path, fname), os.path.join(dir_path, "out", fname))
copyfile(os.path.join(dir_path, "nivplot.wasm"), os.path.join(dir_path, "out/js/nivplot.wasm"))

main_js = "out/js/main.js"
with open(os.path.join(dir_path, main_js), 'r') as f:
    data = f.read()
with open(os.path.join(dir_path, main_js), 'w') as f:
    ver_prefix = 'var nivalis_ver_num = '
    ver_text = ver_prefix + ver_num + ";"
    if data[:len(ver_prefix)] == ver_prefix:
        data = data[len(ver_text):]
    data = ver_text + data
    data = data.replace('"nivplot.wasm"', '"nivplot.wasm?' + ver_num + '"')
    f.write(data)
del data

worker_js = "out/worker.js"
with open(os.path.join(dir_path, worker_js), 'r') as f:
    worker_data = f.read()
with open(os.path.join(dir_path, worker_js), 'w') as f:
    worker_data = worker_data.replace('"worker.wasm"', '"worker.wasm?' + ver_num + '"')
    f.write(worker_data)
