#include "wrapping_integers.hh"

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    // 64位 -> 32位，大值域->小值域，映射唯一
    // seqno = (abs_seqno + isn) mod 2^32 = abs_seqno mod 2^32 + isn mod 2^32
    return WrappingInt32{isn + static_cast<uint32_t>(n)};
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    // 32位 -> 64位，小值域 -> 大值域，一对多
    // seqno可以映射到很多abs_seqno (abs_seno + k * 2^32),  我们要找的就是0-2^32之间的那个abs_seqno
    // 注意checkpoint虽然是uint64_t, 但是为了筛选[0, 2^32)的abs_seqno，它的实际值不会超过2^33
    // 因为只有这样，才能有选中[0, 2^32)的abs_seqno的可能

    int32_t diff = n - wrap(checkpoint, isn);
    int64_t abs_seqno = checkpoint + diff;
    if (abs_seqno < 0)
        abs_seqno = abs_seqno + (1ul << 32);
    return static_cast<uint64_t>(abs_seqno);
}
