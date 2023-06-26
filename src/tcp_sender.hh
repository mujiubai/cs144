#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"
#include <deque>
#include <queue>

class TCPSender
{
  Wrap32 isn_;              // 初始序列号
  uint64_t initial_RTO_ms_; // 初始超时时间
  uint64_t unack_idx_;      // 尚未收到确认的第一个stream index，注意这不是absolute sequence number
  uint64_t next_send_idx_;  // 下一个待发送的stream index
  uint16_t win_size_;       // 真实的发送窗口大小，最小为1，初始为1
  uint16_t
    recv_win_size_; // 从接收方接受到的发送窗口大小，最小为0，初始为1，主要用于RTO是否翻倍判断:大于0，则不翻倍，否则翻倍
  std::deque<TCPSenderMessage> messages_out_;         // 等待发送的报文段
  std::deque<TCPSenderMessage> messages_outstanding_; // 已经发送但等待确认的报文
  size_t timer_;                                      // 当前定时器时间
  bool timer_runing_;                                 // 当前定时器是否在运行
  uint64_t outstanding_count_; // 当前待确认的sequence number数量，注意不是stream index，因此包括SYN和FIN
  bool is_SYN_;                // 是否成功发送SYN初始报文
  bool is_SYN_sending_; // SYN是否正在发送中，此时会占据一个窗口大小
  bool is_FIN_;         // 是否成功发送FIN报文
  uint16_t retrans_times_; 

public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( uint64_t initial_RTO_ms, std::optional<Wrap32> fixed_isn );

  /* Push bytes from the outbound stream */
  void push( Reader& outbound_stream );

  /* Send a TCPSenderMessage if needed (or empty optional otherwise) */
  std::optional<TCPSenderMessage> maybe_send();

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage send_empty_message() const;

  /* Receive an act on a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called. */
  void tick( uint64_t ms_since_last_tick );

  /* Accessors for use in testing */
  uint64_t sequence_numbers_in_flight() const; // How many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions()
    const; // How many consecutive *re*transmissions have happened? 指当前连续重传次数
};
