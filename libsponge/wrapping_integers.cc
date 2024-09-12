#include "wrapping_integers.hh"

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    // 重载运算符里定义了一些WrappingInt32的常用运算
    // WrappingInt32类对象和unit64_t类对象的之间的运算，WrappingInt32对象必须放在前面，因为重载运算符只定义了这种运算
    // 64位转32位，大值域 映射到 小值域，不会出现unit64_t对象映射到多个WrappingInt32对象的情况，所以可以直接计算
    // unit64计算后溢出后会直接从头开始存储，满足图片上的眼球
    return isn + static_cast<uint32_t>(n);  
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
    // 其实重载运算符里已经给了我们提示了，这里给出了wrappingint32位对象相减 返回int32_t的函数
    // 说明希望计算真正（有正负）的偏移量
    int32_t offset = n - wrap(checkpoint, isn);
    int64_t result = checkpoint + offset;   // unit64_t对象 + int32_t对象 自动转化为unit64_t对象
    
    if (result < 0) result = result + (1ul << 32);
    return result;
}
