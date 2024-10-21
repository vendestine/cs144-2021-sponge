#include "network_interface.hh"

#include "arp_message.hh"
#include "buffer.hh"
#include "ethernet_frame.hh"
#include "ethernet_header.hh"
#include "ipv4_datagram.hh"
#include "parser.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();
    
    // 如果ip在arp_cache_table中，直接封装成frame，然后发送
    if (_arp_cache_table.count(next_hop_ip)) {
        EthernetFrame frame;
        frame.header().src = _ethernet_address;
        frame.header().dst = _arp_cache_table[next_hop_ip]._mac;
        frame.header().type = EthernetHeader::TYPE_IPv4;
        frame.payload() = dgram.serialize();
        _frames_out.push(frame);
    }
    // ip不在arp_cache_table中，缓存datagram，发送arp request
    else {
        // 把datagram缓存起来
        _datagram_cache_queue.emplace_back(DatagramCacheEntry{next_hop, dgram});

        // 如果对该ip的arp request已经发过(且没超过5s) 不再发送arp request
        if (ip_arp_request_time.count(next_hop_ip)) return;

        // 对该ip发送arp request
        EthernetFrame arp_request_frame;
        arp_request_frame.header().dst = ETHERNET_BROADCAST;
        arp_request_frame.header().src = _ethernet_address;
        arp_request_frame.header().type = EthernetHeader::TYPE_ARP;
        
        ARPMessage arp_request_message;
        arp_request_message.opcode = ARPMessage::OPCODE_REQUEST;
        arp_request_message.sender_ethernet_address = _ethernet_address;
        arp_request_message.sender_ip_address = _ip_address.ipv4_numeric();
        arp_request_message.target_ethernet_address = {};
        arp_request_message.target_ip_address = next_hop_ip;
        arp_request_frame.payload() = BufferList(arp_request_message.serialize());
        
        _frames_out.push(arp_request_frame);
        ip_arp_request_time[next_hop_ip] = _time_counter;
    } 
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    //  ignore any frames not destined for the network interface
    if (frame.header().dst != ETHERNET_BROADCAST && frame.header().dst != _ethernet_address) {
        return nullopt;
    }
    
    // 如果frame是ipv4 type
    if (frame.header().type == EthernetHeader::TYPE_IPv4) {
        InternetDatagram ipv4_datagram;
        if (ipv4_datagram.parse(frame.payload()) == ParseResult::NoError) {
            return ipv4_datagram;
        }
        else return nullopt;
    }
    // 如果frame是arp type
    else if (frame.header().type == EthernetHeader::TYPE_ARP) {
        ARPMessage arp_message;
        if (arp_message.parse(frame.payload()) == ParseResult::NoError) {
            // Learn mappings from both requests and replies
            _arp_cache_table[arp_message.sender_ip_address] = {arp_message.sender_ethernet_address, _time_counter};

            // receive arp request，send arp reply
            if (arp_message.opcode == ARPMessage::OPCODE_REQUEST && arp_message.target_ip_address == _ip_address.ipv4_numeric()) {
                EthernetFrame arp_reply_frame;
                // header
                arp_reply_frame.header().dst = arp_message.sender_ethernet_address;
                arp_reply_frame.header().src = _ethernet_address;
                arp_reply_frame.header().type = EthernetHeader::TYPE_ARP;
                // payload
                ARPMessage arp_reply_message;
                arp_reply_message.opcode = ARPMessage::OPCODE_REPLY;
                arp_reply_message.sender_ethernet_address = _ethernet_address;
                arp_reply_message.sender_ip_address = _ip_address.ipv4_numeric();
                arp_reply_message.target_ethernet_address = arp_message.sender_ethernet_address;
                arp_reply_message.target_ip_address = arp_message.sender_ip_address;
                arp_reply_frame.payload() = BufferList(arp_reply_message.serialize());
            
                _frames_out.push(arp_reply_frame);
            }
            // receive arp reply, send datagram cache
            if (arp_message.opcode == ARPMessage::OPCODE_REPLY) {
                for (auto it = _datagram_cache_queue.begin(); it != _datagram_cache_queue.end();) {
                    // 收到某ip的arp reply，就将对该ip缓存的datagram全部发出去
                    if (it->_ip.ipv4_numeric() == arp_message.sender_ip_address) {
                        send_datagram(it->_datagram, it->_ip);
                        _datagram_cache_queue.erase(it ++);
                    }
                    else it++;
                }
                // 此时对该ip的arp_request完成
                ip_arp_request_time.erase(arp_message.sender_ip_address);
            }
        }
    }
    return nullopt;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    _time_counter += ms_since_last_tick;

    // 删除arp_cache_table中ttl超过30s的entry
    for (auto it = _arp_cache_table.begin(); it != _arp_cache_table.end();) {
        auto ethernet_entry = it->second;
        if (_time_counter - ethernet_entry._time >= 30 * 1000) {
            _arp_cache_table.erase(it ++);
        }
        else it ++;
    }

    // ip_arp_request_time中ttl超过5s的entry，重发arp request
    for (auto it = ip_arp_request_time.begin(); it != ip_arp_request_time.end(); it++) {
        auto ip = it->first;
        auto arp_request_time = it->second;
        if (_time_counter - arp_request_time >= 5 * 1000) {
            // 对该ip发送arp request
            EthernetFrame arp_request_frame;
            arp_request_frame.header().dst = ETHERNET_BROADCAST;
            arp_request_frame.header().src = _ethernet_address;
            arp_request_frame.header().type = EthernetHeader::TYPE_ARP;
            
            ARPMessage arp_request_message;
            arp_request_message.opcode = ARPMessage::OPCODE_REQUEST;
            arp_request_message.sender_ethernet_address = _ethernet_address;
            arp_request_message.sender_ip_address = _ip_address.ipv4_numeric();
            arp_request_message.target_ethernet_address = {};
            arp_request_message.target_ip_address = ip;
            arp_request_frame.payload() = BufferList(arp_request_message.serialize());
            
            _frames_out.push(arp_request_frame);
            ip_arp_request_time[ip] = _time_counter;
        }
    }
}
