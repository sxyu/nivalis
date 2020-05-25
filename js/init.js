let setupHandlers = function() {
    // Resize handler
    window.addEventListener('resize', onResizeCanvas, false);

    let canvas = document.getElementById("canvas");
    FuncEdit.new_func(false);
    $('#function-expr-' + FuncEdit.func_names[0]).focus();
    document.addEventListener('keydown', e=>{
        // Must either have ctrl down OR nothing selected to activate
        if (event.ctrlKey===false &&
            $('*:focus').length > 0) return;
        if (event.ctrlKey===true &&
            (event.which === 61 || event.which === 107 ||
                event.which === 173 || event.which === 109  || event.which === 187  ||
                event.which === 189 || event.which === 72 ||
                event.which === 79 || event.which === 80)) {
            event.preventDefault();
        }
        Module.on_key(e.which, e.ctrlKey, e.shiftKey, e.altKey);
        ViewConfig.updateViewPolar();
        ViewConfig.updateViewBounds();
        Renderer.redraw();
    });
    canvas.addEventListener("mousedown", e=>{
        if (e.button == 0) {
            Module.on_mousedown(e.pageX, e.pageX);
            Renderer.redraw();
        }
    });
    var pinch_zoom_dist = -1.;
    var touch_cen_x = -1.;
    var touch_cen_y = -1.;
    canvas.addEventListener("touchstart", e=>{
        if (e.touches.length > 0) {
            touch_cen_x = 0.; touch_cen_y = 0.;
            for (let i = 0; i < e.touches.length; i++) {
                touch_cen_x += e.touches[i].pageX;
                touch_cen_y += e.touches[i].pageY;
            }
            touch_cen_x /= e.touches.length;
            touch_cen_y /= e.touches.length;
            let oleft = $(canvas).offset().left;
            let otop = $(canvas).offset().top;
            touch_cen_x -= oleft;
            touch_cen_y -= otop;
            if (e.touches.length == 2) {
                let touch1 = e.touches[0],
                    touch2 = e.touches[1];
                let dist = Math.hypot(
                    touch1.pageX - touch2.pageX,
                    touch1.pageY - touch2.pageY);
                pinch_zoom_dist = dist;
            }
            Module.on_mousedown(e.touches[0].pageX - oleft,
                e.touches[0].pageY - otop);
            Renderer.redraw();
            e.preventDefault();
        }
    });
    canvas.addEventListener("mousemove", function(e){
        if (e.button == 0) {
            Module.on_mousemove(e.pageX, e.pageY);
            ViewConfig.updateViewBounds();
            Renderer.redraw();
        }
    });
    canvas.addEventListener("touchmove", e=>{
        if (e.touches.length > 0) {
            if (e.touches.length == 2) {
                let touch1 = e.touches[0],
                    touch2 = e.touches[1];
                let dist = Math.hypot(
                    touch1.pageX - touch2.pageX,
                    touch1.pageY - touch2.pageY);
                if (pinch_zoom_dist > 0.001) {
                    let delta_y = pinch_zoom_dist - dist;
                    Module.on_mousewheel(delta_y < 0,
                        Math.max(Math.abs(delta_y) * 2., 1.0001),
                        touch_cen_x, touch_cen_y);
                }
                pinch_zoom_dist = dist;
            }
            let oleft = $(canvas).offset().left;
            let otop = $(canvas).offset().top;
            Module.on_mousemove(e.touches[0].pageX - oleft,
                e.touches[0].pageY - otop);
            ViewConfig.updateViewBounds();
            Renderer.redraw();
            e.preventDefault();
        }
    });
    canvas.addEventListener("mouseup", e=>{
        if (e.button == 0) {
            Module.on_mouseup(e.pageX, e.pageY);
            Renderer.redraw();
        }
    });
    canvas.addEventListener("touchend", e=>{
        Module.on_mouseup(0, 0);
        Renderer.redraw();
        pinch_zoom_dist = -1.;
    });
    canvas.addEventListener("wheel", e=>{
        if(e.ctrlKey)
            event.preventDefault();//prevent zoom
        Module.on_mousewheel(e.deltaY < 0,
            Math.abs(e.deltaY) * 5., e.offsetX, e.offsetY);
        Renderer.redraw();
        ViewConfig.updateViewBounds();
    });
};
let onInit = function() {
    setupHandlers()

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

    glfwPatch();

    // Hide marker if mouse somehow enters it
    $('#marker').mouseenter(e => {
        $('#marker').css('opacity', '0');
        setTimeout(
            function() {$('#marker').css('display', 'none');}, 200)
    });

    // View
    ViewConfig.init();
    // View toggle
    $('#view-toggle').click(function() {
        if ($('#view-config').css('display') ===  'block') {
            $('#view-config').css('opacity', '0');
            $('#view-toggle').html("View <ion-icon name='caret-down'  class='toggle-caret'></ion-icon>");
            $('#view-toggle').removeClass('active');
            setTimeout(
                function() {$('#view-config').css('display', 'none');}, 500)
        } else {
            $('#view-config').css('display', 'block');
            $('#view-toggle').html("View <ion-icon name='caret-up'  class='toggle-caret'></ion-icon>");
            $('#view-toggle').addClass('active');
            setTimeout(
                function() {$('#view-config').css('opacity', '1');}, 50)
        }
    });

    // Shell
    $('#open-shell').click(Shell.toggle);

    // IO
    $('#open-export').click(function() {
        var now_str = new Date().toLocaleString().replace(/ /g, '_').replace(',', '');
        Util.download('nivalis_' +now_str + '.json', Module.export_json());
    });
    $('#import-btn').click(function() {
        var err_txt = $('#import-error');
        err_txt.text('');
        var files = $('#import-file')[0].files;
        if (files.length === 0) return;

        var reader = new FileReader();
        reader.onload = function(event) {
            var content = event.target.result;
            try {
                var _ = JSON.parse(content);
            } catch (e) {
                err_txt.text('File is not in JSON format');
                // Force dropdown open
                if ($('#import-file-dropdown').is(":hidden")){
                    $('#import-dropdown-toggle').dropdown('toggle');
                }
                return;
            }
            $('#import-file').siblings(".custom-file-label")
                .html('Import json success');
            Module.import_json(content);
            Shell.con.info("Import JSON success");
            resync();
            Module.redraw();
        };
        reader.readAsText(files[0]);
    });

    $(".custom-file-input").on("change", function() {
        var fileName = $(this).val().split("\\").pop();
        $(this).siblings(".custom-file-label").addClass("selected").html(fileName);
    });

    // Sliders
    Sliders.init();

    // Sidebar resizing
    var sbar_drag_x, sbar_drag_y;
    var sbar_drag = false;
    var sidebar_resize = function(e) {
        if (sbar_drag === true) {
            let touch = e.type == 'touchmove';
            if (touch && e.touches.length == 0) return;
            let x = touch ? e.touches[0].screenX : e.pageX;
            let y = touch ? e.touches[0].screenY : e.pageY;
            const dx = sbar_drag_x - x;
            const dy = sbar_drag_y - y;
            sbar_drag_x = x;
            sbar_drag_y = y;
            let sbar = $('#sidebar');
            let drag = $('#sidebar-dragger');
            if (drag.width() < drag.height()) {
                let new_wid = Math.min(
                    Math.max(sbar.width() - dx,
                        Math.max(150, window.innerWidth * 0.1)),
                    window.innerWidth - 50);
                sbar.css('min-width', new_wid);
                sbar.css('max-width', new_wid);
            } else {
                let new_hi = Math.min(
                    Math.max(sbar.height() + dy, 0),
                    window.innerHeight - 150);
                sbar.css('min-height', new_hi);
                sbar.css('max-height', new_hi);
            }
            onResizeCanvas();
            if (touch) e.preventDefault();
        }
    };
    $(document).on('mousemove', sidebar_resize);
    $('#sidebar-dragger').on('touchmove', sidebar_resize);
    $('#sidebar-dragger').on('mousedown touchstart', e => {
        let touch = e.type == 'touchstart';
        if (touch && e.touches.length == 0) return;
        sbar_drag_x = touch ? e.touches[0].screenX : e.pageX;
        sbar_drag_y = touch ? e.touches[0].screenY : e.pageY;
        if (touch) e.preventDefault();
        if ($('#sidebar').hasClass('collapse')) {
            $('#sidebar').removeClass('collapse');
            $('#sidebar-dragger').removeClass('minimize');
            onResizeCanvas();
        } else {
            sbar_drag = true;
        }
    });
    $('#sidebar-dragger').dblclick(e => {
        if (!$('#sidebar').hasClass('collapse')) {
            let drag = $('#sidebar-dragger');
            if (drag.width() < drag.height()) {
                $('#sidebar').addClass('collapse');
                $('#sidebar-dragger').addClass('minimize');
                onResizeCanvas();
            }
        }
    });
    $('#navbar-collapse-toggler').click(function(){
        window.setTimeout(function() {
            onResizeCanvas();
        }, 400);
    });
    $(document).on('mouseup touchend', e => {
        sbar_drag = false;
        pinch_zoom_dist = -1.;
    });
    if (Util.is_mobile) {
        Module.set_marker_clickable_radius(12);
    }
};
