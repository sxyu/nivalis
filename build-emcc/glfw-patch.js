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

// Additional handlers
window.addEventListener('resize', js_resizeCanvas, false);
function js_resizeCanvas() {
    document.getElementById('canvas').width = window.innerWidth;
    document.getElementById('canvas').height = window.innerHeight;
}
document.addEventListener('keypress',function(e){
    e.preventDefault();
});
document.addEventListener('keydown',function(e){
    if (event.ctrlKey==true &&
        (event.which == '61' || event.which == '107' ||
            event.which == '173' || event.which == '109'  || event.which == '187'  ||
            event.which == '189' ||
            event.which == '72' || event.which == '67' || event.which == '86' ||
            event.which == '88' || event.which == '79' || event.which == '80')) {
        event.preventDefault();
    }
});
document.body.addEventListener("wheel", e=>{
    if(e.ctrlKey)
        event.preventDefault();//prevent zoom
});
if (is_mobile) {
    document.body.addEventListener("click", e=>{
        var text_input = document.getElementById("text-input");
        text_input.focus();
        text_input.click();
    }, true);
}
var download = function(filename, text) {
    var element = document.createElement('a');
    element.setAttribute('href', 'data:text/plain;charset=utf-8,' + encodeURIComponent(text));
    element.setAttribute('download', filename);

    element.style.display = 'none';
    document.body.appendChild(element);

    element.click();

    document.body.removeChild(element);
};
