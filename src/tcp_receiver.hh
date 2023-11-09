#pragma once

#include "reassembler.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"
#include "wrapping_integers.hh"

/**
 * @brief 功能：1.接受消息数据   2.发送ack消息
 * 
 */
class TCPReceiver
{
private:
  Wrap32 zero_point_;    // 初始sequence number
  uint64_t check_point_; // 下一个待发送的stream index，初始为UINT64_MAX代表SYN未收到过
public:
  /*
   * The TCPReceiver receives TCPSenderMessages, inserting their payload into the Reassembler
   * at the correct stream index.
   */
  void receive( TCPSenderMessage message, Reassembler& reassembler, Writer& inbound_stream );

  /* The TCPReceiver sends TCPReceiverMessages back to the TCPSender. */
  TCPReceiverMessage send( const Writer& inbound_stream ) const;

  TCPReceiver();
};
