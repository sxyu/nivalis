#include "plotgui.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <chrono>
#include <queue>
#include <utility>
#include <nana/gui.hpp>
#include <nana/gui/widgets/form.hpp>
#include <nana/gui/widgets/textbox.hpp>
#include <nana/gui/widgets/widget.hpp>
#include <nana/gui/drawing.hpp>
#include <nana/gui/widgets/label.hpp>
#include <nana/gui/widgets/button.hpp>
#include <nana/gui/dragger.hpp>
#include <nana/paint/graphics.hpp>
// #include <nana/threads/pool.hpp>

#include "expr.hpp"
#include "parser.hpp"
#include "util.hpp"

namespace nivalis {
using namespace nana;

namespace {
const color& color_from_int(size_t color_index)
{
    static const color palette[] = {
        colors::red, colors::royal_blue, colors::green, colors::orange,
        colors::purple, colors::black,
        color(255, 220, 0), color(201, 13, 177), color(34, 255, 94),
        color(255, 65, 54), color(255, 255, 64), color(0, 116, 217),
        color(27, 133, 255), color(190, 18, 240), color(20, 31, 210),
        color(75, 20, 133), color(255, 219, 127), color(204, 204, 57),
        color(112, 153, 61), color(64, 204, 46), color(112, 255, 1),
        color(170, 170, 170), color(225, 30, 42)
    };
    return palette[color_index % (sizeof palette / sizeof palette[0])];
}
}  // namespace

struct PlotGUI::impl {
    std::chrono::high_resolution_clock::time_point lazy_start;
    void update(bool force = false) {
        auto finish = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(finish - lazy_start).count();
        if (force || elapsed > 1000) {
            dw.update();
            lazy_start = std::chrono::high_resolution_clock::now();
        }
    }
    impl(Environment expr_env, std::string init_expr)
        : fm(API::make_center(1000, 600)), dw(fm),
             env(expr_env) {
        fm.caption("Nivalis");
        // Func label
        label label_func(fm, rectangle{20, 20, 230, 30});
        label_func.caption("Function 0");
        label_func.transparent(true);
        // Textbook for editing functions
        textbox tb(fm, rectangle{20, 40, 230, 40});
        tb.caption(init_expr);

        int edit_expr_idx = 0;
        auto upd_expr = [&](){ 
            size_t idx = edit_expr_idx;
            auto& expr = exprs[idx];
            auto& expr_str = expr_strs[idx];
            expr_str = tb.caption();
            util::trim(expr_str);
            expr = parser(expr_str, env);
            expr.optimize();
            update();
        };
        tb.events().key_char([&](const nana::arg_keyboard& arg) {
            switch(arg.key) {
            case keyboard::enter:
                arg.ignore = true;
                break;
            case keyboard::escape:
                arg.ignore = true;
                fm.focus();
                break;
            }
        });
        auto seek_func = [&](int delta){
            edit_expr_idx += delta;
            std::string suffix;
            if (edit_expr_idx < 0) {
                edit_expr_idx = 0;
                return;
            }
            else if (edit_expr_idx >= exprs.size()) {
                std::string tmp = expr_strs.back();
                util::trim(tmp);
                if (!tmp.empty()) {
                    exprs.emplace_back();
                    expr_strs.emplace_back();
                    if (reuse_colors.empty()) {
                        expr_colors.push_back(color_from_int(last_expr_color++));
                    } else {
                        expr_colors.push_back(reuse_colors.front());
                        reuse_colors.pop();
                    }
                    suffix = " [new]";
                } else {
                    edit_expr_idx = exprs.size()-1;
                    return;
                }
            }
            label_func.caption("Function " +
                    std::to_string(edit_expr_idx) + suffix);
            tb.caption(expr_strs[edit_expr_idx]);
            update(true);
        };
        auto delete_func = [&]() {
            if (exprs.size() > 1) {
                exprs.erase(exprs.begin() + edit_expr_idx);
                expr_strs.erase(expr_strs.begin() + edit_expr_idx);
                reuse_colors.push(expr_colors[edit_expr_idx]);
                expr_colors.erase(expr_colors.begin() + edit_expr_idx);
                if (edit_expr_idx >= exprs.size()) {
                    edit_expr_idx--;
                }
            } else {
                expr_strs[0] = "";
            }
            upd_expr();
            seek_func(0);
        };
        tb.events().key_release([&](const nana::arg_keyboard& arg) {
            upd_expr();
            if (arg.key == 38 || arg.key == 40) {
                // Up/down: go to different funcs
                seek_func(arg.key == 38 ? -1 : 1);
            } else if (arg.key == 127) {
                // Delete
                delete_func();
            }
        });

        // Home button
        button btn_home(fm, rectangle{20, 80, 110, 30});
        btn_home.transparent(true);
        btn_home.caption(L"Reset View");
        btn_home.events().click([&](){ 
            xmax = 10.0; xmin = -10.0;
            ymax = 6.0; ymin = -6.0;
            update();
            fm.focus();
        });

        // Prev/next/del-func button
        button btn_prev(fm, rectangle{130, 80, 40, 30});
        btn_prev.transparent(true);
        btn_prev.caption(L"<");
        btn_prev.events().click([&](){ seek_func(-1); fm.focus(); });
        button btn_next(fm, rectangle{170, 80, 40, 30});
        btn_next.transparent(true);
        btn_next.caption(L">");
        btn_next.events().click([&](){ seek_func(1); fm.focus(); });
        button btn_del(fm, rectangle{210, 80, 40, 30});
        btn_del.transparent(true);
        btn_del.caption(L"x");
        btn_del.events().click([&](){ delete_func(); fm.focus(); });

        // Form keyboard handle
        auto keyn_handle = fm.events().key_release([&](const nana::arg_keyboard&arg)
        {
            switch(arg.key) {
                case 81:
                    // q: quit
                    fm.close();
                    break;
                case 37: case 38: case 39: case 40:
                    // Arrows
                    if (arg.key & 1) {
                        // LR
                        auto delta = (xmax - xmin) * 0.02;
                        if (arg.key == 37) delta = -delta;
                        xmin += delta; xmax += delta;
                    } else {
                        // UD
                        auto delta = (ymax - ymin) * 0.02;
                        if (arg.key == 40) delta = -delta;
                        ymin += delta; ymax += delta;
                    }
                    update();
                    break;
                case 61: case 45:
                    // Zooming +-
                    {
                        auto fa = (arg.key == 45) ? 1.05 : 0.95;
                        auto dy = (ymax - ymin) * (fa - 1.) /2;
                        auto dx = (xmax - xmin) * (fa - 1.) /2;
                        if (arg.ctrl) dy = 0.; // x-only
                        if (arg.alt) dx = 0.;  // y-only
                        xmin -= dx; xmax += dx;
                        ymin -= dy; ymax += dy;
                        update();
                    }
                    break;
                case 72:
                    // ctrl H: Home
                    if (arg.ctrl) {
                        if (!arg.alt) xmax = 10.0; xmin = -10.0;
                        ymax = 6.0; ymin = -6.0;
                        update();
                    }
                    break;
                case 69:
                    // E: Edit (focus tb)
                    tb.focus();
                    break;
            }
        });

        // Dragging + scrolling
        point dragpt, lastpt;
        bool dragdown = false;
        double xmaxi, xmini, ymaxi, ymini;
        auto down_handle = fm.events().mouse_down([&](const nana::arg_mouse&arg)
        {
            if (!dragdown) {
                lastpt = dragpt = arg.pos;
                dragdown = true;
                xmaxi = xmax; xmini = xmin;
                ymaxi = ymax; ymini = ymin;
            }
        });

        auto move_handle =
            fm.events().mouse_move(
                    [&](const nana::arg_mouse&arg) {
                if (dragdown) {
                        lastpt = arg.pos;
                        double swid = fm.size().width;
                        double shigh = fm.size().height;
                        int dx = arg.pos.x - dragpt.x;
                        int dy = arg.pos.y - dragpt.y;
                        double fx = (xmax - xmin) / swid * dx;
                        double fy = (ymax - ymin) / shigh * dy;
                        xmax = xmaxi - fx; xmin = xmini - fx;
                        ymax = ymaxi + fy; ymin = ymini + fy;
                        update();
                }
            });

        auto up_handle = fm.events().mouse_up([&dragdown](const nana::arg_mouse&arg) { dragdown = false; });
        auto leave_handle = fm.events().mouse_leave([&dragdown](const nana::arg_mouse&arg) { dragdown = false; });
        int win_wid = 1000, win_hi = 600;
        auto resize_handle = fm.events().resized([this, &win_wid, &win_hi](const nana::arg_resized&arg) {
            double wf = (xmax - xmin) * (1.*arg.width / win_wid - 1.) / 2;
            double hf = (ymax - ymin) * (1.*arg.height / win_hi - 1.) / 2;
            xmax += wf; xmin -= wf;
            ymax += hf; ymin -= hf;

            win_wid = arg.width;
            win_hi = arg.height;
            update();
        });


        auto scroll_handle = fm.events().mouse_wheel([&](
                    const nana::arg_wheel&arg)
        {
            dragdown = false;
            constexpr double multiplier = 0.01;
            double scaling;
            if (arg.upwards) {
                scaling = exp(-log(arg.distance) * multiplier);
            } else {
                scaling = exp(log(arg.distance) * multiplier);
            }
            double xdiff = (xmax - xmin) * (scaling-1.);
            double ydiff = (ymax - ymin) * (scaling-1.);

            double focx = arg.pos.x * 1. / fm.size().width;
            double focy = arg.pos.y * 1./ fm.size().height;
            xmax += xdiff * (1-focx);
            xmin -= xdiff * focx;
            ymax += ydiff * focy;
            ymin -= ydiff * (1-focy);
            update();
        });
        dw.draw([this](paint::graphics& graph){ graph.rectangle(true, colors::white); });

        x_var = env.addr_of("x", false);
        env.vars[x_var] = 3.0;

        exprs.push_back(parser(init_expr, env));
        expr_strs.push_back(init_expr);
        expr_colors.push_back(color_from_int(last_expr_color++));
        exprs[0].optimize();
        dw.draw([this, &edit_expr_idx](paint::graphics& graph) {
            int swid = fm.size().width, shigh = fm.size().height;
            double xdiff = xmax - xmin, ydiff = ymax - ymin;

            int sx0 = 0, sy0 = 0;
            int cnt_visible_axis = 0;
            // Draw axes
            if (ymin <= 0 && ymax >= 0) {
                double y0 = ymax / (ymax - ymin);
                sy0 = shigh * y0;
                graph.line(point(0, sy0), point(swid, sy0), colors::dark_gray);
                graph.line(point(0, sy0+1), point(swid, sy0+1), colors::dark_gray);
                ++cnt_visible_axis;
            }
            else if (ymin > 0) {
                sy0 = shigh - 26;
            }
            if (xmin <= 0 && xmax >= 0) {
                double x0 = - xmin / (xmax - xmin);
                sx0 = swid * x0;
                graph.line(point(sx0,0), point(sx0, shigh), colors::dark_gray);
                graph.line(point(sx0+1,0), point(sx0+1, swid), colors::dark_gray);
                ++cnt_visible_axis;
            }
            else if (xmax < 0) {
                sx0 = swid - 50;
            }

            // Draw lines
            auto round125 = [](double step) {
                int fa = 1, fan;
                if (step < 1) {
                    int subdiv = 5;
                    while(1./fa > step) {
                        fan = fa; fa *= 2; subdiv = 5;
                        if(1./fa <= step) break;
                        fan = fa; fa /= 2; fa *= 5; subdiv = 4;
                        if(1./fa <= step) break;
                        fan = fa; fa *= 2; subdiv = 5;
                    }
                    return std::pair<double, double>(1./fan/subdiv, 1./fan);
                } else {
                    int subdiv = 5;
                    while(fa < step) {
                        fa *= 2; subdiv = 4;
                        if(fa >= step) break;
                        fa /= 2; fa *= 5; subdiv = 5;
                        if(fa >= step) break;
                        fa *= 2; subdiv = 5;
                    }
                    return std::pair<double, double>(fa * 1. / subdiv, fa);
                }
            };
            double ystep, xstep, ymstep, xmstep;
            std::tie(ystep, ymstep) = round125((ymax - ymin) / shigh * 600 / 10.8);
            std::tie(xstep, xmstep) = round125((xmax - xmin) / swid * 1000 / 18.);
            double xli = std::ceil(xmin / xstep) * xstep;
            double xr = std::floor(xmax / xstep) * xstep;
            double ybi = std::ceil(ymin / ystep) * ystep;
            double yt = std::floor(ymax / ystep) * ystep;
            double yb = ybi, xl = xli;
            int idx = 0;
            while (xl <= xr) {
                int sxi = swid * (xl - xmin) / (xmax - xmin);
                graph.line(point(sxi,0), point(sxi, shigh), colors::light_gray);
                xl = xstep * idx + xli;
                ++idx;
            }
            idx = 0;
            while (yb <= yt) {
                int syi = shigh * (ymax - yb) / (ymax - ymin);
                graph.line(point(0, syi), point(swid, syi), colors::light_gray);
                yb = ystep * idx + ybi;
                ++idx;
            }
            // Larger lines + text
            double xmli = std::ceil(xmin / xmstep) * xmstep;
            double xmr = std::floor(xmax / xmstep) * xmstep;
            double ymbi = std::ceil(ymin / ymstep) * ymstep;
            double ymt = std::ceil(ymax / ymstep) * ymstep;
            double ymb = ymbi, xml = xmli;
            idx = 0;
            while (xml <= xmr) {
                int sxi = swid * (xml - xmin) / (xmax - xmin);
                graph.line(point(sxi,0), point(sxi, shigh), colors::gray);

                if (xml != 0) {
                    std::stringstream sstm;
                    sstm << std::setprecision(4) << xml;
                    graph.string(point(sxi-7, sy0+5), sstm.str());
                }
                ++idx;
                xml = xmstep * idx + xmli;
            }
            idx = 0;
            while (ymb <= ymt) {
                int syi = shigh * (ymax - ymb) / (ymax - ymin);
                graph.line(point(0, syi), point(swid, syi), colors::gray);

                std::stringstream sstm;
                if (ymb != 0) {
                    sstm << std::setprecision(4) << ymb;
                    graph.string(point(sx0+5, syi-6), sstm.str());
                }
                ++idx;
                ymb = ymstep * idx + ymbi;
            }

            // Draw 0
            if (cnt_visible_axis == 2) {
                graph.string(point(sx0 - 12, sy0 + 5), "0");
            }

            // Draw functions
            for (size_t exprid = 0; exprid < exprs.size(); ++exprid) {
                bool reinit = true;
                int psx = -1, psy = -1;
                auto& expr = exprs[exprid];
                const color& func_color = expr_colors[exprid];
                for (double sxd = 0; sxd < swid; sxd += 0.25) {
                    const double x = sxd / swid * xdiff + xmin;
                    env.vars[x_var] = x;
                    double y = expr(env);
                    if (!std::isnan(y)) {
                        reinit = true;
                        if (std::isinf(y)) {
                            if (y>0) y = ymax+(ymax-ymin)*1000;
                            else y = ymin-(ymax-ymin)*1000;
                        }
                        int sy = (ymax - y) / ydiff * shigh;
                        int sx = static_cast<int>(sxd);
                        if (reinit) {
                            if (!(y > ymax || y < ymin)) {
                                reinit = false;
                                if (~psx) {
                                    graph.line(point(psx, psy), point(sx,sy), func_color);
                                    if (edit_expr_idx == exprid) {
                                        // Thicken current function
                                        graph.line(point(psx+1, psy), point(sx+1,sy), func_color);
                                        graph.line(point(psx, psy+1), point(sx,sy+1), func_color);
                                    }
                                }
                            }
                        } else {
                            graph.line(point(psx, psy), point(sx,sy), func_color);
                            if (edit_expr_idx == exprid) {
                                graph.line(point(psx+1, psy), point(sx+1,sy), func_color);
                                graph.line(point(psx, psy+1), point(sx,sy+1), func_color);
                            }
                            if (y > ymax || y < ymin) {
                                reinit = true;
                            }
                        }
                        psx = sx;
                        psy = sy;
                    }
                }
            }
        });
        update(true);
    
        fm.show();
        exec();
    }

    double xmax = 10, xmin = -10;
    double ymax = 6, ymin = -6;

private:
    form fm;
    drawing dw;

    Environment env;
    Parser parser;
    std::vector<Expr> exprs;
    std::vector<std::string> expr_strs;
    std::queue<color> reuse_colors;
    size_t last_expr_color = 0;
    std::vector<color> expr_colors;
    uint32_t x_var;

    // nana::threads::pool pool;
};

PlotGUI::PlotGUI(Environment& env, const std::string& init_expr)
    : pImpl(std::make_unique<impl>(env, init_expr)) {
}

PlotGUI::~PlotGUI() =default;

}  // namespace nivalis
