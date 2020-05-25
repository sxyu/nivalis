// Emscripten GLFW monkey patch
// - removed undesired fullscreen request on resize
// - remove e.preventDefault() in keydown handler which broke backspace
var glfwPatch = function() {
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
};
