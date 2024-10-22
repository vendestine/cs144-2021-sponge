#include "router.hh"
#include "address.hh"

#include <cstdint>
#include <iostream>

using namespace std;

// Dummy implementation of an IP router

// Given an incoming Internet datagram, the router decides
// (1) which interface to send it out on, and
// (2) what next hop address to send it to.

// For Lab 6, please replace with a real implementation that passes the
// automated checks run by `make check_lab6`.

// You will need to add private members to the class declaration in `router.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

//! \param[in] route_prefix The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
//! \param[in] prefix_length For this route to be applicable, how many high-order (most-significant) bits of the route_prefix will need to match the corresponding bits of the datagram's destination address?
//! \param[in] next_hop The IP address of the next hop. Will be empty if the network is directly attached to the router (in which case, the next hop address should be the datagram's final destination).
//! \param[in] interface_num The index of the interface to send the datagram out on.
void Router::add_route(const uint32_t route_prefix,
                       const uint8_t prefix_length,
                       const optional<Address> next_hop,
                       const size_t interface_num) {
    cerr << "DEBUG: adding route " << Address::from_ipv4_numeric(route_prefix).ip() << "/" << int(prefix_length)
         << " => " << (next_hop.has_value() ? next_hop->ip() : "(direct)") << " on interface " << interface_num << "\n";

    // 添加一个route rule到router table里去
    auto route_rule = RouteRule(route_prefix, prefix_length, next_hop, interface_num);
    _route_table.push_back(std::move(route_rule));
}

//! \param[in] dgram The datagram to be routed
void Router::route_one_datagram(InternetDatagram &dgram) {
    auto dst_ip = std::move(dgram.header().dst);

    int max_match_index = -1;
    int max_match_len = -1;
    for (size_t i = 0; i < _route_table.size(); i ++) {
        // 计算当前route rule的前缀掩码
        uint32_t mask = get_mask(_route_table[i].prefix_length);
        // 找dst_ip在route table中的最长前缀匹配
        if (_route_table[i].prefix_length > max_match_len && (dst_ip & mask) == _route_table[i].route_prefix) {
            max_match_index = i;
            max_match_len = _route_table[i].prefix_length;
        }
    }
    
    // 如果没有匹配的路由，路由器会丢弃数据报
    if (max_match_index == -1) return;
    
    // The router decrements the datagram’s TTL (time to live). If the TTL was zero already,
    // or hits zero after the decrement, the router should drop the datagram.
    if (dgram.header().ttl == 0 || dgram.header().ttl == 1) return;
    --dgram.header().ttl;

    // the router sends the modified datagram on the appropriate interface to the appropriate next hop
    if (_route_table[max_match_index].next_hop.has_value()) {
        // 下一跳不为空，说明下一跳ip地址为 下一个路由的ip地址
        auto interface_num = std::move(_route_table[max_match_index].interface_num);
        auto next_hop = std::move(_route_table[max_match_index].next_hop.value());
        interface(interface_num).send_datagram(dgram, next_hop);
    }
    else {
        // 下一跳为空，说明下一跳ip地址为 目标ip地址
        auto interface_num = std::move(_route_table[max_match_index].interface_num);
        auto next_hop = Address::from_ipv4_numeric(std::move(dgram.header().dst));
        interface(interface_num).send_datagram(dgram, next_hop);
    }

}

uint32_t Router::get_mask(uint8_t prefix_length) {
    if (prefix_length == 0) return 0;
    if (prefix_length == 32) return 0xffffffff;
    return ~((1 << (32 - prefix_length)) -1);
}

void Router::route() {
    // Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
    for (auto &interface : _interfaces) {
        auto &queue = interface.datagrams_out();
        while (not queue.empty()) {
            route_one_datagram(queue.front());
            queue.pop();
        }
    }
}
