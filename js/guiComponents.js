var Renderer = {
    redraw_cnt: 0,
    _redraw : function() {
        Renderer.redraw_cnt -= 1;
        Module.redraw();
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

var FuncEdit = {
    func_names: [],
    func_indices: {},
    reparse : function(name) {
            var idx = FuncEdit.func_indices[name];
            Module.set_func_expr(idx, $('#function-expr-'+name).val());
            var uses_t = Module.get_func_uses_t(idx);
            if (uses_t) {
                $('#function-tbounds-' + name).css('display', 'flex');
            } else {
                $('#function-tbounds-' + name).css('display', 'none');
            }
            Module.redraw();
    },
    new_func: function(add) {
        var fidx = FuncEdit.func_names.length;
        if (add) {
            success = Module.add_func();
            if (!success) return;
        }
        var fname = Module.get_func_name(fidx);
        $('#functions').append($('#function-form-template').html().replace(
                new RegExp('{name}', 'g'), fname));
        FuncEdit.func_names.push(fname);
        FuncEdit.func_indices[fname] = fidx;

        var editor_tb = $('#function-expr-' + fname);
        editor_tb.val(Module.get_func_expr(fidx));
        editor_tb.on('input', function() {
            // Change editor
            var this_name = this.id.substr(14);
            FuncEdit.reparse(this_name);
        });
        editor_tb.on('keyup', function(e) {
            // Up/down
            var $this = $(this);
            var this_name = this.id.substr(14);
            var idx = FuncEdit.func_indices[this_name];
            if (e.which === 38 && idx > 0) {
                e.preventDefault();
                var next_tb = $('#function-expr-' + FuncEdit.func_names[idx - 1]);
                next_tb.focus();
                // next_tb[0].selectionStart = next_tb[0].selectionEnd = next_tb[0].value.length;
            } else if (e.which === 40) {
                e.preventDefault();
                if (idx + 1 === Module.num_funcs()) {
                    FuncEdit.new_func(true);
                }
                var next_tb = $('#function-expr-' + FuncEdit.func_names[idx + 1]);
                next_tb.focus();
                // next_tb[0].selectionStart = next_tb[0].selectionEnd = next_tb[0].value.length;
            } else if (e.which === 27) {
                // Esc
                $this.blur();
            }
            Module.redraw();
        });
        editor_tb.focus(function() {
            // Focus (change curr func)
            var $this = $(this);
            var this_name = this.id.substr(14);
            var idx = FuncEdit.func_indices[this_name];
            if (idx === Module.num_funcs() - 1) {
                // Add function
                FuncEdit.new_func(true);
            }
            $('#function-name-' + this_name).removeClass('inactive');
            $('#function-del-' + this_name).removeClass('inactive');
            Module.set_curr_func(idx);
            Module.redraw();
        });
        editor_tb.blur(function() {
            // blur: redraw
            // hack to fix canvas size when closing mobile keyboard
            setTimeout(onResizeCanvas, 100);
        });
        $('#function-tmin-' + fname).val(Math.floor(Module.get_func_tmin(fidx) * 1e6)/1e6);
        $('#function-tmax-' + fname).val(Math.ceil(Module.get_func_tmax(fidx) * 1e6)/1e6);
        $('#function-tmin-' + fname).on('input', function() {
            var $this = $(this);
            var this_name = this.id.substr(14);
            var idx = FuncEdit.func_indices[this_name];
            var maxv = $('#function-tmax-' + fname).val();
            Module.set_func_tmin(fidx, Number.parseFloat($this.val()));
            Module.redraw();
        });
        $('#function-tmax-' + fname).on('input', function() {
            var $this = $(this);
            var this_name = this.id.substr(14);
            var idx = FuncEdit.func_indices[this_name];
            var minv = $('#function-tmin-' + fname).val();
            Module.set_func_tmax(fidx, Number.parseFloat($this.val()));
            Module.redraw();
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
            {el: '#color-picker-' +  fname}, pickrConfig
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

var Sliders = {
    slider_ids: [],
    slider_indices: {},
    last_id: 0,
    new_slider: function(add) {
        var sidx = Sliders.slider_ids.length;
        var sid = Sliders.last_id;
        Sliders.last_id += 1;
        if (add) Module.add_slider();
        Sliders.slider_ids.push(sid);
        Sliders.slider_indices[sid] = sidx;
        $('#sliders').append($('#slider-form-template').html().replace(
                new RegExp('{name}', 'g'), sid));
        {
            var slider_var = $('#slider-var-' + sid);
            slider_var.val(Module.get_slider_var(sidx));
            slider_var.on('input', function() {
                var $this = $(this);
                var this_sid = this.id.substr(11);
                var idx = Sliders.slider_indices[this_sid];
                Module.set_slider_var(idx, $this.val());
                Module.redraw();
            });
        }
        var sliderChangeVal = function() {
            var $this = $(this);
            var this_sid = this.id.substr(11);
            var idx = Sliders.slider_indices[this_sid];
            var new_val = Number.parseFloat($this.val());
            Module.set_slider_val(idx, new_val);
            Renderer.redraw();
            if ($this.hasClass('slider-sli')) {
                $('#slider-val-' + this_sid).val(new_val);
            } else {
                $('#slider-sli-' + this_sid).val(new_val);
            }
        };
        {
            var slider_val = $('#slider-val-' + sid);
            slider_val.val(Module.get_slider_val(sidx))
            slider_val.on('input', sliderChangeVal);
        }
        var init_min = Module.get_slider_lo(sidx);
        var init_max = Module.get_slider_hi(sidx);
        {
            var slider = $('#slider-sli-' + sid);
            slider.val(Module.get_slider_val(sidx))
            slider.attr('min', init_min);
            slider.attr('max', init_max);
            slider.on('input', sliderChangeVal);
        }
        {
            var sliderChangeBounds = function(e) {
                var this_sid = this.id.substr(11);
                var idx = Sliders.slider_indices[this_sid];
                var new_min = Number.parseFloat($('#slider-min-' + this_sid).val());
                var new_max = Number.parseFloat($('#slider-max-' + this_sid).val());
                Module.set_slider_lo_hi(idx, new_min, new_max);
                var this_slider = $('#slider-sli-' + this_sid);
                this_slider.attr('min', new_min);
                this_slider.attr('max', new_max);
            }
            var slider_min = $('#slider-min-' + sid);
            var slider_max = $('#slider-max-' + sid);
            slider_min.val(init_min)
            slider_max.val(init_max)
            slider_min.on('input', sliderChangeBounds);
            slider_max.on('input', sliderChangeBounds);
        }
        {
            var slider_del = $('#slider-del-' + sid);
            slider_del.click(function() {
                var this_sid = this.id.substr(11);
                var idx = Sliders.slider_indices[this_sid];
                $('#slider-' + this_sid).remove();
                $('#slider-bounds-' + this_sid).remove();
                Sliders.slider_ids.splice(idx, 1);
                delete Sliders.slider_indices[this_sid];
                for (var t = idx; t < Sliders.slider_ids.length; t++) {
                    Sliders.slider_indices[Sliders.slider_ids[t]] -= 1;
                }
                Module.delete_slider(idx);
                Module.redraw();
            });
        }
        Module.redraw();
    },
    init: function() {
        $('#btn-add-slider').click(function() {
            Sliders.new_slider(true);
        });
    },
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
    },
    init : function() {
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

        $('.reset-view').click(ViewConfig.resetView);

        ViewConfig.updateViewPolar();
        $('#view-polar').change(function(){
            Module.set_is_polar_grid(
                $('#view-polar').prop('checked')
            );
            Module.redraw();
        });
    }
};

var Shell = {
    initialized: false,
    prompt: function() {
        Shell.shell.write('>>> ');
    },
    con : new SimpleConsole({
        placeholder: "Enter Nivalis expression",
        autofocus: false,
        handleCommand: function(command){
            try {
                let ret = Module.shell_exec(command);
                if (ret) {
                    Shell.con.log(Module.get_shell_output());
                } else {
                    Shell.con.error(Module.get_shell_output());
                }
                $("#shell").scrollTop(
                    $('#shell').prop("scrollHeight"));
            } catch(error) {
                Shell.con.error(error);
            }
        },
        storageID: "shell-main",
    }),
    start: function() {
        if (Shell.initialized === false) {
            Shell.initialized = true;
            let init_output = Module.get_shell_output();
            Shell.con.info(init_output);
            $('#shell')[0].appendChild(Shell.con.element);
            $('#shell-close').click(function() {
                Shell.toggle();
            });
        }
    },
    toggle: function() {
        let shell = $('#shell');
        if (shell.css('display') == 'block') {
            shell.hide();
            $('#open-shell').html(
                'Shell '+
                '<ion-icon name="caret-down" class="toggle-caret"></ion-icon>')
            $('#open-shell').removeClass('active');
        } else {
            shell.height(window.innerHeight - 130);
            shell.show();
            $('#open-shell').addClass('active');
            Shell.start();
            $('#open-shell').html(
                'Shell '+
                '<ion-icon name="caret-up"  class="toggle-caret"></ion-icon>');
            $('#open-shell').addClass('active');
            $(this).blur();
            shell.find('input').focus();
        }
    },
};

// Re-synchronize all functions/sliders
var resync = function() {
    $('#functions').html('');
    $('#sliders').html('');
    FuncEdit.func_names = [];
    FuncEdit.func_indices = {};
    Sliders.slider_ids = [];
    Sliders.slider_indices = [];
    for (let i = 0; i < Module.num_funcs(); i++ ){
        FuncEdit.new_func(false);
    }
    for (let i = 0; i < Module.num_funcs() - 1; i++){
        let fn = FuncEdit.func_names[i];
        $('#function-name-' + fn).removeClass('inactive');
        $('#function-del-' + fn).removeClass('inactive');
    }
    for (let i = 0; i < Module.num_sliders(); i++ ){
        Sliders.new_slider(false);
    }
    $('#function-expr-' +
        FuncEdit.func_names[Module.get_curr_func()]).focus();
};
