var is_mobile = ( /Android|webOS|iPhone|iPad|iPod|BlackBerry|IEMobile|Opera Mini/i.test(navigator.userAgent));

var download = function(filename, text) {
    var element = document.createElement('a');
    element.setAttribute('href', 'data:text/plain;charset=utf-8,' + encodeURIComponent(text));
    element.setAttribute('download', filename);

    element.style.display = 'none'; document.body.appendChild(element);
    element.click();

    document.body.removeChild(element);
};

var Renderer = {
    redraw_cnt: 0,
    _redraw : function() {
        Module.redraw();
        Renderer.redraw_cnt -= 1;
        if (Renderer.redraw_cnt > 0) {
            window.requestAnimationFrame(Renderer._redraw);
        }
    },
    redraw : function() {
        if (Renderer.redraw_cnt <= 0) {
            Renderer.redraw_cnt = 50;
            window.requestAnimationFrame(Renderer._redraw);
        } else {
            Renderer.redraw_cnt = 50;
        }
    },
};

var pickrConfig = {
    theme: 'monolith',
    comparison: false,
    lockOpacity: true,
    padding: 0,
    swatches: [
        '#FF0000', '#4169E1',
        '#008000', '#FFA500',
        '#800080', '#000000',
        'rgba(255, 116, 0, 1)',
    ],
    useAsButton: true,
    components: {

        // Main components
        preview: true,
        opacity: false,
        hue: true,

        // Input / output Options
        interaction: {
            hex: true,
            rgba: true,
            hsla: false,
            hsva: false,
            cmyk: false,
            input: true,
            clear: false,
            save: false
        }
    }
};

