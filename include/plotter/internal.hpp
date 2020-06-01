#pragma once

#ifndef _INTERNAL_H_6C668A32_9DF8_4A08_95CC_8A5423CB2407
#define _INTERNAL_H_6C668A32_9DF8_4A08_95CC_8A5423CB2407

// Plot x to screen x
#define _X_TO_SX(x) (static_cast<float>(((x) - view.xmin) * view.swid / (view.xmax - view.xmin)))
// Screen x to plot x
#define _SX_TO_X(sx) (1.*(sx) * (view.xmax - view.xmin) / view.swid + view.xmin)
// Plot y to screen y
#define _Y_TO_SY(y) (static_cast<float>((view.ymax - (y)) * view.shigh / (view.ymax - view.ymin)))
// Screen y to plot y
#define _SY_TO_Y(sy) ((view.shigh - (sy))*1. * (view.ymax - view.ymin) / view.shigh + view.ymin)

#endif // ifndef _INTERNAL_H_6C668A32_9DF8_4A08_95CC_8A5423CB2407
