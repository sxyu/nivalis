var Renderer = {
    redraw_cnt: 0,
    _redraw : function() {
        Renderer.redraw_cnt -= 1;
        Nivalis.redraw();
        if (Renderer.redraw_cnt > 0 ||
            Nivalis.is_any_slider_animating()) {
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
            Nivalis.set_func_expr(idx, FuncEdit.func_mfields[idx].latex());
            var uses_t = Nivalis.get_func_uses_t(idx);
            if (uses_t) {
                $('#function-tbounds-' + name).css('display', 'flex');
            } else {
                $('#function-tbounds-' + name).css('display', 'none');
            }
            Nivalis.redraw();
    },
    _render_func_type: function(name) {
        let idx = FuncEdit.func_indices[name];
        let type = Nivalis.get_func_type(idx);
        let type_nm = type & ~Nivalis.FUNC_TYPE_MOD_ALL;
        if ((type & Nivalis.FUNC_TYPE_MOD_INEQ) && type_nm <= Nivalis.FUNC_TYPE_IMPLICIT) {
            return '<span title="inequality"><svg style="width:22px;height:22px" viewBox="0 0 24 24"><path fill="#888" d="M22,21H2V3H4V15.54L9.5,6L16,9.78L20.24,2.45L21.97,3.45L22,21Z" /></svg></span>';
        }
        switch(type_nm) {
            case Nivalis.FUNC_TYPE_EXPLICIT:
                return '<span title="Explicit y=f(x); tip: you can use ' + name + '(x) elsewhere">f<sub>' + name.substr(1) + '</sub>&nbsp;(x)</span>';
            case Nivalis.FUNC_TYPE_EXPLICIT_Y:
                return '<span title="Explicit x=f(y); tip: you can use ' + name + '(y) elsewhere">f<sub>' + name.substr(1) + '</sub>&nbsp;(y)</span>';
            case Nivalis.FUNC_TYPE_IMPLICIT:
                return '<span title="Implicit function"><svg style="width:22px;height:22px" viewBox="0 0 24 24"><path fill="#888" d="M16,11.78L20.24,4.45L21.97,5.45L16.74,14.5L10.23,10.75L5.46,19H22V21H2V3H4V17.54L9.5,8L16,11.78Z" /></svg></span>';
            case Nivalis.FUNC_TYPE_PARAMETRIC:
                return '<span title="Parametric">(x,y)</span>';
            case Nivalis.FUNC_TYPE_POLAR:
                return '<span title="Polar"><svg style="width:22px;height:22px" viewBox="0 0 24 24"><path fill="#888" d="M20,19H4.09L14.18,4.43L15.82,5.57L11.28,12.13C12.89,12.96 14,14.62 14,16.54C14,16.7 14,16.85 13.97,17H20V19M7.91,17H11.96C12,16.85 12,16.7 12,16.54C12,15.28 11.24,14.22 10.14,13.78L7.91,17Z" /></svg></span>';
            case Nivalis.FUNC_TYPE_FUNC_DEFINITION:
                return '<span title="Definition" style="color:#999">def</span>';
            case Nivalis.FUNC_TYPE_COMMENT:
                return '<span title="Comment"><svg style="width:22px;height:22px" viewBox="0 0 24 24"><path fill="#aaa" d="M10,7L8,11H11V17H5V11L7,7H10M18,7L16,11H19V17H13V11L15,7H18Z"/></svg></span>';
            case Nivalis.FUNC_TYPE_GEOM_POLYLINE:
                if (type & Nivalis.FUNC_TYPE_MOD_CLOSED)
                    return '<span title="Polygon"><svg style="width:22px;height:22px" viewBox="0 0 24 24"><path fill="#888" d="M2,2V8H4.28L5.57,16H4V22H10V20.06L15,20.05V22H21V16H19.17L20,9H22V3H16V6.53L14.8,8H9.59L8,5.82V2M4,4H6V6H4M18,5H20V7H18M6.31,8H7.11L9,10.59V14H15V10.91L16.57,9H18L17.16,16H15V18.06H10V16H7.6M11,10H13V12H11M6,18H8V20H6M17,18H19V20H17" /></svg></span>';
                else
                    return '<span title="Point/polyline"><svg style="width:22px;height:22px" viewBox="0 0 24 24"><path fill="#888" d="M2 3V9H4.95L6.95 15H6V21H12V16.41L17.41 11H22V5H16V9.57L10.59 15H9.06L7.06 9H8V3M4 5H6V7H4M18 7H20V9H18M8 17H10V19H8Z" /></svg></span>';
            case Nivalis.FUNC_TYPE_GEOM_RECT:
                    return '<span title="Rectangle"><svg style="width:22px;height:22px" viewBox="0 0 24 24"><path fill="#888" d="M4,6V19H20V6H4M18,17H6V8H18V17Z"/></svg></span>';
            case Nivalis.FUNC_TYPE_GEOM_CIRCLE:
                    return '<span title="Circle"><svg style="width:22px;height:22px" viewBox="0 0 24 24"><path fill="#888" d="M12,20A8,8 0 0,1 4,12A8,8 0 0,1 12,4A8,8 0 0,1 20,12A8,8 0 0,1 12,20M12,2A10,10 0 0,0 2,12A10,10 0 0,0 12,22A10,10 0 0,0 22,12A10,10 0 0,0 12,2Z"/><svg></span>';
            case Nivalis.FUNC_TYPE_GEOM_ELLIPSE:
                    return '<span title="Ellipse"><svg style="width:22px;height:22px" viewBox="0 0 24 24"><path fill="#888" d="M12,6C16.41,6 20,8.69 20,12C20,15.31 16.41,18 12,18C7.59,18 4,15.31 4,12C4,8.69 7.59,6 12,6M12,4C6.5,4 2,7.58 2,12C2,16.42 6.5,20 12,20C17.5,20 22,16.42 22,12C22,7.58 17.5,4 12,4Z" /></svg></span>';
            case Nivalis.FUNC_TYPE_GEOM_TEXT:
                    return '<span title="Text"><svg style="width:22px;height:22px" viewBox="0 0 24 24"><path fill="#888" d="M18.5,4L19.66,8.35L18.7,8.61C18.25,7.74 17.79,6.87 17.26,6.43C16.73,6 16.11,6 15.5,6H13V16.5C13,17 13,17.5 13.33,17.75C13.67,18 14.33,18 15,18V19H9V18C9.67,18 10.33,18 10.67,17.75C11,17.5 11,17 11,16.5V6H8.5C7.89,6 7.27,6 6.74,6.43C6.21,6.87 5.75,7.74 5.3,8.61L4.34,8.35L5.5,4H18.5Z" /></svg></span>';
        }
        return "?";
    },
    _update_comment_class(name) {
        if (Nivalis.get_func_type(FuncEdit.func_indices[name]) == Nivalis.FUNC_TYPE_COMMENT ||
            Nivalis.get_func_type(FuncEdit.func_indices[name]) == Nivalis.FUNC_TYPE_FUNC_DEFINITION) {
            $('#function-widget-' +  name).addClass('comment');
        } else {
            $('#function-widget-' +  name).removeClass('comment');
        }
    },
    new_func: function(add) {
        let fidx = FuncEdit.func_names.length;
        if (add) {
            success = Nivalis.add_func();
            if (!success) return;
        }
        var fname = Nivalis.get_func_name(fidx);
        FuncEdit.func_names.push(fname);
        FuncEdit.func_indices[fname] = fidx;
        $('#functions').append($('#function-form-template').html().replace(
                new RegExp('{name}', 'g'), fname).replace(
                new RegExp('{rendered_name}', 'g'), FuncEdit._render_func_type(fname)));

        let editor_tb = $('#function-expr-' + fname);
        let MQ = MathQuill.getInterface(2);
        const mf = MQ.MathField(editor_tb[0],
            {
                handlers: {
                    edit: function(mathField) {
                        var this_name = mathField.el().id.substr(14);
                        FuncEdit.reparse(this_name);
                        let tex = mathField.latex();
                        let ctrl = mathField.__controller;
                        if (tex === "#") {
                            const placeholder = "comment";
                            mathField.latex("\\text{" + placeholder + "}");
                            ctrl.moveLeft();
                            for (var i = 0; i < placeholder.length; ++i) {
                                ctrl.selectLeft();
                            }
                        } else if (tex === "ssl") {
                            const placeholder = "comment";
                            mathField.write("\ \\text{" + placeholder + "}");
                            ctrl.moveLeft();
                            for (var i = 0; i < placeholder.length; ++i) {
                                ctrl.selectLeft();
                            }
                        } else {
                            const opn_str = "\\operatorname{";
                            if (tex.startsWith(opn_str) && tex[tex.length - 1] === '}') {
                                let tex_opn = tex.substr(opn_str.length, tex.length - opn_str.length - 1);
                                if (tex_opn.length <= 1) return;
                                if (tex_opn[0] === 'f' || tex_opn[0] === 'F')
                                    tex_opn = tex_opn.substr(1);
                                if (tex_opn === "text") {
                                    const placeholder = "edit me";
                                    mathField.write("\\ \\text{" + placeholder + "}\\ \\operatorname{at}\ \\ \\left(0, 0\\right)");
                                    for (var i = 0; i < 10; ++i) ctrl.moveLeft();
                                    for (var i = 0; i < placeholder.length; ++i) ctrl.selectLeft();
                                } else if (tex_opn === "poly") {
                                    mathField.write("\\ \\left(0,0\\right),\\ \\left(0,1\\right),\\ \\left(1,1\\right)");
                                    for (let i = 0; i < 7; ++i) ctrl.selectLeft();
                                } else if (tex_opn === "rect") {
                                    mathField.write("\\ \\left(0,0\\right),\\ \\left(1,1\\right)");
                                    for (let i = 0; i < 4; ++i) ctrl.selectLeft();
                                } else if (tex_opn === "circ") {
                                    mathField.write("\\ 1\\ \\operatorname{at}\ \\ \\left(0, 0\\right)");
                                    ctrl.moveLeft();
                                    for (let i = 0; i < 3; ++i) ctrl.selectLeft();
                                } else if (tex_opn === "ellipse") {
                                    mathField.write("\\ \\left(1,1\\right)\\\ \\operatorname{at}\ \\ \\left(0, 0\\right)");
                                    ctrl.moveLeft();
                                    for (let i = 0; i < 3; ++i) ctrl.selectLeft();
                                }
                            }
                        }
                        $('#function-name-' + this_name).html(FuncEdit._render_func_type(this_name));
                        FuncEdit._update_comment_class(this_name);
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
                        if (idx + 1 === Nivalis.num_funcs()) {
                            FuncEdit.new_func(true);
                        }
                        var next_tb = $('#function-expr-' + FuncEdit.func_names[idx + 1]);
                        next_tb.mousedown(); next_tb.mouseup();
                    },
                }
            });
        FuncEdit.func_mfields.push(mf);
        mf.latex(Nivalis.get_func_expr(fidx));
        var tb_mousedown = function() {
            // Focus (change curr func)
            let this_name;
            if ($(this).hasClass('function-expr')) {
                this_name = this.id.substr(14);
            } else {
                this_name = this.id.substr(22);
            }
            var idx = FuncEdit.func_indices[this_name];
            if (idx === Nivalis.num_funcs() - 1) {
                // Add function
                FuncEdit.new_func(true);
            }
            $('#function-widget-' + this_name).removeClass('inactive');
            if (idx != Nivalis.get_curr_func()) {
                Nivalis.set_curr_func(idx);
                Nivalis.redraw();
                Nivalis.set_curr_func(idx);
            }
            FuncEdit.func_mfields[idx].focus();
        };
        $('#function-expr-wrapper-' + fname).click(tb_mousedown);
        editor_tb.mousedown(tb_mousedown);
        editor_tb.keyup(function() {
            var this_name = this.id.substr(14);
            FuncEdit.reparse(this_name);
            $('#function-name-' + this_name).html(FuncEdit._render_func_type(this_name));
            FuncEdit._update_comment_class(this_name);
        });
        $('#function-tmin-' + fname).val(Math.floor(Nivalis.get_func_tmin(fidx) * 1e6)/1e6);
        $('#function-tmax-' + fname).val(Math.ceil(Nivalis.get_func_tmax(fidx) * 1e6)/1e6);
        $('#function-tmin-' + fname).on('input', function() {
            Nivalis.set_func_tmin(fidx, Number.parseFloat(this.value));
            Nivalis.redraw();
        });
        $('#function-tmax-' + fname).on('input', function() {
            Nivalis.set_func_tmax(fidx, Number.parseFloat(this.value));
            Nivalis.redraw();
        });

        {
            let function_del = $('#function-del-' + fname);
            function_del.html(Util.icon_delete);
            function_del.click(function() {
                // Delete
                var this_name = this.id.substr(13);
                var idx = FuncEdit.func_indices[this_name];
                if (FuncEdit.func_names.length > idx + 1) {
                    Nivalis.delete_func(idx);
                    Nivalis.redraw();
                    FuncEdit.func_names.splice(idx, 1);
                    delete FuncEdit.func_indices[this_name];
                    FuncEdit.func_mfields.splice(idx, 1);
                    for (var t = 0; t < FuncEdit.func_names.length; t++) {
                        if (FuncEdit.func_indices[FuncEdit.func_names[t]] >= idx)
                            FuncEdit.func_indices[FuncEdit.func_names[t]] -= 1;
                    }
                    $('#function-widget-' + this_name).css('min-height', 0);
                    $('#function-widget-' + this_name).css('max-height', 0);
                    setTimeout(function() {
                        $('#function-widget-' + this_name).remove();
                    }, 500);
                }
            });
        }
        // Color picker
        const pickr = Pickr.create(Object.assign(
            {el: '#color-picker-' +  fname}, pickrConfig
        ));
        pickr.on("init", function(instance) {
            var this_name = instance.options.el.id.substr(13);
            var idx = FuncEdit.func_indices[this_name];
            pickr.setColor("#" + Nivalis.get_func_color(idx));
        });
        pickr.on("change", function(color, instance) {
            var hex = color.toHEXA().toString().substr(1);
            var this_name = instance.options.el.id.substr(13);
            var idx = FuncEdit.func_indices[this_name];
            Nivalis.set_func_color(idx, hex);
            Nivalis.redraw();
        });
        FuncEdit._update_comment_class(fname);
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
        if (add) Nivalis.add_slider();
        Sliders.slider_ids.push(sid);
        Sliders.slider_indices[sid] = sidx;
        $('#sliders').append($('#slider-form-template').html().replace(
                new RegExp('{name}', 'g'), sid));
        {
            var slider_var = $('#slider-var-' + sid);
            slider_var.val(Nivalis.get_slider_var(sidx));
            slider_var.on('input', function() {
                var $this = $(this);
                var this_sid = this.id.substr(11);
                var idx = Sliders.slider_indices[this_sid];
                Nivalis.set_slider_var(idx, $this.val());
                Nivalis.redraw();
            });
        }
        var sliderChangeVal = function() {
            var $this = $(this);
            var this_sid = this.id.substr(11);
            var idx = Sliders.slider_indices[this_sid];
            var new_val = Number.parseFloat($this.val());
            Nivalis.set_slider_val(idx, new_val);
            Renderer.redraw();
            if ($this.hasClass('slider-sli')) {
                $('#slider-val-' + this_sid).val(new_val);
            } else {
                $('#slider-sli-' + this_sid).val(new_val);
            }
        };
        var init_min = Nivalis.get_slider_lo(sidx);
        var init_max = Nivalis.get_slider_hi(sidx);
        {
            var sliderChangeBounds = function(e) {
                var this_sid = this.id.substr(11);
                var idx = Sliders.slider_indices[this_sid];
                var new_min = Number.parseFloat($('#slider-min-' + this_sid).val());
                var new_max = Number.parseFloat($('#slider-max-' + this_sid).val());
                Nivalis.set_slider_lo_hi(idx, new_min, new_max);
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
            slider.val(Nivalis.get_slider_val(sidx))
            slider.on('input', sliderChangeVal);
        }
        {
            var slider_val = $('#slider-val-' + sid);
            slider_val.val(Nivalis.get_slider_val(sidx))
            slider_val.on('input', sliderChangeVal);
        }
        {
            var slider_del = $('#slider-del-' + sid);
            slider_del.html(Util.icon_delete);
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
                Nivalis.delete_slider(idx);
                Nivalis.redraw();
            });
        }
        {
            var slider_ani = $('#slider-ani-' + sid);
            slider_ani.html(Util.icon_play);
            slider_ani.click(function() {
                var this_sid = this.id.substr(11);
                var idx = Sliders.slider_indices[this_sid];
                if (Nivalis.slider_animation_dir(idx) != 0) {
                    Nivalis.end_slider_animation(idx);
                    $(this).html(Util.icon_play);
                } else {
                    Nivalis.begin_slider_animation(idx);
                    $(this).html(Util.icon_stop);
                }
                Renderer.redraw();
            });
        }
        Nivalis.redraw();
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
            vbound.val(Nivalis['get_' + b]());
        }
    },
    updateViewOptions: function() {
        $('#view-axes').prop('checked', Nivalis.get_axes_enabled());
        $('#view-grid').prop('checked', Nivalis.get_grid_enabled());
        $('#view-polar').prop('checked', Nivalis.get_is_polar_grid());
    },
    resetView: function() {
        Nivalis.reset_view();
        ViewConfig.updateViewBounds();
        Nivalis.redraw();
    },
    init : function() {
        ViewConfig.updateViewBounds();
        for (var i = 0; i < ViewConfig.vbounds.length; i++) {
            var b = ViewConfig.vbounds[i];
            var vbound = $('#view-' + b);
            vbound.on('input', function() {
                Nivalis.set_view(
                    Number.parseFloat($('#view-xmin').val()),
                    Number.parseFloat($('#view-xmax').val()),
                    Number.parseFloat($('#view-ymin').val()),
                    Number.parseFloat($('#view-ymax').val()));
                Nivalis.redraw();
            });
        }

        $('.reset-view').click(ViewConfig.resetView);

        ViewConfig.updateViewOptions();
        $('#view-axes').change(function(){
            Nivalis.set_axes_enabled(
                $('#view-axes').prop('checked')
            );
            Nivalis.redraw();
        });
        $('#view-grid').change(function(){
            Nivalis.set_grid_enabled(
                $('#view-grid').prop('checked')
            );
            Nivalis.redraw();
        });
        $('#view-polar').change(function(){
            Nivalis.set_is_polar_grid(
                $('#view-polar').prop('checked')
            );
            Nivalis.redraw();
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
                let ret = Nivalis.shell_exec(command);
                if (ret) {
                    Shell.con.log(Nivalis.get_shell_output());
                } else {
                    Shell.con.error(Nivalis.get_shell_output());
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
            let init_output = Nivalis.get_shell_output();
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
            $('#open-shell').children('.custom-caret').html(Util.icon_caret_down)
            $('#open-shell').removeClass('active');
        } else {
            shell.height(window.innerHeight - 130);
            shell.show();
            $('#open-shell').addClass('active');
            Shell.start();
            $('#open-shell').children('.custom-caret').html(Util.icon_caret_up)
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
                Nivalis.import_json(localStorage['save' + this_save]);
                resync();
                Nivalis.redraw();
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
            localStorage.setItem('save' + nsaves, Nivalis.export_json());
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
    for (let i = 0; i < Nivalis.num_funcs(); i++ ){
        FuncEdit.new_func(false);
        FuncEdit.reparse(FuncEdit.func_names[i]);
    }
    for (let i = 0; i < Nivalis.num_funcs() - 1; i++){
        let fn = FuncEdit.func_names[i];
        $('#function-widget-' + fn).removeClass('inactive');
    }
    for (let i = 0; i < Nivalis.num_sliders(); i++){
        Sliders.new_slider(false);
    }
    FuncEdit.func_mfields[Nivalis.get_curr_func()].focus();
    ViewConfig.updateViewBounds();
    ViewConfig.updateViewOptions();
};
