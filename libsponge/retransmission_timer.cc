#include "retransmission_timer.hh"

RetransmissionTimer::RetransmissionTimer(const size_t retx_timeout) : _rto(retx_timeout), _init_rto(retx_timeout) {}

void RetransmissionTimer::start_timer() {
    if (!_is_running) {
        _is_running = true;
        _rto = _init_rto;
        _accumulate_time = 0;
    }
}

void RetransmissionTimer::stop_timer() {
    if (_is_running) {
        _is_running = false;
    }
}

void RetransmissionTimer::handle_timeout() {
    // timeout后 rto翻倍，累计时间清空
    _rto *= 2;
    _accumulate_time = 0;
}

bool RetransmissionTimer::check_timeout(const size_t ms_since_last_tick) {
    // 定时器开启时
    if (_is_running) {
        // 计算累计时间，判断是否timeout
        _accumulate_time += ms_since_last_tick;
        return _accumulate_time >= _rto;
    }
    // 定时器没有开启，肯定没有timeout
    return false;
}

void RetransmissionTimer::set_timeout(const size_t rto_time) { _rto = rto_time; }
void RetransmissionTimer::set_accumulate_time(const size_t accumulate_time) {_accumulate_time = accumulate_time; }