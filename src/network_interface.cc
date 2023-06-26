#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"
#include <iostream>

using namespace std;

const size_t arp_expired_time = 30000;
const size_t arp_repeat_time = 5000;
const EthernetAddress eth_broadcast_addr { 255, 255, 255, 255, 255, 255 };

// ethernet_address: Ethernet (what ARP calls "hardware") address of the interface
// ip_address: IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( const EthernetAddress& ethernet_address, const Address& ip_address )
  : ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
  , ip2eth_map_()
  , arp_req_sending_()
  , eth_frame_out_()
  , datagram_out_()
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address_ ) << " and IP address "
       << ip_address.ip() << "\n";
}

// dgram: the IPv4 datagram to be sent
// next_hop: the IP address of the interface to send it to (typically a router or default gateway, but
// may also be another host if directly connected to the same network as the destination)

// Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) by using the
// Address::ipv4_numeric() method.

// 发送数据报：若能得到以太网地址则直接发送，否则发送ARP请求，一定时间内不发送重复地址的ARP请求
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  auto it = ip2eth_map_.find( next_hop );
  EthernetFrame frame;
  if ( it != ip2eth_map_.end() ) {       // 能找到对应以太网地址
    frame.header.dst = it->second.first; // 设置以太网头部
    frame.header.src = ethernet_address_;
    frame.header.type = EthernetHeader::TYPE_IPv4;
    Serializer s;
    dgram.serialize( s ); // 将数据报序列化，放在以太帧的数据中
    frame.payload = s.output();
    eth_frame_out_.push( frame );
  } else {                                    // 无法找到对应以太网地址
    if ( arp_req_sending_.count( next_hop ) ) // 同一地址的arp请求未过5秒，为避免泛洪，不发起请求
      return;
    arp_req_sending_[next_hop] = 0; // 记录当前发起的arp请求, 只会被recieve和tick函数删除
    datagram_out_.push( { dgram, next_hop } ); // 数据报进行等待

    frame.header.type = EthernetHeader::TYPE_ARP; // 设置以太网头部
    frame.header.src = ethernet_address_;
    frame.header.dst = eth_broadcast_addr;
    ARPMessage arp_msg;
    arp_msg.opcode = ARPMessage::OPCODE_REQUEST; // 设置ARP头部
    arp_msg.sender_ethernet_address = ethernet_address_;
    arp_msg.sender_ip_address = ip_address_.ipv4_numeric();
    arp_msg.target_ethernet_address = {};
    arp_msg.target_ip_address = next_hop.ipv4_numeric();
    Serializer s;
    arp_msg.serialize( s );
    frame.payload = s.output(); // 将arp帧放入以太帧数据中
    eth_frame_out_.push( frame );
    // arp_msg.
  }
}

// 接受到以太网帧应该进行的处理，包括处理IP消息，ARP请求、回复消息，以及对等待ARP回复的数据报进行发送
// frame: the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  // 非广播地址或当前主机以太地址，则跳过
  if ( frame.header.dst != ethernet_address_ && frame.header.dst != eth_broadcast_addr )
    return nullopt;
  if ( frame.header.type == EthernetHeader::TYPE_IPv4 ) { // 若为IPv4信息
    InternetDatagram dgram;
    if ( parse( dgram, frame.payload ) ) { // 能反序列化就直接返回
      return dgram;
    } else {
      return nullopt;
    }
  } else if ( frame.header.type == EthernetHeader::TYPE_ARP ) { // 若为ARP消息
    ARPMessage arp;
    if ( parse( arp, frame.payload ) ) { // 反序列化成功
      auto arp_sender_ip = Address::from_ipv4_numeric( arp.sender_ip_address );
      if ( arp.opcode == arp.OPCODE_REPLY ) {            // arp请求
        if ( arp_req_sending_.count( arp_sender_ip ) ) { // 从正在发送arp哈希表中移除
          arp_req_sending_.erase( arp_sender_ip );
        }
        // 记录IP到eth映射
        ip2eth_map_[arp_sender_ip] = { arp.sender_ethernet_address, arp_expired_time };
        while ( !datagram_out_.empty() ) {
          auto front = datagram_out_.front();
          // 如果数据报队列中存在和当前收到arp地址一样的数据报，则调用send_datagram进行发送
          if ( front.second.ipv4_numeric() == arp.sender_ip_address ) {
            send_datagram( front.first, front.second );
            datagram_out_.pop();
          } else {
            break;
          }
        }
      } else if ( arp.opcode == arp.OPCODE_REQUEST
                  && arp.target_ip_address
                       == ip_address_.ipv4_numeric() ) { // 收到arp request且其目的地址为当前主机地址才进行处理
        // 收到请求，也需要记录下对方的IP到eth地址
        ip2eth_map_[arp_sender_ip] = { arp.sender_ethernet_address, arp_expired_time };
        ARPMessage arp_msg;
        arp_msg.opcode = ARPMessage::OPCODE_REPLY; // 填充恢复arp首部
        arp_msg.sender_ethernet_address = ethernet_address_;
        arp_msg.sender_ip_address = ip_address_.ipv4_numeric();
        arp_msg.target_ethernet_address = arp.sender_ethernet_address;
        arp_msg.target_ip_address = arp.sender_ip_address;
        Serializer s;
        arp_msg.serialize( s );
        EthernetFrame arp_reply_frame;
        arp_reply_frame.payload = s.output(); // 填充恢复arp的以太帧数据
        arp_reply_frame.header.dst = arp.sender_ethernet_address;
        arp_reply_frame.header.src = ethernet_address_;
        arp_reply_frame.header.type = EthernetHeader::TYPE_ARP;
        eth_frame_out_.push( arp_reply_frame );
      }
    }
  }
  return nullopt;
}

// 主要是对arp记录过期的处理，及重复ARP请求记录的处理
// ms_since_last_tick: the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  auto it = ip2eth_map_.begin();
  while ( it != ip2eth_map_.end() ) { // 判断ip2eth映射记录是否超时，超时则移除
    if ( it->second.second > ms_since_last_tick ) {
      it->second.second -= ms_since_last_tick;
      ++it;
    } else {
      auto last = it;
      ++it;
      ip2eth_map_.erase( last );
    }
  }
  auto arp_it = arp_req_sending_.begin();
  while ( arp_it != arp_req_sending_.end() ) { // arp请求发送时间是否超时，超时则可以重新发送重复地址arp
    arp_it->second += ms_since_last_tick;
    if ( arp_it->second >= arp_repeat_time ) {
      auto last = arp_it;
      ++arp_it;
      arp_req_sending_.erase( last );
    } else {
      ++arp_it;
    }
  }
}

// 从帧队列首部取出帧进行发送
optional<EthernetFrame> NetworkInterface::maybe_send()
{
  // std::cout<<"maybe_send running...\n";
  if ( eth_frame_out_.empty() )
    return nullopt;
  auto front = eth_frame_out_.front();
  eth_frame_out_.pop();
  // std::cout << front.header.to_string() << std::endl;
  return front;
}
