#include "byte_stream.hh"

#include <deque>

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) : _capacity(capacity) {}

size_t ByteStream::write(const string &data) {
    size_t write_count = min(data.size(), remaining_capacity());
    for (auto it = data.begin(); it < data.begin() + write_count; ++it) {
        _buffer.push_back(*it);
    }
    _bytes_write += write_count;
    return write_count;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    string str{};
    if (buffer_size() >= len) {
        for (auto it = _buffer.begin(); it < _buffer.begin() + len; it++) {
            str.push_back(*it);
        }
    }
    return str;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t read_count = 0;
    if (buffer_size() >= len) {
        int count = len;
        for (int i = 0; i < count; i++) {
            _buffer.pop_front();
            read_count++;
        }
    }
    _bytes_read += read_count;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    auto str = peek_output(len);
    pop_output(len);
    return str;
}

void ByteStream::end_input() { _end_input = true; }

bool ByteStream::input_ended() const { return _end_input; }

size_t ByteStream::buffer_size() const { return _buffer.size(); }

bool ByteStream::buffer_empty() const { return _buffer.empty(); }

bool ByteStream::eof() const { return input_ended() && buffer_empty(); }

size_t ByteStream::bytes_written() const { return _bytes_write; }

size_t ByteStream::bytes_read() const { return _bytes_read; }

size_t ByteStream::remaining_capacity() const { return _capacity - _buffer.size(); }
