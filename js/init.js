let setupHandlers = function() {
    // Resize handler
    window.addEventListener('resize', onResizeCanvas, false);

    let canvas = document.getElementById("canvas");
    FuncEdit.new_func(false);
    let mf1 = $('#function-expr-' + FuncEdit.func_names[0]);
    mf1.mousedown();
    mf1.mouseup();
    document.addEventListener('keydown', function(e){
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
        Nivalis.on_key(e.which, e.ctrlKey, e.shiftKey, e.altKey);
        if (e.which === 69) {
            let tb = $('#function-expr-' + FuncEdit.func_names[FuncEdit.last_focus]);
            tb.mousedown();
            tb.mouseup();
            e.preventDefault();
        } else {
            ViewConfig.updateViewOptions();
            ViewConfig.updateViewBounds();
        }
        Renderer.redraw();
    });
    canvas.addEventListener("mousedown", function(e){
        if (e.button == 0) {
            let oleft = $(canvas).offset().left;
            let otop = $(canvas).offset().top;
            Nivalis.on_mousedown(e.clientX - oleft, e.clientY - otop);
            Renderer.redraw();
        }
    });
    var pinch_zoom_dist = -1.;
    var touch_cen_x = -1.;
    var touch_cen_y = -1.;
    canvas.addEventListener("touchstart", function(e){
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
            Nivalis.on_mousedown(e.touches[0].pageX - oleft,
                e.touches[0].pageY - otop);
            Renderer.redraw();
            e.preventDefault();
        }
    });
    canvas.addEventListener("mousemove", function(e){
        if (e.button == 0) {
            let oleft = $(canvas).offset().left;
            let otop = $(canvas).offset().top;
            Nivalis.on_mousemove(e.clientX - oleft, e.clientY - otop);
            ViewConfig.updateViewBounds();
            Renderer.redraw();
        }
    });
    canvas.addEventListener("touchmove", function(e){
        if (e.touches.length > 0) {
            if (e.touches.length == 2) {
                let touch1 = e.touches[0],
                    touch2 = e.touches[1];
                let dist = Math.hypot(
                    touch1.pageX - touch2.pageX,
                    touch1.pageY - touch2.pageY);
                if (pinch_zoom_dist > 0.001) {
                    let delta_y = pinch_zoom_dist - dist;
                    Nivalis.on_mousewheel(delta_y < 0,
                        Math.max(Math.abs(delta_y) * 10., 1.0001),
                        touch_cen_x, touch_cen_y);
                }
                pinch_zoom_dist = dist;
            }
            let oleft = $(canvas).offset().left;
            let otop = $(canvas).offset().top;
            Nivalis.on_mousemove(e.touches[0].pageX - oleft,
                e.touches[0].pageY - otop);
            ViewConfig.updateViewBounds();
            Renderer.redraw();
            e.preventDefault();
        }
    });
    canvas.addEventListener("mouseup", function(e){
        if (e.button == 0) {
            let oleft = $(canvas).offset().left;
            let otop = $(canvas).offset().top;
            Nivalis.on_mouseup(e.clientX - oleft, e.clientY - otop);
            Renderer.redraw();
        }
    });
    canvas.addEventListener("touchend", function(){
        Nivalis.on_mouseup(0, 0);
        Renderer.redraw();
        pinch_zoom_dist = -1.;
    });
    canvas.addEventListener("wheel", function(e){
        if(e.ctrlKey)
            event.preventDefault();//prevent zoom
        Nivalis.on_mousewheel(e.deltaY < 0,
            15., e.offsetX, e.offsetY);
        Renderer.redraw();
        ViewConfig.updateViewBounds();
    });
};
let onInit = function() {
    setupHandlers()
    glfwPatch();

    // Hide marker if mouse somehow enters it
    $('#marker').mouseenter(function() {
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
            $('#view-toggle').children('.custom-caret').html(Util.icon_caret_down);
            $('#view-toggle').removeClass('active');
            setTimeout(
                function() {$('#view-config').css('display', 'none');}, 500)
        } else {
            $('#view-config').css('display', 'block');
            $('#view-toggle').children('.custom-caret').html(Util.icon_caret_up);
            $('#view-toggle').addClass('active');
            setTimeout(
                function() {$('#view-config').css('opacity', '1');}, 50)
        }
    });

    // Shell
    $('#open-shell').click(Shell.toggle);

    // IO
    $('#export-btn').click(function() {
        var now_str = new Date().toLocaleString().replace(/ /g, '_').replace(',', '');
        Util.download('nivalis_' +now_str + '.json', Nivalis.export_json(true));
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
                setTimeout(function() {
                    if ($('#import-file-dropdown').is(":hidden")){
                        $('#import-dropdown-toggle').dropdown('toggle');
                    }
                }, 50);
                return;
            }
            $('#import-file').siblings(".custom-file-label")
                .html('Import json success');
            Nivalis.import_json(content);
            if (Shell.initialized) {
                Shell.con.info("Import JSON success");
            }
            resync();
            Nivalis.redraw();
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
    $('#sidebar-dragger').on('mousedown touchstart', function(e) {
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
    $('#sidebar-dragger').dblclick(function() {
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
    $(document).on('mouseup touchend', function(){
        sbar_drag = false;
        pinch_zoom_dist = -1.;
    });
    if (Util.is_mobile) {
        Nivalis.set_marker_clickable_radius(16);
        Nivalis.set_passive_marker_click_drag_view(true);
    } else {
        Nivalis.set_marker_clickable_radius(10);
        Nivalis.set_passive_marker_click_drag_view(false);
    }

    var MQ = MathQuill.getInterface(2);

    MQ.config({
        // spaceBehavesLikeTab: true,
        leftRightIntoCmdGoes: 'up',
        restrictMismatchedBrackets: true,
        sumStartsWithNEquals: true,
        supSubsRequireOperand: true,
        charsThatBreakOutOfSupSub: '=<>',
        autoSubscriptNumerals: true,
        autoCommands: 'pi Pi theta vartheta Theta alpha beta gamma Gamma delta Delta zeta psi Psi phi Phi mu nu epsilon varepsilon eta kappa Kappa chi omega Omega tau Tau rho varrho iota xi Xi sqrt nthroot sum prod int choose binom and or',
        autoOperatorNames: 'sin cos tan arcsin arccos arctan sinh cosh tanh sgn exp log ln mod gcd lcm floor ceil round poly fpoly Fpoly rect frect Frect circ fcirc Fcirc ellipse fellipse Fellipse text lgamma fact ifact rifact fafact at',
        maxDepth: 1,
        // substituteTextarea: function() {
        //     return document.createElement('textarea');
        // },
    });
    let first_view_example = true;
    $('#example-dropdown-toggle').click(function() {
        if (!first_view_example) return;
        first_view_example = false;
        setTimeout(function() {
            // Initialize examples the first time
            // dropdown is opened
            // Must be after open, since o.w.
            // paren heights will be wrong
            $('.example').each(function() {
                if (!$(this).hasClass('nomath')) {
                    let m = MQ.StaticMath(this);
                    if (this.hasAttribute('expr')) {
                        m.latex($(this).attr('expr'));
                    }
                }
            });
        }, 1);
    });
    // Fix weird disappearing parentheses
    // $('.mq-paren').css('transform','scale(1.2, 1.2)')
    $('.example').click(function(){
        let $this = $(this);
        let fidx = FuncEdit.func_names.length-1;
        if (FuncEdit.func_names.length == 2 &&
            Nivalis.get_func_expr(0).trim() === '') {
            fidx = 0;
        }
        if (this.hasAttribute('load')) {
            Util.getFile($this.attr("load"), function(err, data) {
                if (err === null) {
                    Nivalis.import_json(data);
                    resync();
                    Nivalis.redraw();
                } else {
                    console.log("Failed to retrieve sample JSON file");
                }
            });
        } else {
            let fn = FuncEdit.func_names[fidx];
            let mf = FuncEdit.func_mfields[fidx];
            let expr;
            if (this.hasAttribute('expr')) {
                expr = $(this).attr('expr');
            } else {
                expr = $this.children('.mq-selectable').text();
            }
            expr = expr.substr(1, expr.length - 2);
            mf.latex(expr.trim());
            if (this.hasAttribute('tmax')) {
                $('#function-tmax-' + fn).val($this.attr('tmax'));
                Nivalis.set_func_tmax(fidx, Number.parseFloat($this.attr('tmax')));
            }
            if (this.hasAttribute('tmin')) {
                $('#function-tmin-' + fn).val($this.attr('tmin'));
                Nivalis.set_func_tmin(fidx, Number.parseFloat($this.attr('tmin')));
            }
            let expr_ele = $('#function-expr-' + fn);
            expr_ele.mousedown();
            expr_ele.mouseup();
            FuncEdit.reparse(fn);
            Nivalis.redraw();
        }
    });

    // Saves
    Saves.init();

    // Close loading screen
    document.getElementById("loading").style.opacity = '0.0';
    window.setTimeout(function() {
        document.getElementById("loading").style.display =
            'none';
    }, 300);
    let xs = $('#main-wrapper').css('flex-direction') !== 'row';
    if (xs) {
        new_hi = window.innerHeight * 0.4;
        $('#sidebar').css('min-height', new_hi);
        $('#sidebar').css('max-height', new_hi);
    }
    onResizeCanvas();

    // Init sortable
    let funcs = document.getElementById('functions');
    // const sortable =
    new Sortable(funcs, {
        handle: '.draggable',
        filter: '.inactive',
        animation: 150,
        onMove: function (e) {
            if ($(e.related).hasClass('inactive')) return false;
            let from_name = e.dragged.id.substr(16);
            let to_name = e.related.id.substr(16);
            let from_idx = FuncEdit.func_indices[from_name];
            let to_idx = FuncEdit.func_indices[to_name];
            if (from_idx === to_idx) return true;

            let step = from_idx > to_idx ? -1 : 1;
            let tmp_mf = FuncEdit.func_mfields[from_idx];
            for (let i = from_idx; i != to_idx; i += step) {
                let j = i + step;
                FuncEdit.func_names[i] = FuncEdit.func_names[j];
                FuncEdit.func_mfields[i] = FuncEdit.func_mfields[j];
                FuncEdit.func_indices[FuncEdit.func_names[i]] = i;
            }
            FuncEdit.func_names[to_idx] = from_name;
            FuncEdit.func_mfields[to_idx] = tmp_mf;
            FuncEdit.func_indices[from_name] = to_idx;

            // Move call
            Nivalis.move_func(from_idx, to_idx);
            Nivalis.redraw();
            return true;
        }
    });

    // Set initial carets
    $('.custom-caret').each(function() {$(this).html(Util.icon_caret_down); });
};
