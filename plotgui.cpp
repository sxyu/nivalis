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

namespace nivalis {
using namespace nana;

namespace { }  // namespace

struct PlotGUI::impl {
    std::chrono::high_resolution_clock::time_point lazy_start;
    bool lazy_init = true;
    void update() {
        auto finish = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(finish - lazy_start).count();
        if (lazy_init || elapsed > 1000) {
            lazy_init = false;
            dw.update();
            lazy_start = std::chrono::high_resolution_clock::now();
        }
    }
    impl(Environment expr_env, std::string init_expr)
        : fm(API::make_center(1000, 600)), dw(fm),
             env(expr_env), expr_str(init_expr) {
        fm.caption("Nivalis");
        // Change function
        button btn_upd_expr(fm, rectangle{20, 60, 120, 30});
        btn_upd_expr.caption(L"Update");
        btn_upd_expr.transparent(true);
        textbox tb(fm, rectangle{20, 20, 160, 40});
        tb.caption(expr_str);

        auto upd_expr = [&](){ 
            expr_str = tb.caption();
            expr = parser(expr_str, env);
            expr.optimize();
            update();
            fm.focus();
        };
        btn_upd_expr.events().click(upd_expr);
        tb.events().key_char([&](const nana::arg_keyboard& _arg) {
            if (keyboard::enter == _arg.key)
            {
                upd_expr();
                _arg.ignore = true;
            }
            else if (keyboard::escape == _arg.key)
            {
                fm.focus();
                _arg.ignore = true;
            }
        });

        // Home button
        button btn_home(fm, rectangle{140, 60, 40, 30});
        btn_home.transparent(true);
        btn_home.caption(L"\u2302");
        btn_home.events().click([&](){ 
            xmax = 10.0; xmin = -10.0;
            ymax = 6.0; ymin = -6.0;
            update();
            fm.focus();
        });

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

        expr = parser(expr_str, env);
        expr.optimize();
        dw.draw([this](paint::graphics& graph) {
            int swid = fm.size().width, shigh = fm.size().height;
            double xdiff = xmax - xmin, ydiff = ymax - ymin;
            bool reinit = true;
            int psx = -1, psy;

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

            // Draw function
            for (double sxd = 0; sxd < swid; sxd += 0.1) {
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
                                graph.line(point(psx, psy), point(sx,sy), colors::red);
                                graph.line(point(psx+1, psy), point(sx+1,sy), colors::red);
                                graph.line(point(psx, psy+1), point(sx,sy+1), colors::red);
                            }
                        }
                    } else {
                        graph.line(point(psx, psy), point(sx,sy), colors::red);
                        graph.line(point(psx+1, psy), point(sx+1,sy), colors::red);
                        graph.line(point(psx, psy+1), point(sx,sy+1), colors::red);
                        if (y > ymax || y < ymin) {
                            reinit = true;
                        }
                    }
                    psx = sx;
                    psy = sy;
                }
            }
        });
        update();
    
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
    std::string expr_str;
    Expr expr;
    uint32_t x_var;

    // nana::threads::pool pool;
};

PlotGUI::PlotGUI(Environment& env, const std::string& init_expr)
    : pImpl(std::make_unique<impl>(env, init_expr)) {
}

PlotGUI::~PlotGUI() =default;

}  // namespace nivalis
