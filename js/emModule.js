var cppNotifyFocusEditor = function(k) {
    // C++ side wants to focus editor
    // (user interaction with canvas/keyboard entry)
    setTimeout(function() {
        $('#function-expr-f' + k).focus();
    }, 250);
};

var cppNotifyMarker = function(x, y) {
    // Marker 'may' have changed
    var marker_text = Nivalis.get_marker_text();
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

var cppNotifyAnimSlider = function() {
    // Slider animation step occurred, need to update GUI
    for (var i = 0; i < Nivalis.num_sliders(); i++) {
        if (Nivalis.slider_animation_dir(i) != 0) {
            $('#slider-sli-' +
                Sliders.slider_ids[i]).val(Nivalis.get_slider_val(i));
        }
    }
};

var cppNotifyFuncErrorChanged = function() {
    // Function error 'may' have changed
    $('#function-error').text(Nivalis.get_func_error());
};

var cppNotifySliderErrorChanged = function() {
    // Slider error 'may' have changed
    $('#slider-error').text(Nivalis.get_slider_error());
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
    let canvas = document.getElementById("canvas");
    if (canvas.width != new_wid) {
        canvas.width = new_wid;
    }
    if (canvas.height != new_hi) {
        canvas.height = new_hi;
    }
    Nivalis.redraw_force();
};

var Nivalis = {
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
