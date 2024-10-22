#include "stream_reassembler.hh"

#include <iostream>

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _output(capacity), _capacity(capacity), _window(capacity, '\0') {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    // eof出现过，同时记录该segment的结尾
    if (eof) {
        _eof = true;
        _eof_segment_end = index + data.size();
    }

    // 每次push_substring之前，因为内部bytestream可能已经read了一些bytes，window会变化
    _window_size = _capacity - _output.buffer_size();
    _window.resize(_window_size, '\0');
    _bitmap.resize(_window_size, false);

    // case1: data 和 window没有交集，直接丢弃
    if (index + data.size() <= _window_start || index >= _window_start + _window_size) {
        if (_eof && empty() && _eof_segment_end <= _window_start + _window_size)  // 不要忘记判断是否eof
            _output.end_input();
        return;
    }

    // case2: data 和 window有交集
    // 具体又可以分为 很多部分 data和window左部分重合，和window右部分重合，data在window里，data包含window
    // 处理的逻辑可以是一致的，截取data在window的部分即可
    size_t data_start = max(index, _window_start);
    size_t data_end = min(index + data.size(), _window_start + _window_size);

    // 将data放入window里
    for (auto i = data_start; i < data_end; i++) {
        size_t window_index = i - _window_start;
        size_t data_index = i - index;
        if (_bitmap[window_index] == false) {
            _window[window_index] = data[data_index];
            _bitmap[window_index] = true;
            _bytes_in_window++;
        }
    }

    // check window里 是否有满足条件的字符串，如果有就写入内部的byte_stream
    // 要从window中取字符才是符合逻辑的，而不是从data字符串中取字符
    string str{};
    while (!_window.empty()) {
        if (_bitmap.front() == false)
            break;
        str.push_back(_window.front());
        _window.pop_front();
        _bitmap.pop_front();
        _bytes_in_window--;
    }

    _output.write(str);
    _window_start = _output.bytes_written();
    _window_size = _capacity - _output.buffer_size();

    // 如果窗口为空，eof_segment出现过，eof_segment的最后一位也已经被窗口接收，不再往byte_stream里写入
    if (_eof && empty() && _eof_segment_end <= _window_start + _window_size)
        _output.end_input();
}

size_t StreamReassembler::unassembled_bytes() const { return _bytes_in_window; }

bool StreamReassembler::empty() const { return _bytes_in_window == 0; }
