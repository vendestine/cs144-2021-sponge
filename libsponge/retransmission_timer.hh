#ifndef __RETRANSMISSION_TIMER__
#define __RETRANSMISSION_TIMER__

#include <cstddef>

class RetransmissionTimer {
  private:
    bool _is_running{false};     // 定时器状态
    size_t _accumulate_time{0};  // 累计时间
    size_t _rto;                 // 当前rto
    size_t _init_rto;            // 初始rto

  public:
    // 构造函数
    RetransmissionTimer(const size_t retx_timeout);

    // 开启定时器
    void start_timer();

    // 关闭定时器
    void stop_timer();

    // 定时器time_out事件处理函数
    void handle_timeout();

    // 每次tick后，计算累计时间，检查是否timeout
    bool check_timeout(const size_t ms_since_last_tick);

    // 设置定时器
    void set_timeout(const size_t rto_time);
    void set_accumulate_time(const size_t accumulate_time);
};

#endif /* __RETRANSMISSION_TIMER__ */
