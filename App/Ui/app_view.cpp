/**
 * @file    app_view.cpp
 * @brief   C 显示层与控制类之间的桥
 *
 * 只有这一处知道 "C 函数 → Control 单例" 的映射关系；
 * 新增给界面用的数据时，在这里加一个 getter，并在 app_view.h 里声明。
 */
#include "app_view.h"

#include "Control.hpp"
#include "State.hpp"

namespace {

inline const app::State& view()
{
    return app::Control::instance().state();
}

} // namespace

extern "C" {

float App_GetAngle(void)        { return view().angle; }
int   App_GetEncoder(void)      { return view().encoder; }
int   App_GetPositionZero(void) { return view().positionZero; }
int   App_GetVoltageMv(void)    { return view().voltage; }
int   App_GetMenu(void)         { return view().menu; }
int   App_GetAmplitude(int idx) {
    if (idx < 0 || idx > 3) return 0;
    return view().amplitude[idx];
}
float App_GetBalanceKp(void)  { return view().balanceKp; }
float App_GetBalanceKd(void)  { return view().balanceKd; }
float App_GetPositionKp(void) { return view().positionKp; }
float App_GetPositionKd(void) { return view().positionKd; }

} // extern "C"
