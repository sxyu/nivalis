var Renderer = {
    redraw_cnt: 0,
    _redraw : function() {
        Renderer.redraw_cnt -= 1;
        Module.redraw();
        if (Renderer.redraw_cnt > 0 ||
            Module.is_any_slider_animating()) {
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
    func_mfields: [],
    func_indices: {},
    reparse : function(name) {
            var idx = FuncEdit.func_indices[name];
            Module.set_func_expr(idx, FuncEdit.func_mfields[idx].latex());
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
        var MQ = MathQuill.getInterface(2);
        var sm = MQ.StaticMath($('#function-name-' + fname)[0]);
        sm.latex("f_{" + fname.substr(1) + "}");
        var editor_tb = $('#function-expr-' + fname);
        const mf = MQ.MathField(editor_tb[0],
            {
                handlers: {
                    edit: function(mathField) {
                        var this_name = mathField.el().id.substr(14);
                        FuncEdit.reparse(this_name);
                        let tex = mathField.latex();
                        if (tex == "#") {
                            mathField.write("\ \\text{comment}");
                        } else {
                            const opn_str = "\\operatorname{";
                            if (tex.startsWith(opn_str) && tex[tex.length - 1] == '}') {
                                let tex_opn = tex.substr(opn_str.length, tex.length - opn_str.length - 1);
                                if (tex_opn.length <= 1) return;
                                if (tex_opn[0] == 'f' || tex_opn[0] == 'F')
                                    tex_opn = tex_opn.substr(1);
                                if (tex_opn == "text") {
                                    mathField.write("\\ \\text{edit me}\\ \\operatorname{at}\ \\ \\left(0, 0\\right)");
                                } else if (tex_opn == "poly") {
                                    mathField.write("\\ ");
                                    mathField.typedText("(");
                                } else if (tex_opn == "rect") {
                                    mathField.write("\\ \\left(0,0\\right),\\ \\left(1,1\\right)");
                                } else if (tex_opn == "circ") {
                                    mathField.write("\\ 1\\ \\operatorname{at}\ \\ \\left(0, 0\\right)");
                                } else if (tex_opn == "ellipse") {
                                    mathField.write("\\ (1,1)\\\ \\operatorname{at}\ \\ \\left(0, 0\\right)");
                                }
                            }
                        }
                    },
                    upOutOf: function(mathField) {
                        var this_name = mathField.el().id.substr(14);
                        var idx = FuncEdit.func_indices[this_name];
                        if (idx > 0) {
                            var next_tb = $('#function-expr-' + FuncEdit.func_names[idx - 1]);
                            next_tb.mousedown(); next_tb.mouseup();
                        }
                    },
                    downOutOf: function(mathField) {
                        var this_name = mathField.el().id.substr(14);
                        var idx = FuncEdit.func_indices[this_name];
                        if (idx + 1 === Module.num_funcs()) {
                            FuncEdit.new_func(true);
                        }
                        var next_tb = $('#function-expr-' + FuncEdit.func_names[idx + 1]);
                        next_tb.mousedown(); next_tb.mouseup();
                    },
                }
            });
        FuncEdit.func_mfields.push(mf);
        mf.latex(Module.get_func_expr(fidx));

        editor_tb.mousedown(function() {
            // Focus (change curr func)
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
        editor_tb.keyup(function() {
            var this_name = this.id.substr(14);
            FuncEdit.reparse(this_name);
        });
        $('#function-tmin-' + fname).val(Math.floor(Module.get_func_tmin(fidx) * 1e6)/1e6);
        $('#function-tmax-' + fname).val(Math.ceil(Module.get_func_tmax(fidx) * 1e6)/1e6);
        $('#function-tmin-' + fname).on('input', function() {
            Module.set_func_tmin(fidx, Number.parseFloat(this.value));
            Module.redraw();
        });
        $('#function-tmax-' + fname).on('input', function() {
            Module.set_func_tmax(fidx, Number.parseFloat(this.value));
            Module.redraw();
        });
        $('#function-del-' + fname).click(function() {
            // Delete
            var this_name = this.id.substr(13);
            var idx = FuncEdit.func_indices[this_name];
            if (FuncEdit.func_names.length > idx + 1) {
                Module.delete_func(idx);
                Module.redraw();
                $('#function-widget-' + this_name).remove();
                FuncEdit.func_names.splice(idx, 1);
                delete FuncEdit.func_indices[this_name];
                FuncEdit.func_mfields.splice(idx, 1);
                for (var t = idx; t < FuncEdit.func_names.length; t++) {
                    FuncEdit.func_indices[FuncEdit.func_names[t]] -= 1;
                }
            }
        });
        // Color picker
        const pickr = Pickr.create(Object.assign(
            {el: '#color-picker-' +  fname}, pickrConfig
        ));
        pickr.on("init", function(instance) {
            var this_name = instance.options.el.id.substr(13);
            var idx = FuncEdit.func_indices[this_name];
            pickr.setColor("#" + Module.get_func_color(idx));
        });
        pickr.on("change", function(color, instance) {
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
        var init_min = Module.get_slider_lo(sidx);
        var init_max = Module.get_slider_hi(sidx);
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
            var slider = $('#slider-sli-' + sid);
            slider.attr('min', init_min);
            slider.attr('max', init_max);
            slider.val(Module.get_slider_val(sidx))
            slider.on('input', sliderChangeVal);
        }
        {
            var slider_val = $('#slider-val-' + sid);
            slider_val.val(Module.get_slider_val(sidx))
            slider_val.on('input', sliderChangeVal);
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
        {
            var slider_ani = $('#slider-ani-' + sid);
            slider_ani.click(function() {
                var this_sid = this.id.substr(11);
                var idx = Sliders.slider_indices[this_sid];
                if (Module.slider_animation_dir(idx) != 0) {
                    Module.end_slider_animation(idx);
                    $(this).html('<ion-icon name="play" class="toggle-caret"></ion-icon>')
                } else {
                    Module.begin_slider_animation(idx);
                    $(this).html('<ion-icon name="stop" class="toggle-caret"></ion-icon>')
                }
                Renderer.redraw();
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

var Saves = {
    add_save_menu_item: function(i) {
        let savei = localStorage['save' + i] || 0;
        if (savei.length > 0) {
            $('#save-divider-bottom').before($('#save-template').html().replace(
                new RegExp('{name}', 'g'), i));
            let menuitem = $('#save-' + i);
            menuitem.click(function() {
                let this_save = this.id.substr(5);
                Module.import_json(localStorage['save' + this_save]);
                resync();
                Module.redraw();
            });
        }
    },
    _force_dropdown_open: function() {
        // Force dropdown open
        setTimeout(function() {
            if ($('#save-dropdown').is(":hidden")){
                $('#save-dropdown-toggle').dropdown('toggle');
            }
        }, 50);
    },
    init: function() {
        $('#save-btn').click(function() {
            let nsaves = Number.parseInt(localStorage['nsaves']) || 0;
            localStorage.setItem('nsaves', nsaves + 1);
            localStorage.setItem('save' + nsaves, Module.export_json());
            Saves.add_save_menu_item(nsaves);
            Saves._force_dropdown_open();
        });
        $('#save-clear-btn').click(function() {
            let nsaves = Number.parseInt(localStorage['nsaves']) || 0;
            localStorage.setItem('nsaves', 0);
            for (var i = 0; i < nsaves; i++) {
                localStorage.removeItem('save' + i);
                $('#save-' + i).remove();
            }
            Saves._force_dropdown_open();
        });

        let nsaves = Number.parseInt(localStorage['nsaves']) || 0;
        for (var i = 0; i < nsaves; i++) {
            Saves.add_save_menu_item(i);
        }
    }
}

// Re-synchronize all functions/sliders
var resync = function() {
    $('#functions').html('');
    $('#sliders').html('');
    FuncEdit.func_names = [];
    FuncEdit.func_indices = {};
    FuncEdit.func_mfields = [];
    Sliders.slider_ids = [];
    Sliders.slider_indices = [];
    for (let i = 0; i < Module.num_funcs(); i++ ){
        FuncEdit.new_func(false);
        FuncEdit.reparse(FuncEdit.func_names[i]);
    }
    for (let i = 0; i < Module.num_funcs() - 1; i++){
        let fn = FuncEdit.func_names[i];
        $('#function-name-' + fn).removeClass('inactive');
        $('#function-del-' + fn).removeClass('inactive');
    }
    for (let i = 0; i < Module.num_sliders(); i++){
        Sliders.new_slider(false);
    }
    $('#function-expr-' +
        FuncEdit.func_names[Module.get_curr_func()]).focus();
};