var FuncEdit = {
    func_names: [],
    func_indices: {},
    new_func: function(add) {
        var added = true;
        if (add) added = Module.add_func();
        if (!added) return;
        var fidx = Module.num_funcs() - 1;
        var fname = Module.get_func_name(fidx);
        $('#functions').append(
          "<div class='input-group mb-2 function-form' id='function-form-" + fname + "'>" +
            "<div class='input-group-prepend'>" +
            "<span class='input-group-text function-name inactive' id='function-name-" +
            fname + "'>" + fname + "</span></div>" +
            "<input class='form-control function-expr'" +
                     "id='function-expr-" + fname + "' type='text'>" +
             "<div class='input-group-append'>" +
             "<button class='btn btn-sm btn-light btn-outline-secondary color-picker' id='color-picker-" +
            fname +
            "'><ion-icon name='color-palette'></ion-icon></button>" +
            "<button class='btn btn-sm btn-light btn-outline-secondary function-del' id='function-del-" +
                fname + "'><ion-icon name='trash'></ion-icon></button></div>" + "</div>" +
          "<div class='input-group function-tbounds' id='function-tbounds-" +
            fname + "'> " +
            "<input class='form-control function-tmin'" +
            " id='function-tmin-" + fname + "' type='number' step='0.000001'>" +
            "<span class='input-group-btn' style='width:0px;'></span>" +
            "<span class='form-control var-bounds-text'> &le; t &le;</span>" +
            "<span class='input-group-btn' style='width:0px;'></span>" +
            "<input class='form-control function-tmax'" +
                     "id='function-tmax-" + fname + "' type='number' step='0.000001'>"
        );
        FuncEdit.func_names.push(fname);
        FuncEdit.func_indices[fname] = fidx;

        var obj = $('#function-expr-' + fname);
        obj.on('input', function() {
            // Change editor
            var $this = $(this);
            var this_name = this.id.substr(14);
            var idx = FuncEdit.func_indices[this_name];
            Module.set_func_expr(idx, $this.val());
            var uses_t = Module.get_func_uses_t(idx);
            if (uses_t) {
                $('#function-tbounds-' + this_name).css('display', 'flex');
            } else {
                $('#function-tbounds-' + this_name).css('display', 'none');
            }
            Module.redraw();
        });
        obj.on('keyup', function(e) {
            if (e.ctrlKey) return;
            // Up/down
            var $this = $(this);
            var this_name = this.id.substr(14);
            var idx = FuncEdit.func_indices[this_name];
            if (e.which == 38 && idx > 0) {
                e.preventDefault();
                var next_tb = $('#function-expr-' + FuncEdit.func_names[idx - 1]);
                next_tb.focus();
                // next_tb[0].selectionStart = next_tb[0].selectionEnd = next_tb[0].value.length;
            } else if (e.which == 40) {
                e.preventDefault();
                if (idx + 1 == Module.num_funcs()) {
                    FuncEdit.new_func(true);
                }
                var next_tb = $('#function-expr-' + FuncEdit.func_names[idx + 1]);
                next_tb.focus();
                // next_tb[0].selectionStart = next_tb[0].selectionEnd = next_tb[0].value.length;
            } else if (e.which == 27) {
                // Esc
                $this.blur();
            }
            Module.redraw();
        });
        obj.focus(function() {
            // Focus (change curr func)
            var $this = $(this);
            var this_name = this.id.substr(14);
            var idx = FuncEdit.func_indices[this_name];
            if (idx == Module.num_funcs() - 1) {
                // Add function
                FuncEdit.new_func(true);
            }
            $('#function-name-' + this_name).removeClass('inactive');
            Module.set_curr_func(idx);
            Module.redraw();
        });
        $('#function-tmin-' + fname).val(Math.floor(Module.get_func_tmin(fidx) * 1e6)/1e6);
        $('#function-tmax-' + fname).val(Math.ceil(Module.get_func_tmax(fidx) * 1e6)/1e6);
        $('#function-tmin-' + fname).on('input', function() {
            var $this = $(this);
            var this_name = this.id.substr(14);
            var idx = FuncEdit.func_indices[this_name];
            var maxv = $('#function-tmax-' + fname).val();
            if ($this.val() > maxv) {
                $this.val(maxv - 1e-6);
            } else {
                Module.set_func_tmin(fidx, Number.parseFloat($this.val()));
                Module.redraw();
            }
        });
        $('#function-tmax-' + fname).on('input', function() {
            var $this = $(this);
            var this_name = this.id.substr(14);
            var idx = FuncEdit.func_indices[this_name];
            var minv = $('#function-tmin-' + fname).val();
            if ($this.val() < minv) {
                $this.val(minv + 1e-6);
            } else {
                Module.set_func_tmax(fidx, Number.parseFloat($this.val()));
                Module.redraw();
            }
        });
        $('#function-del-' + fname).click(function() {
            // Delete
            var $this = $(this);
            var this_name = this.id.substr(13);
            var idx = FuncEdit.func_indices[this_name];
            if (FuncEdit.func_names.length > idx + 1) {
                Module.delete_func(idx);
                Module.redraw();
                $('#function-form-' + this_name).remove();
                $('#function-tbounds-' + this_name).remove();
                FuncEdit.func_names.splice(idx, 1);
                delete FuncEdit.func_indices[this_name];
                for (var t = idx; t < FuncEdit.func_names.length; t++) {
                    FuncEdit.func_indices[FuncEdit.func_names[t]] -= 1;
                }
            }
        });
        // Color picker
        const pickr = Pickr.create(Object.assign(
            {el: '#color-picker-' +  fname},
            pickrConfig
        ));
        pickr.on("init", instance => {
            var this_name = instance.options.el.id.substr(13);
            var idx = FuncEdit.func_indices[this_name];
            pickr.setColor("#" + Module.get_func_color(idx));
        });
        pickr.on("change", (color, instance) => {
            var hex = color.toHEXA().toString().substr(1);
            var this_name = instance.options.el.id.substr(13);
            var idx = FuncEdit.func_indices[this_name];
            Module.set_func_color(idx, hex);
            Module.redraw();
        });
    }
};

var ViewConfig = {
    vbounds : ['xmin', 'xmax', 'ymin', 'ymax'],
    updateViewBounds: function() {
        for (var i = 0; i < ViewConfig.vbounds.length; i++) {
            var b = ViewConfig.vbounds[i];
            var vbound = $('#view-' + b);
            vbound.val(Module['get_' + b]());
        }
    },
    updateViewPolar: function() {
        $('#view-polar').prop('checked', Module.get_is_polar_grid());
    },
    resetView: function() {
        Module.reset_view();
        ViewConfig.updateViewBounds();
        Module.redraw();
    }
};

