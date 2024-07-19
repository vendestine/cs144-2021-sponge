#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity): q(), capa(capacity), idx(-1), written_bytes(0), read_bytes(0), input_flag(false)
{
}

size_t ByteStream::write(const string &data) {
    size_t cnt = 0;
    for (auto c: data) {
        if (q.size() < capa) {
            ++ idx;
            q.push_front(c);
            ++ cnt;
            ++ written_bytes;
        }
    }
    return cnt;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    string res = "";
    int i = idx;
    for (size_t j = 0; j < len && i >= 0; -- i, ++ j) {
        res += q[i];
    }
    return res;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) { 
    for (size_t i = 0; i < len; i ++) {
        if (q.size()) {
            -- idx;
            q.pop_back();
            ++ read_bytes;
        }
    }
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string res = "";
    for (size_t i = 0; i < len; i ++) {
        if (q.size()) {
            -- idx;
            q.pop_back();
            res += q[i];
            ++ read_bytes;
        }
    }
    return res;
}

void ByteStream::end_input() { input_flag = true; }

bool ByteStream::input_ended() const { return input_flag; }

size_t ByteStream::buffer_size() const { return q.size(); }

bool ByteStream::buffer_empty() const { return q.empty(); }

bool ByteStream::eof() const { return buffer_empty() && input_flag; }

size_t ByteStream::bytes_written() const { return written_bytes; }

size_t ByteStream::bytes_read() const { return read_bytes; }

size_t ByteStream::remaining_capacity() const { return capa - q.size(); }
