#ifndef __RETRANSMISSION_TIMER__
#define __RETRANSMISSION_TIMER__

#include <cstddef>  // size_t声明头文件

class RetransmissionTimer {
  private:
    bool _is_running{false};  // 定时器状态
    size_t _elapsed_time{0};  // 定时器走过的时间
    size_t _rto;              // 当前rto
    size_t _init_rto;         // 初始rto

  public:
    // 构造函数
    RetransmissionTimer(const size_t retx_timeout);

    // 开启定时器
    void start_timer();

    // 关闭定时器
    void stop_timer();

    // 定时器time_out事件处理函数
    void handle_timeout();

    // 检查定时器是否timeout
    bool check_timeout(const size_t ms_since_last_tick);

    // 重置定时器
    void reset_timer();
};

#endif /* __RETRANSMISSION_TIMER__ */