var runtimeInitialized = function() {
    $(document).ready(function() {
        // Resize handler
        window.addEventListener('resize', onResizeCanvas, false);

        var canvas = document.getElementById("canvas");
        FuncEdit.new_func(false);
        document.addEventListener('keydown', e=>{
            // Must either have ctrl down OR nothing selected to activate
            if (event.ctrlKey==false &&
                $('*:focus').length > 0) return;
            if (event.ctrlKey==true &&
                (event.which == '61' || event.which == '107' ||
                    event.which == '173' || event.which == '109'  || event.which == '187'  ||
                    event.which == '189' ||
                    event.which == '72' || event.which == '67' || event.which == '86' ||
                    event.which == '88' || event.which == '79' || event.which == '80')) {
                event.preventDefault();
            }
            Module.on_key(e.which, e.ctrlKey, e.shiftKey, e.altKey);
            ViewConfig.updateViewPolar();
            ViewConfig.updateViewBounds();
            Renderer.redraw();
        });
        canvas.addEventListener("mousedown", e=>{
            if (e.button == 0) {
                Module.on_mousedown(e.offsetX, e.offsetY);
                Renderer.redraw();
            }
        });
        canvas.addEventListener("mousemove", e=>{
            if (e.button == 0) {
                Module.on_mousemove(e.offsetX, e.offsetY);
                ViewConfig.updateViewBounds();
                Renderer.redraw();
            }
        });
        canvas.addEventListener("mouseup", e=>{
            if (e.button == 0) {
                Module.on_mouseup(e.offsetX, e.offsetY);
                Renderer.redraw();
            }
        });
        canvas.addEventListener("wheel", e=>{
            if(e.ctrlKey)
                event.preventDefault();//prevent zoom
            Module.on_mousewheel(e.deltaY < 0,
                Math.abs(e.deltaY) * 5., e.offsetX, e.offsetY);
            Renderer.redraw();
            ViewConfig.updateViewBounds();
        });

        // Close loading screen
        if (document.getElementsByTagName('body')[0] !== undefined) {
            document.getElementById("loading").style.opacity =
                '0.0';
            window.setTimeout(function() {
                document.getElementById("loading").style.display =
                    'none';
            }, 300);
            onResizeCanvas();
        }

        // Emscripten GLFW monkey patch
        // - removed undesired fullscreen request on resize
        // - fixed touch mouse events and implements pinch-to-scroll
        GLFW.setWindowSize = function(winid, width, height) {
            var win = GLFW.WindowFromId(winid);
            if (!win) return;
            if (GLFW.active.id == win.id) {
                Browser.setCanvasSize(width, height);
                win.width = width;
                win.height = height
            }
            if (!win.windowSizeFunc)
                return;
            dynCall_viii(win.windowSizeFunc, win.id, width, height)
        };

        Module["canvas"].removeEventListener("wheel", GLFW.onMouseWheel, true);
        Module["canvas"].removeEventListener("mousewheel", GLFW.onMouseWheel, true);
        GLFW.onMouseWheel = function(event) {
            var delta = -Browser.getMouseWheelDelta(event);
            delta = delta == 0 ? 0 : delta > 0 ? Math.max(delta, 1) : Math.min(delta, -1);
            GLFW.wheelPos += delta;
            if (!GLFW.active || !GLFW.active.scrollFunc || event.target != Module["canvas"])
                return;
            var sx = 0;
            var sy = 0;
            if (event.type == "mousewheel") {
                sx = event.wheelDeltaX;
                sy = -event.wheelDeltaY
            } else {
                sx = event.deltaX;
                sy = -event.deltaY
            }
            dynCall_vidd(GLFW.active.scrollFunc, GLFW.active.id, sx, sy);
            event.preventDefault()
        };
        Module["canvas"].addEventListener("wheel", GLFW.onMouseWheel, true);
        Module["canvas"].addEventListener("mousewheel", GLFW.onMouseWheel, true);

        window.removeEventListener("keydown", GLFW.onKeydown, true);
        GLFW.onKeydown = function (event) {
            GLFW.onKeyChanged(event.keyCode, 1);
        };
        window.addEventListener("keydown", GLFW.onKeydown, true);

        var PinchZoomState = {
            type: "_PinchZoom",
            target: Module["canvas"],
            dist: 0.0,
            delta: 0.0,
            deltaX: 0.0,
            deltaY: 0.0,
            ntouches: 0,
            focus_first: 0,
        };

        Browser.getMouseWheelDelta = function(event) {
            var delta = 0;
            switch (event.type) {
                case "_PinchZoom":
                    delta = event.delta;
                    break;
                case "DOMMouseScroll":
                    delta = event.detail / 3;
                    break;
                case "mousewheel":
                    delta = event.wheelDelta / 120;
                    break;
                case "wheel":
                    delta = event.deltaY;
                    switch (event.deltaMode) {
                        case 0:
                            delta /= 100;
                            break;
                        case 1:
                            delta /= 3;
                            break;
                        case 2:
                            delta *= 80;
                            break;
                        default:
                            throw "unrecognized mouse wheel delta mode: " + event.deltaMode
                    }
                    break;
                default:
                    throw "unrecognized mouse wheel event: " + event.type
            }
            return delta;
        };

        Browser.calculateMouseEvent = function(event) {
            if (Browser.pointerLock) {
                if (event.type != "mousemove" && "mozMovementX"in event) {
                    Browser.mouseMovementX = Browser.mouseMovementY = 0
                } else {
                    Browser.mouseMovementX = Browser.getMovementX(event);
                    Browser.mouseMovementY = Browser.getMovementY(event)
                }
                if (typeof SDL != "undefined") {
                    Browser.mouseX = SDL.mouseX + Browser.mouseMovementX;
                    Browser.mouseY = SDL.mouseY + Browser.mouseMovementY
                } else {
                    Browser.mouseX += Browser.mouseMovementX;
                    Browser.mouseY += Browser.mouseMovementY
                }
            } else {
                var rect = Module["canvas"].getBoundingClientRect();
                var cw = Module["canvas"].width;
                var ch = Module["canvas"].height;
                var scrollX = typeof window.scrollX !== "undefined" ?
                    window.scrollX : window.pageXOffset;
                var scrollY = typeof window.scrollY !== "undefined" ?
                    window.scrollY : window.pageYOffset;
                if (event.type === "touchstart" ||
                    event.type === "touchend" || event.type === "touchmove") {
                    var touches = event.touches;
                    if (touches === undefined ||
                        touches.length < 1) {
                        PinchZoomState.ntouches = 0;
                        return;
                    }
                    if (event.type === "touchstart"){
                        PinchZoomState.focus_first = (PinchZoomState.ntouches === 1);
                        PinchZoomState.ntouches = touches.length;
                    }

                    if (touches.length === 2) {
                        // Pinch to zoom
                        var touch1 = touches[0],
                            touch2 = touches[1];
                        if (PinchZoomState.focus_first) {
                            var adjustedX = touch1.pageX - (scrollX + rect.left);
                            var adjustedY = touch1.pageY - (scrollY + rect.top);
                        } else {
                            var adjustedX = (touch1.pageX + touch2.pageX)/2.
                                - (scrollX + rect.left);
                            var adjustedY = (touch1.pageY + touch2.pageY)/2.
                                - (scrollY + rect.top);
                        }
                        adjustedX = adjustedX * (cw / rect.width);
                        adjustedY = adjustedY * (ch / rect.height);
                        var coords = {
                            x: adjustedX,
                            y: adjustedY
                        };
                        var dist = Math.hypot(
                            touch1.pageX - touch2.pageX,
                            touch1.pageY - touch2.pageY);
                        if (event.type === "touchstart"){
                            PinchZoomState.dist = dist;
                        } else {
                            PinchZoomState.deltaY =
                                PinchZoomState.delta =
                                (PinchZoomState.dist - dist) * 1.5;
                            PinchZoomState.dist = dist;
                            GLFW.onMouseWheel(PinchZoomState);
                        }
                    } else {
                        var touch = touches[0];
                        var adjustedX = touch.pageX - (scrollX + rect.left);
                        var adjustedY = touch.pageY - (scrollY + rect.top);
                        adjustedX = adjustedX * (cw / rect.width);
                        adjustedY = adjustedY * (ch / rect.height);
                        var coords = {
                            x: adjustedX,
                            y: adjustedY
                        };
                        if (event.type === "touchstart") {
                            Browser.lastTouches[touch.identifier] = coords;
                            Browser.touches[touch.identifier] = coords;
                        } else if (event.type === "touchend" ||
                            event.type === "touchmove") {
                            var last = Browser.touches[touch.identifier];
                            if (!last)
                            last = coords;
                            Browser.lastTouches[touch.identifier] = last;
                            Browser.touches[touch.identifier] = coords;
                        }
                    }
                    Browser.mouseX = coords.x;
                    Browser.mouseY = coords.y;
                    return;
                }
                var x = event.pageX - (scrollX + rect.left);
                var y = event.pageY - (scrollY + rect.top);
                x = x * (cw / rect.width);
                y = y * (ch / rect.height);
                Browser.mouseMovementX = x - Browser.mouseX;
                Browser.mouseMovementY = y - Browser.mouseY;
                Browser.mouseX = x;
                Browser.mouseY = y
            }
        };

        $('#marker').mouseenter(e => {
            $('#marker').css('opacity', '0');
            setTimeout(
                function() {$('#marker').css('display', 'none');}, 200)
        });

        // View
        ViewConfig.updateViewBounds();
        for (var i = 0; i < ViewConfig.vbounds.length; i++) {
            var b = ViewConfig.vbounds[i];
            var vbound = $('#view-' + b);
            vbound.on('input', function() {
                Module.set_view(
                    Number.parseFloat($('#view-xmin').val()),
                    Number.parseFloat($('#view-xmax').val()),
                    Number.parseFloat($('#view-ymin').val()),
                    Number.parseFloat($('#view-ymax').val()));
                Module.redraw();
            });
        }

        $('#btn-reset-view').click(e => { ViewConfig.resetView(); });

        ViewConfig.updateViewPolar();
        $('#view-polar').change(function(){
            Module.set_is_polar_grid(
                $('#view-polar').prop('checked')
            );
            Module.redraw();
        });

        // Sliders
        $('#btn-add-slider').click(e => {
        });
    });
};

