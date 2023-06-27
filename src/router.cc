#include "router.hh"

#include <iostream>
#include <limits>

using namespace std;

Router::Router() : route_map_() {}

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";
  SubNetAddr addr { route_prefix, prefix_length };
  auto it = route_map_.find( addr );
  if ( it != route_map_.end() ) { // 如果存在，则进行更新
    it->second.interface_num = interface_num;
    it->second.next_hop = next_hop;
  } else { // 插入
    // route_map_[addr]={ next_hop, interface_num };//奇怪，这样会编译不过
    route_map_.insert( { addr, { next_hop, interface_num } } );
  }
}

// 遍历所有接口，每次只处理一个报文或将一个接口中的报文处理完皆可
void Router::route()
{
  for ( size_t i = 0; i < interfaces_.size(); ++i ) {
    while ( true ) {
      auto dgram = interfaces_[i].maybe_receive();
      if ( dgram == nullopt ) // 无报文则处理下一个接口
        break;
      if ( dgram->header.ttl <= 1 ) // ttl小于1则直接丢弃
        continue;
      --dgram->header.ttl;                              // 减少ttl数
      auto info = find_best_match( dgram->header.dst ); // 找到最匹配前缀和
      if ( info == nullopt ) {                          // 丢弃报文，处理下一个报文
        // std::cout << "dont find match route" << std::endl;
        continue;
      }
      auto next_hop = info->next_hop;
      if ( next_hop == nullopt ) { // 若记录中没有下一跳，则说明下一眺为目的地址
        next_hop = Address::from_ipv4_numeric( dgram->header.dst );
      }
      // std::cout << "send a dgram dst=" << dgram->header.dst << " to interface " << info->interface_num <<
      dgram->header.compute_checksum(); // 必须重新更新校验和，当时卡了一个多小时
      interfaces_[info->interface_num].send_datagram( dgram.value(), next_hop.value() );
    }
  }
}

std::optional<SubNetInfo> Router::find_best_match( uint32_t dst_addr )
{
  std::optional<SubNetInfo> res = nullopt;
  uint8_t maxLen = 0;
  // 前缀匹配函数
  auto is_equal = []( uint32_t ip1, uint32_t ip2, uint8_t len ) {
    if ( len == 0 ) // len为0会导致移32位，会出错，此时直接返回即可
      return true;
    uint32_t offset = 0xFFFFFFFF << ( 32 - len );
    return ( ip1 & offset ) == ( ip2 & offset );
  };
  for ( auto it = route_map_.begin(); it != route_map_.end(); ++it ) {
    if ( it->first.prefix_length < maxLen )//小于当前长度的直接跳过
      continue;
    if ( is_equal( dst_addr, it->first.route_prefix, it->first.prefix_length ) ) {
      maxLen = it->first.prefix_length;
      res = it->second;
    }
  }
  return res;
}
