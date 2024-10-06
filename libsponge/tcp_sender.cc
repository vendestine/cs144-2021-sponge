#include "tcp_sender.hh"

#include "tcp_config.hh"
#include "wrapping_integers.hh"
#include <iostream>
#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _retransmission_timer(retx_timeout) {}

uint64_t TCPSender::bytes_in_flight() const { return _next_seqno - _receiver_ack; }

void TCPSender::fill_window() {
    
    
    // 创建一个TCPSegment
    TCPSegment segment{};
    uint64_t window_size = _receiver_window_size == 0 ? 1 : _receiver_window_size;

    if (_set_fin_flag) return;
    // 发syn segment
    if (_next_seqno == 0) {
        segment.header().syn = true;
        segment.header().seqno = _isn;

        _set_syn_flag = true;
        ++_next_seqno;  // 发了一个syn，更新 next_seqno
    }
    // 发 payload + fin segment
    else {
        size_t payload_size =
            std::min({window_size - bytes_in_flight(), _stream.buffer_size(), TCPConfig::MAX_PAYLOAD_SIZE});
        segment.payload() = Buffer(_stream.read(payload_size));
        segment.header().seqno = next_seqno();
        _next_seqno += payload_size;  // 发了payload_size，更新 next_seqno

        if (_stream.eof() && window_not_full(window_size)) {
            segment.header().fin = true;
            _set_fin_flag = true;
            ++_next_seqno;  // 发了fin，更新 next_seqno
        }

        if (!_set_fin_flag && payload_size == 0) return;
    }

    // std::cout << "seqno: " << segment.header().seqno << std::endl;
    // std::cout << "payload: " << segment.payload().str() << std::endl;
    _segments_out.push(segment);
    _outstanding_segments.push(segment);
    _retransmission_timer.start_timer();

    if (window_not_full(window_size)) {
        fill_window();
    }
}


//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint64_t abs_ackno = unwrap(ackno, _isn, _next_seqno);

    // 不可靠的ack，直接return
    if (abs_ackno > _next_seqno || abs_ackno < _receiver_ack)
        return;

    bool is_ack_update = false;
    _receiver_window_size = window_size;

    // 检查发送队列里的segment
    while (!_outstanding_segments.empty()) {
        auto segment = _outstanding_segments.front();
        auto seqno = segment.header().seqno;
        uint64_t abs_seqno = unwrap(seqno, _isn, _next_seqno);

        // segment的end <= ackno，说明该segment接收成功，从发送队列里pop
        if (abs_seqno + segment.length_in_sequence_space() <= abs_ackno) {
            _receiver_ack = abs_seqno + segment.length_in_sequence_space();
            is_ack_update = true;
            _outstanding_segments.pop();
        }
        // 否则没有接收成功，后面的segment一定也没有接收成功
        else
            break;
    }

    // 所有的segment都接收了，关闭定时器
    if (_outstanding_segments.empty()) {
        _retransmission_timer.stop_timer();
    }

    // ack有变化，说明有收到ack，重置定时器，重传次数归0
    if (is_ack_update) {
        _retransmission_timer.set_timeout(_initial_retransmission_timeout);
        _retransmission_timer.set_accumulate_time(0);
        _consecutive_retransmissions = 0;
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    if (_retransmission_timer.check_timeout(ms_since_last_tick)) {
        if (_receiver_window_size > 0) {
            _retransmission_timer.handle_timeout();
        }
        else if (_receiver_window_size == 0) {
            _retransmission_timer.set_timeout(_initial_retransmission_timeout);
            _retransmission_timer.set_accumulate_time(0);
        }
        ++_consecutive_retransmissions;
        segments_out().push(_outstanding_segments.front());
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

void TCPSender::send_empty_segment() {
    TCPSegment segment{};
    segment.header().seqno = next_seqno();
    _segments_out.push(segment);
}