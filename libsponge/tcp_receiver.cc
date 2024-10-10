#include "tcp_receiver.hh"

#include "wrapping_integers.hh"

#include <optional>

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    // tcp segment必须按照状态机的顺序到达，不是按照顺序的会放弃
    // 有可能一个tcp segment，会在一次函数里依次进入三个状态（ 直接发一个syn + data + fin的segment）

    // listen状态：没有收到syn
    if (!_isn.has_value()) {
        if (seg.header().syn) {
            _isn = seg.header().seqno;
        } else
            return;
    }

    // syn_recv状态: 收到过syn （包含很多种可能，此时有可能是 syn，syn + data, syn + data + fin, data + fin, fin;)
    // syn_recv状态里，可以接收segment，然后push进底层的reassembler
    if (_isn.has_value() && !_reassembler.stream_out().input_ended()) {
        // 写入reassembler
        string data = seg.payload().copy();
        uint64_t ack_index = _reassembler.stream_out().bytes_written();
        uint64_t abs_seqno = unwrap(seg.header().seqno, _isn.value(), ack_index);
        // uint64_t index = (abs_seqno == 0) ? 0 : abs_seqno - 1;
        uint64_t index = (seg.header().syn) ? 0 : abs_seqno - 1;
        bool eof = seg.header().fin;
        _reassembler.push_substring(data, index, eof);
    }

    // fin_recv状态：不仅仅是收到过fin，而是fin segment已经从窗口中滑出了
    if (_reassembler.stream_out().input_ended()) {
        return;
    }
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    // listen状态时，isn为空，ackno为空
    if (!_isn.has_value()) {
        return std::nullopt;
    }

    // syn_recv状态和fin状态时，isn不为空，ack_index不为空
    uint64_t ack_index = _reassembler.stream_out().bytes_written();
    // uint64_t abs_ackno = ack_index + 1 + _fin_sent;
    // ack_index 在 fin的对应位时，不仅仅是发过fin，而是fin的segment全都从窗口划出了，对应着input_end
    uint64_t abs_ackno = ack_index + 1 + _reassembler.stream_out().input_ended();
    optional<WrappingInt32> ackno = wrap(abs_ackno, _isn.value());
    return ackno;
}

size_t TCPReceiver::window_size() const { return _capacity - _reassembler.stream_out().buffer_size(); }