var cppNotifyFocusEditor = function(k) {
    // C++ side wants to focus editor
    // (user interaction with canvas/keyboard entry)
    setTimeout(function() {
        $('#function-expr-f' + k).focus();
    }, 250);
}

var cppNotifyMarker = function(x, y) {
    // Marker 'may' have changed
    var marker_text = Module.get_marker_text();
    var canvas_off = $('#canvas').offset();
    if(marker_text.length == 0) {
        if ($('#marker').css('display') == 'block') {
            $('#marker').css('opacity', '0');
            setTimeout(
                function() {$('#marker').css('display', 'none');}, 200)
        }
    } else {
        $('#marker').css('top', canvas_off.top +  y);
        $('#marker').css('left', canvas_off.left + x);
        $('#marker').text(marker_text);
        if ($('#marker').css('display') == 'none') {
            $('#marker').css('display', 'block');
            $('#marker').css('opacity', '0');
            setTimeout(
                function() {$('#marker').css('opacity', '1');}, 50)
        }
    }
}

var onResizeCanvas = function() {
    // Handle window resize
    document.getElementById("canvas").width =
        window.innerWidth - $('#sidebar').outerWidth();
    document.getElementById("canvas").height = window.innerHeight;
    Module.redraw();
}

var Module = {
    preRun: [],
    postRun: [],
    print: (function() {})(),
    printErr: function(text) {},
    canvas: (function() {
        var canvas = document.getElementById('canvas');
        canvas.addEventListener("webglcontextlost", function(e) {
            e.preventDefault(); }, false);
        return canvas;
    })(),
    setStatus: function(text) {},
    totalDependencies: 0,
    monitorRunDependencies: function(left) {},
    onRuntimeInitialized: runtimeInitialized
};
