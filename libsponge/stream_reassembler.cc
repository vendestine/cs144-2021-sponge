#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : 
    _buffer(capacity,'\0'),
    _bitmap(capacity, false),
    _eof_flag(false),
    _eof_index(0),
    _unassembled_bytes(0),
    _output(capacity),
    _capacity(capacity){}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    // 初始化 三个重要的变量
    auto first_unread = _output.bytes_read();
    auto first_unassembled = _output.bytes_written();
    auto first_unacceptable = first_unread + _capacity;
    
    // data在字节流中的区间 [index, index + data.size()), 注意是左开右闭
    auto start = index, end = index + data.size(); 
    
    // 当segment的eof为true，说明是最后一段，记录segment最后一位的索引
    if(eof) {
        _eof_flag = true;
        _eof_index = end;
    }

    // case1：当前segment完全在窗口外，不处理
    if (end <= first_unassembled || start >= first_unacceptable) return;

    // case 2: 当前segment的左部分和窗口有交集（可能完全在窗口里），交集的部分放入窗口
    if (start >= first_unassembled) {
        end = min(end, first_unacceptable);
        auto len = end - start;
        auto offset = index - first_unassembled;

        for (size_t i = 0; i < len; i ++) {
            if (_bitmap[i + offset]) continue; //已经在窗口里了，重复字符直接丢弃 
            _buffer[i + offset] = data[i];
            _bitmap[i + offset] = true;
            ++ _unassembled_bytes;
        }
    }

    // case 3: 当前segment的右部分和窗口有交集 或者 segment覆盖了窗口，交集的部分放入窗口
    else if (end > first_unassembled) {
        start = max(start, first_unassembled);
        end = min(end, first_unacceptable);
        auto len = end - start;
        auto offset = first_unassembled - index;

        for (size_t i = 0; i < len; i ++) {
            if (_bitmap[i]) continue;
            _buffer[i] = data[i + offset];
            _bitmap[i] = true;
            ++ _unassembled_bytes;
        }
    }

    // 把_buffer窗口里的连续字节写入byteStream里
    string str;
    while (_bitmap.front()) {
        auto c = _buffer.front();
        str.push_back(c);
        _buffer.pop_front();
        _bitmap.pop_front();
        _buffer.push_back('\0');
        _bitmap.push_back(false);
    }
    if (str.size() > 0) {
        _output.write(str);
        _unassembled_bytes -= str.size();
    }



    // 之前已经出现过eof，并且包括eof前的所有字节已经写进去，停止input
    if (_eof_flag && _eof_index == _output.bytes_written()) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled_bytes; }

bool StreamReassembler::empty() const { return _unassembled_bytes == 0; }