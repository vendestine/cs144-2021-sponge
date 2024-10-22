#include "tcp_sender.hh"

#include "retransmission_timer.hh"
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
    , _timer(retx_timeout) {}

uint64_t TCPSender::bytes_in_flight() const { return _bytes_in_fight; }

void TCPSender::fill_window() {
    // close状态: 发送syn
    if (_next_seqno == 0) {
        _receiver_window_size = 1;
        TCPSegment segment{};
        segment.header().syn = true;
        send_segment(segment);
        return;
    }
    // syn_sent状态: syn还没有ack，什么都不做
    else if (_next_seqno > 0 && _next_seqno == bytes_in_flight()) {
        return;
    }
    // syn_ack状态：可以发送 data和fin （发data，发data + fin，发fin，发空）
    else if (_next_seqno > bytes_in_flight() && _next_seqno < stream_in().bytes_written() + 2) {
        uint64_t window_size = (_receiver_window_size == 0) ? 1 : _receiver_window_size;
        while (bytes_in_flight() < window_size) {
            TCPSegment segment{};
            // 发data
            size_t payload_size =
                std::min({window_size - bytes_in_flight(), _stream.buffer_size(), TCPConfig::MAX_PAYLOAD_SIZE});
            segment.payload() = Buffer(_stream.read(payload_size));

            // 判断是否发fin
            if (!_fin_sent && _stream.eof() && bytes_in_flight() + segment.length_in_sequence_space() < window_size) {
                segment.header().fin = true;
                _fin_sent = true;
            }

            // 如果segment是空，不发送，直接退出循环
            if (segment.length_in_sequence_space() == 0)
                break;
            // 否则继续循环发送
            send_segment(segment);
        }
    }
    // fin_sent状态 / fin_acked状态，什么都不做
    else if (_fin_sent) {
        return;
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    // 拿到ack
    uint64_t abs_ackno = unwrap(ackno, _isn, _next_seqno);

    // 不可靠的ack，直接return
    if (abs_ackno > _next_seqno || abs_ackno < _receiver_ackno)
        return;

    bool is_ack_update = false;
    _receiver_window_size = window_size;

    // 检查已发出但还未ack的segment是否ack
    while (!_outstanding_segments.empty()) {
        auto segment = _outstanding_segments.front();
        auto seqno = segment.header().seqno;
        uint64_t abs_seqno = unwrap(seqno, _isn, _next_seqno);

        // segment的end <= ackno，说明该segment接收成功，从发送队列里pop
        if (abs_seqno + segment.length_in_sequence_space() <= abs_ackno) {
            _outstanding_segments.pop();
            _bytes_in_fight -= segment.length_in_sequence_space();
            _receiver_ackno = abs_seqno + segment.length_in_sequence_space();
            is_ack_update = true;
        }
        // 否则没有接收成功，后面的segment一定也没有接收成功
        else
            break;
    }

    // ack有变化，说明有收到新的ack，重置定时器
    if (is_ack_update) {
        _timer.reset_timer();
        _consecutive_retransmissions = 0;
    }

    // 所有的segment都接收了，关闭定时器
    if (_outstanding_segments.empty()) {
        _timer.stop_timer();
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    // 如果timeout了
    if (_timer.check_timeout(ms_since_last_tick)) {
        if (_receiver_window_size == 0) {
            _timer.reset_timer();
        } else if (_receiver_window_size > 0) {
            _timer.handle_timeout();
        }
        // timeout之后，两种情况最后都需要重传segment
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

void TCPSender::send_segment(TCPSegment &segment) {
    segment.header().seqno = next_seqno();
    _segments_out.push(segment);
    _outstanding_segments.push(segment);
    _next_seqno += segment.length_in_sequence_space();
    _bytes_in_fight += segment.length_in_sequence_space();
    _timer.start_timer();
}
