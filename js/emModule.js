var cppNotifyFocusEditor = function(k) {
    // C++ side wants to focus editor
    // (user interaction with canvas/keyboard entry)
    setTimeout(function() {
        $('#function-expr-f' + k).focus();
    }, 250);
};

var cppNotifyMarker = function(x, y) {
    // Marker 'may' have changed
    var marker_text = Module.get_marker_text();
    var canvas_off = $('#canvas').offset();
    if(marker_text.length === 0) {
        if ($('#marker').css('display') === 'block') {
            $('#marker').css('opacity', '0');
            setTimeout(
                function() {$('#marker').css('display', 'none');}, 200)
        }
    } else {
        $('#marker').css('top', canvas_off.top + y +
                         (Util.is_mobile ? -70 : 0));
        $('#marker').css('left', canvas_off.left + x +
                         (Util.is_mobile ? -160 : 0));
        $('#marker').text(marker_text);
        if ($('#marker').css('display') === 'none') {
            $('#marker').css('display', 'block');
            $('#marker').css('opacity', '0');
            setTimeout(
                function() {$('#marker').css('opacity', '1');}, 50)
        }
    }
};

var cppNotifyFuncErrorChanged = function() {
    // Function error 'may' have changed
    $('#function-error').text(Module.get_func_error());
};

var cppNotifySliderErrorChanged = function() {
    // Slider error 'may' have changed
    $('#slider-error').text(Module.get_slider_error());
};

var customWidth = -1;
var fromXSMode = false;
var onResizeCanvas = function() {
    // Handle window resize
    let xs = $('#main-wrapper').css('flex-direction') !== 'row';
    let new_wid, new_hi;
    if (xs) {
        new_wid = window.innerWidth;
        let total_height = (window.innerHeight - $('#header').outerHeight());
        new_hi = total_height - $('#sidebar').height()
                    - $('#sidebar-dragger').height();
        $('#sidebar').css('min-width', new_wid);
        $('#sidebar').css('max-width', new_wid);
    } else {
        new_wid = window.innerWidth - $('#sidebar').outerWidth() -
            $('#sidebar-dragger').outerWidth();
        if ($('#sidebar').hasClass('collapse')) {
            new_wid += $('#sidebar').outerWidth();
        }
        new_hi = window.innerHeight - $('#header').outerHeight();
        $('#sidebar').css('min-height', new_hi);
        $('#sidebar').css('max-height', new_hi);
        if (customWidth >= 0 && fromXSMode) {
            $('#sidebar').css('min-width', customWidth);
            $('#sidebar').css('max-width', customWidth);
        }
        else customWidth = $('#sidebar').width();
    }
    fromXSMode = xs;
    document.getElementById("canvas").width = new_wid;
    document.getElementById("canvas").height = new_hi;
    Module.redraw();
};

var Module = {
    preRun: [],
    postRun: [],
    print: (function() {})(),
    printErr: function() {},
    canvas: (function() {
        var canvas = document.getElementById('canvas');
        canvas.addEventListener("webglcontextlost", function(e) {
            e.preventDefault(); }, false);
        return canvas;
    })(),
    setStatus: function() {},
    totalDependencies: 0,
    monitorRunDependencies: function() {},
    onRuntimeInitialized: function() { $(document).ready(onInit); }
};
