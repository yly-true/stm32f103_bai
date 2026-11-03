#include "Calibration.hpp"

#include "Config.hpp"
#include "State.hpp"
#include "main.h"

namespace app {

// 进入向导：关电机、关 TIM1 更新中断，防止控制环继续跑
void Calibration::enter(State& s, Board& io)
{
    s.calibrationRequested = false;
    s.restoreAfterCalibration = true;
    s.stopped = true;
    io.motor.stop();
    TIM1->DIER &= 0xFEu;
}

// 退出向导：恢复中断，清屏
void Calibration::leave(State& s)
{
    s.restoreAfterCalibration = false;
    TIM1->DIER |= 0x01u;

    OLED_Clear();
}

void Calibration::tick(State& s, Keys& keys, Board& io)
{
    // 从向导返回后，第一轮只做恢复
    if (s.restoreAfterCalibration) {
        leave(s);
        return;
    }

    oled_show();

    if (keys.longPressMenu()) {
        s.calibrationRequested = true;
    }
    if (!s.calibrationRequested) {
        return;
    }

    enter(s, io);
    OLED_Clear();

    Step step = Step::Tips;
    int  key = 0;

    for (;;) {
        switch (step) {
        case Step::Tips:
            key = keys.click();
            show_Tips();
            HAL_Delay(30);
            if (key == 5) {
                OLED_Clear();
                step = Step::Page0;
            } else if (key == 12) {
                leave(s);
                return;
            }
            break;

        case Step::Page0:
            HAL_Delay(30);
            key = keys.click();
            step_0();
            if (key == 11) {
                OLED_Clear();
                step = Step::Page1;
            } else if (key == 12) {
                OLED_Clear();
                step = Step::Tips;
            }
            break;

        case Step::Page1:
            step_1();
            HAL_Delay(30);
            key = keys.click();
            if (key == 11) {
                OLED_Clear();
                step = Step::Page2;
            } else if (key == 12) {
                OLED_Clear();
                step = Step::Page0;
            }
            break;

        case Step::Page2:
            step_2();
            HAL_Delay(30);
            key = keys.click();
            if (key == 11) {
                OLED_Clear();
                step = Step::Page3;
            } else if (key == 12) {
                OLED_Clear();
                step = Step::Page1;
            }
            break;

        case Step::Page3:
            s.angle = io.adc.raw(Adc::kAngleChannel, 15);
            step_3();
            HAL_Delay(50);
            key = keys.click();
            if (key == 12) {
                OLED_Clear();
                step = Step::Page2;
            } else if (key == 5) {
                OLED_Clear();
                hitCount_ = 0;
                sampleCount_ = 0;
                countdown_ = 10;
                step = Step::Check;
            }
            break;

        case Step::Check:
            s.angle = io.adc.raw(Adc::kAngleChannel, 15);
            OLED_ShowNumber(88, 16, static_cast<uint32_t>(countdown_), 2, 16);
            check_display();
            ++sampleCount_;
            HAL_Delay(30);

            if (sampleCount_ <= 200) {
                if (s.angle >= 1010.0f && s.angle <= 1030.0f) {
                    if (++hitCount_ > 150) {
                        OLED_Clear();
                        step = Step::Success;
                        break;
                    }
                }
                if (sampleCount_ % 20 == 1) {
                    --countdown_;
                }
            } else {
                sampleCount_ = 0;
                OLED_Clear();
                step = Step::Fail;
            }
            break;

        case Step::Fail:
            fail_display();
            HAL_Delay(2000);
            OLED_Clear();
            step = Step::Page3;
            break;

        case Step::Success:
            sampleCount_ = 0;
            success_display();
            if (keys.click() == 5) {
                leave(s);
                return;
            }
            break;
        }
    }
}

} // namespace app
