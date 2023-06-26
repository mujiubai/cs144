#include "tcp_sender.hh"
#include "tcp_config.hh"

#include <iostream>
#include <random>

using namespace std;

/* TCPSender constructor (uses a random ISN if none given) */
TCPSender::TCPSender( uint64_t initial_RTO_ms, optional<Wrap32> fixed_isn )
  : isn_( fixed_isn.value_or( Wrap32 { random_device()() } ) )
  , initial_RTO_ms_( initial_RTO_ms )
  , unack_idx_( 0 )
  , next_send_idx_( 0 )
  , win_size_( 1 )
  , recv_win_size_( 1 )
  , messages_out_()
  , messages_outstanding_()
  , timer_( 0 )
  , timer_runing_( false )
  , outstanding_count_( 0 )
  , is_SYN_( false )
  , is_SYN_sending_( false )
  , is_FIN_( false )
  , retrans_times_( 0 )
{}

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return { outstanding_count_ };
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  return { retrans_times_ };
}

optional<TCPSenderMessage> TCPSender::maybe_send()
{
  // std::cout << "maybe_send runing---" << std::endl;
  if ( messages_out_.empty() )
    return {};
  TCPSenderMessage msg = messages_out_.front();
  messages_out_.pop_front();
  // 如果待插入的序列号比front序列小，则插入在首部：是为了重传时，将重传报文放在首部，以便一个报文多次重传只需从首部弹出即可
  if ( msg.seqno.unwrap( isn_, unack_idx_ ) < messages_outstanding_.front().seqno.unwrap( isn_, unack_idx_ ) ) {
    messages_outstanding_.push_front( msg );
  } else {
    messages_outstanding_.push_back( msg );
  }

  // 定时器未开启，则开始定时器
  if ( !timer_runing_ ) {
    timer_runing_ = true;
    timer_ = 0;
  }
  return msg;
}

void TCPSender::push( Reader& outbound_stream )
{
  bool is_finish = outbound_stream.is_finished();
  // 从没发送给SYN报文
  if ( !is_SYN_ ) {
    messages_out_.push_back( { Wrap32::wrap( 0ul, isn_ ), true, {}, is_finish } );
    is_SYN_ = true;
    is_SYN_sending_ = true;
    ++outstanding_count_;
    if ( is_finish ) {
      ++outstanding_count_;
      is_FIN_ = true;
    }
  }
  // 不断从outbound_stream中获取数据
  uint64_t ava_win_size = next_send_idx_ - unack_idx_ - is_SYN_sending_;
  while ( ava_win_size < win_size_ ) {
    // len应为最大数据量、可用发送窗口大小、待发送数据中的最小值
    uint16_t len = min(
      TCPConfig::MAX_PAYLOAD_SIZE,
      min( win_size_ - ( next_send_idx_ - unack_idx_ - is_SYN_sending_ ),
           outbound_stream.bytes_buffered() ) ); // 这里不能简单替换为ava_win_size，可能由于类型转换等原因，会出错
    if ( len == 0 )
      break;

    std::string data;
    data.reserve( len );
    for ( uint16_t i = 0; i < len; ++i ) {
      data.push_back( *( outbound_stream.peek().begin() ) );
      outbound_stream.pop( 1 );
    }
    is_finish = outbound_stream.is_finished();
    if ( len >= win_size_ ) { // 如果超过当前窗口大小限制则不设置FIN
      is_finish = false;
    }
    messages_out_.push_back( { Wrap32::wrap( next_send_idx_ + 1, isn_ ), false, data, is_finish } );
    outstanding_count_ += len;
    next_send_idx_ += len;
    if ( is_finish ) {
      ++outstanding_count_;
      is_FIN_ = true;
    }
    // std::cout << "push data:" << data << std::endl;
  }
  ava_win_size = next_send_idx_ - unack_idx_ - is_SYN_sending_;
  if ( outbound_stream.is_finished() && is_FIN_ == false && ava_win_size < win_size_ ) {
    messages_out_.push_back( { Wrap32::wrap( next_send_idx_ + 1, isn_ ), false, {}, true } );
    is_FIN_ = true;
    ++outstanding_count_;
  }
}

