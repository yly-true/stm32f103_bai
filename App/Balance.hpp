#ifndef APP_BALANCE_HPP
#define APP_BALANCE_HPP

#include "Board.hpp"
#include "Pid.hpp"
#include "State.hpp"

namespace app {

// 手动平衡：角度 PD（每 5 ms）+ 位置 PD（每 25 ms），相减后限幅输出。
class Balance {
public:
    void reset();
    void tick(State& s, Board& io);

    AnglePd&    anglePd()    { return anglePd_; }
    PositionPd& positionPd() { return positionPd_; }

private:
    void protect(State& s, const Board& io) const;
    static int limit(int pwm);

    AnglePd    anglePd_;
    PositionPd positionPd_;
};

} // namespace app

#endif
