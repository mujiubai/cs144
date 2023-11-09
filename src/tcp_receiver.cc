#include "tcp_receiver.hh"
#include <iostream>
using namespace std;

TCPReceiver::TCPReceiver() : zero_point_( 0 ), check_point_( UINT64_MAX ) {}

//处理接受到的消息，包括更新序列号等，注意该函数并不处理ack消息（由TCPSender的receive函数处理）
void TCPReceiver::receive( TCPSenderMessage message, Reassembler& reassembler, Writer& inbound_stream )
{
  if ( message.SYN ) {
    // 从未接受过数据则应该设置zero_point和check_point_
    if ( check_point_ == UINT64_MAX ) {
      zero_point_ = message.seqno;
      check_point_ = 0;
    }
    // 如果SYN为true，且数据不为空，此时seqno指的是SYN序号，为了后续处理数据，将seqno加1
    if ( message.payload.size() > 0 ) {
      message.seqno = message.seqno + 1;
    }
  }
  // first_index是处于Absolute Sequence Numbers
  uint64_t first_index = message.seqno.unwrap( zero_point_, check_point_ );
  // 插入时应该是 Stream Indices，因此需要减1
  reassembler.insert( first_index - 1, message.payload, message.FIN, inbound_stream );
  // 如果已经经过syn，且当前数据不空或存在FIN标志，那么应该更新check_point_
  // 注意，为了方便，当检测到需要close时，在reassembler中会将unassem_index_+1，此时FIN所占一哥序号就存在了。虽然逻辑上对unassem_index_的含义不优美
  if ( ( message.payload.size() > 0 || message.FIN ) && check_point_ != UINT64_MAX )
    check_point_ = reassembler.get_unassem_index();
}

//发送ack消息
TCPReceiverMessage TCPReceiver::send( const Writer& inbound_stream ) const
{
  // std::cout << "check_point_=" << check_point_ << " win=" << inbound_stream.available_capacity() << std::endl;
  uint64_t win_size = inbound_stream.available_capacity();
  if ( win_size > UINT16_MAX ) {
    win_size = UINT16_MAX;
  }
  if ( check_point_ == UINT64_MAX ) {
    return { nullopt, static_cast<uint16_t>( win_size ) };
  }
  return { Wrap32::wrap( check_point_ + 1, zero_point_ ), static_cast<uint16_t>( win_size ) };
}