TCPSenderMessage TCPSender::send_empty_message() const
{
  // 当通道关闭后，如果发送空消息，应该多加一个序号
  return { Wrap32::wrap( next_send_idx_ + 1 + is_FIN_, isn_ ), false, {}, false };
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  recv_win_size_ = msg.window_size;
  win_size_ = max( msg.window_size, static_cast<uint16_t>( 1 ) );
  if ( msg.ackno == nullopt )
    return;
  uint64_t ackno = msg.ackno->unwrap( isn_, unack_idx_ ); // unack_idx_-1?? ackno是绝对序列号
  // ackno小于当前未收到ack位置或ackno大于当前待发送序号（此时ackno无效）,ackno为绝对序列号，而unack_idx_和next_send_idx_为流序列号
  if ( ackno < unack_idx_ + is_SYN_ + is_FIN_ || ackno > next_send_idx_ + is_SYN_ + is_FIN_ )
    return;
  while ( !messages_outstanding_.empty() ) {
    const auto& cur_msg = messages_outstanding_.front(); // 注意悬空引用
    auto cur_sqn = cur_msg.seqno.unwrap( isn_, unack_idx_ );
    auto payload_size = cur_msg.payload.size();
    uint64_t cur_end_sqn
      = cur_sqn + payload_size + cur_msg.SYN + cur_msg.FIN - 1; // 当前报文最末尾的绝对序列号，包括FIN和SYN占位

    if ( cur_end_sqn < ackno ) { // 确认号会确认整个报文
      if ( cur_sqn == 0 ) { // 如果接受的是SYN确认，那么将is_SYN_sending_置为false，以后计算可用窗口大小时就无需减1
        is_SYN_sending_ = false;
        outstanding_count_
          -= cur_msg.SYN; // 只有最开始的SYN才会占用一个序列号，如果非开始处，那么即使SYN为true，也不会占用序列哈
      }
      outstanding_count_ -= cur_msg.FIN;

      // // 如果不足以确认整个报文，但文档中不用实现这种半确认，实现了反而会测试不通过
      // std::string payload = static_cast<string>( cur_msg.payload );
      // uint16_t ack_data_size = payload_size;
      // if ( cur_sqn + payload_size + cur_msg.SYN + cur_msg.FIN - 1 >= ackno ) {
      //   ack_data_size = ackno - cur_sqn - cur_msg.SYN;                         // 能够确认的payload大小
      //   TCPSenderMessage new_msg { Wrap32::wrap( cur_sqn + ack_data_size + cur_msg.SYN, isn_ ),
      //                              false, // 能至少确认一个序号，新msg的SYN必为false
      //                              payload.substr( ack_data_size, payload.size() - ack_data_size ),
      //                              true }; // 不能确认完，新msg的FIN必为true
      //   messages_outstanding_.pop_front();
      //   messages_outstanding_.push_front( new_msg );
      // } else {
      // messages_outstanding_.pop_front();
      // }
      messages_outstanding_.pop_front();
      unack_idx_ += payload_size;         // 如果改为确认一部分，那么此处需要改为ack_data_size
      outstanding_count_ -= payload_size; // 如果改为确认一部分，那么此处需要改为ack_data_size
      if ( timer_runing_ ) {
        timer_runing_ = false;
        timer_ = 0;
        retrans_times_ = 0;
      }
    } else {
      break;
    }
  }
}

void TCPSender::tick( const size_t ms_since_last_tick )
{
  // Your code here.
  if ( messages_outstanding_.empty() )
    return;
  if ( !timer_runing_ ) { // timer未运行，且存在未确认报文，当应当开启定时器
    timer_runing_ = true;
    timer_ = 0;
  }
  timer_ += ms_since_last_tick;
  if ( timer_ >= ( initial_RTO_ms_ << retrans_times_ ) ) {
    messages_out_.push_back( messages_outstanding_.front() ); // 超时的报文会插在队头，因此弹出队头即可
    messages_outstanding_.pop_front();
    timer_ = 0; // 超时则重启定时器
    if ( recv_win_size_ > 0 ) // 从接收方收到的发送窗口不为0，注意不是win_size
      ++retrans_times_;
  }
}
