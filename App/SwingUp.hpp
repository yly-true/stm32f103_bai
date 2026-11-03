#ifndef APP_SWINGUP_HPP
#define APP_SWINGUP_HPP

#include "Board.hpp"
#include "Pid.hpp"
#include "State.hpp"

namespace app {

// 自动起摆状态机：
//   第 0 步 Find_Zero —— 把滑块拉回轨道中间，等摆杆静止在"竖直向下"
//   第 1 步 Auto_run  —— 先来回甩杆积累能量，判定能站住后切平衡控制并爬升 PID
class SwingUp {
public:
    void reset();
    void tick(State& s, Board& io, AnglePd& anglePd, PositionPd& positionPd);

private:
    void findZero(State& s, Board& io, AnglePd& anglePd);
    void autoRun(State& s, Board& io, AnglePd& anglePd, PositionPd& positionPd);
    static int limit(int pwm, int upper, int lower);

    // 第 0 步
    bool  step0Done_ = false;
    bool  step1Active_ = false;
    float settleCount_ = 0.0f;
    int   targetPosition_ = cfg::kPositionMiddle;

    // 第 1 步（甩杆）
    IncrementalPi   incrementalPi_;
    PrePositionPd   prePosition_;
    bool  directionLeft_ = false;
    bool  directionRight_ = false;
    int   speed_ = 0;
    long  successCount_ = 0;

    // 第 1 步（起摆成功后的接管与参数爬升）
    bool  successFlag_ = false;
    long  waitCount_ = 0;
    int   helpCount_ = 0;
    long  pidAdjust_ = 0;
};

} // namespace app

#endif
