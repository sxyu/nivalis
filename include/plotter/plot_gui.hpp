#pragma once
#ifndef _PLOTGUI_H_83CE414C_2BBF_49BB_BBE7_83746DD75A5E
#define _PLOTGUI_H_83CE414C_2BBF_49BB_BBE7_83746DD75A5E
#include<memory>
#include "env.hpp"
namespace nivalis {
// Plotter GUI, made with Nana
class PlotGUI {
public:
    explicit PlotGUI(Environment& env, const std::string& init_expr = "");
};
}  // namespace nivalis
#endif // ifndef _PLOTGUI_H_83CE414C_2BBF_49BB_BBE7_83746DD75A5E