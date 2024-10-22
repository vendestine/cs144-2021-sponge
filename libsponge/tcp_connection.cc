#include "tcp_connection.hh"

#include "tcp_segment.hh"
#include "wrapping_integers.hh"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <limits>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_segment_received; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    _time_since_last_segment_received = 0;

    // 1. if the rst (reset) flag is set
    if (seg.header().rst) {
        set_error();
        return;
    }

    // 2. gives the segment to the TCPReceiver
    _receiver.segment_received(seg);

    if (check_inbound_stream_end() && !_sender.stream_in().eof()) {
        _linger_after_streams_finish = false;
    }

    // 3. if the ack flag is set, tells the TCPSender
    if (seg.header().ack) {
        // ack_rst特殊情况，listen时收到ack直接丢弃！
        if (!_receiver.ackno().has_value())
            return;
        _sender.ack_received(seg.header().ackno, seg.header().win);
        _sender.fill_window();
        send_complete_segment();
    }

    // 4. if the incoming segment occupied any sequence numbers
    if (seg.length_in_sequence_space() > 0) {
        _sender.fill_window();

        // 发送队列可能为空，如果为空，发一个empty_segment
        if (!_sender.segments_out().empty()) {
            send_complete_segment();
        } else {
            _sender.send_empty_segment();
            send_complete_segment();
        }
    }

    // 5. extra special case: responding to a “keep-alive” segment
    if (_receiver.ackno().has_value() && (seg.length_in_sequence_space() == 0) &&
        seg.header().seqno == _receiver.ackno().value() - 1) {
        _sender.send_empty_segment();
        send_complete_segment();
    }
}

bool TCPConnection::active() const { return _active; }

size_t TCPConnection::write(const string &data) {
    auto length = _sender.stream_in().write(data);
    _sender.fill_window();
    send_complete_segment();
    return length;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    // 1. tell the TCPSender about the passage of time
    _time_since_last_segment_received += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);

    // 2. retransmit segment
    if (!_sender.segments_out().empty()) {
        // 超过最大重传次数，传rst segment
        if (_sender.consecutive_retransmissions() > _cfg.MAX_RETX_ATTEMPTS) {
            set_error();
            send_rst_segment();
        }
        // 正常重传，重传outbound的队头
        else {
            auto front_segment = _sender.segments_out().front();
            _sender.segments_out().pop();
            add_ack_and_window_to_segment(front_segment);
            _segments_out.push(front_segment);
        }
    }

    // 3. end the connection cleanly if necessary
    if (check_inbound_stream_end() && check_outbound_stream_end()) {
        if (!_linger_after_streams_finish) {
            _active = false;
        } else if (_time_since_last_segment_received >= 10 * _cfg.rt_timeout) {
            _active = false;
        }
    }
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    send_complete_segment();
}

void TCPConnection::connect() {
    _sender.fill_window();
    send_complete_segment();
}

void TCPConnection::add_ack_and_window_to_segment(TCPSegment &segment) {
    // 加上receiver的ackno和window_size
    if (_receiver.ackno().has_value()) {
        segment.header().ack = true;
        segment.header().ackno = _receiver.ackno().value();
    }
    if (_receiver.window_size() > std::numeric_limits<uint16_t>::max()) {
        segment.header().win = std::numeric_limits<uint16_t>::max();
    } else
        segment.header().win = static_cast<uint16_t>(_receiver.window_size());
}

void TCPConnection::send_complete_segment() {
    // 循环取出sender的outbound queue里的segment，加上ackno和window_size后，放入tcp connection的outbound queue
    while (!_sender.segments_out().empty()) {
        // 取出segment
        auto segment = _sender.segments_out().front();
        _sender.segments_out().pop();

        // 加上receiver的ackno和window_size
        add_ack_and_window_to_segment(segment);

        // 放入tcp connection里的outbound queue
        _segments_out.push(segment);
    }
}

bool TCPConnection::check_inbound_stream_end() {
    return _receiver.unassembled_bytes() == 0 && _receiver.stream_out().input_ended();
}

bool TCPConnection::check_outbound_stream_end() { return _sender.stream_in().eof() && _sender.bytes_in_flight() == 0; }

void TCPConnection::set_error() {
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    _active = false;
}

void TCPConnection::send_rst_segment() {
    while (!_sender.segments_out().empty())
        _sender.segments_out().pop();

    _sender.send_empty_segment();
    _sender.segments_out().front().header().rst = true;
    send_complete_segment();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            set_error();
            send_rst_segment();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

// checkout test
