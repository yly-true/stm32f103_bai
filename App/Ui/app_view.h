#ifndef APP_VIEW_H
#define APP_VIEW_H

/* 显示/上报层（C 代码）读取控制状态的唯一入口。
 * 这些函数由 App/Ui/app_view.cpp 实现，内部转调 Control::instance().state()。
 * 全部保持 C 链接，避免 C 代码直接看到 C++ 类。 */
#ifdef __cplusplus
extern "C" {
#endif

float App_GetAngle(void);
int   App_GetEncoder(void);
int   App_GetPositionZero(void);
int   App_GetVoltageMv(void);
int   App_GetMenu(void);
int   App_GetAmplitude(int idx);
float App_GetBalanceKp(void);
float App_GetBalanceKd(void);
float App_GetPositionKp(void);
float App_GetPositionKd(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_VIEW_H */
