#include <stdexcept>

#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity )
  : capacity_( capacity )
  , buffer_( capacity )
  , close_( false )
  , err_( false )
  , totalPushBytes_( 0 )
  , totalPopBytes_( 0 )
{}

void Writer::push( string data )
{
  // Your code here.
  // (void)data;
  // int pushSize = min( data.size(), capacity_ - buffer_.size() );
  // for ( int i = 0; i < pushSize; ++i ) {
  //   buffer_.push_back( data[i] );
  // }
  size_t pushSize = min( data.size(), capacity_ - buffer_.get_store_nums_() );
  if ( pushSize == data.size() ) {
    buffer_.push( data );
  } else {
    buffer_.push( data.substr( 0, pushSize ) );
  }
  totalPushBytes_ += pushSize;
}

void Writer::close()
{
  // Your code here.
  close_ = true;
}

void Writer::set_error()
{
  // Your code here.
  err_ = true;
}

bool Writer::is_closed() const
{
  // Your code here.
  return close_;
}

uint64_t Writer::available_capacity() const
{
  // Your code here.
  return capacity_ - buffer_.get_store_nums_();
}

uint64_t Writer::bytes_pushed() const
{
  // Your code here.
  return totalPushBytes_;
}

string_view Reader::peek() const
{
  // Your code here.
  return buffer_.peek();
}

bool Reader::is_finished() const
{
  // Your code here.
  return close_ && buffer_.get_store_nums_() == 0;
}

bool Reader::has_error() const
{
  // Your code here.
  return err_;
}

void Reader::pop( uint64_t len )
{
  // Your code here.
  // (void)len;
  // if ( buffer_.size() < len )
  //   len = buffer_.size();
  // for ( uint64_t i = 0; i < len; ++i ) {
  //   buffer_.pop_front();
  // }
  if ( buffer_.get_store_nums_() < len ) {
    len = buffer_.get_store_nums_();
  }
  totalPopBytes_ += len;
  buffer_.pop( len );
}

uint64_t Reader::bytes_buffered() const
{
  // Your code here.
  return { buffer_.get_store_nums_() };
}

uint64_t Reader::bytes_popped() const
{
  // Your code here.
  return { totalPopBytes_ };
}

void LoopBuffer::resize( size_t size )
{
  buffer_.resize( size );
  // 还有问题 需要更新两个指针
}
std::string LoopBuffer::pop( size_t len )
{
  if ( store_nums_ < len )
    len = store_nums_;
  std::string ans;
  ans.reserve( len );
  size_t right_data_len = min( len, buffer_.size() - front_index_ );
  size_t left_data_len = len - right_data_len;
  if ( right_data_len > 0 ) {
    copy( buffer_.begin() + front_index_, buffer_.begin() + front_index_ + right_data_len, ans.end() );
    front_index_ += right_data_len;
  }
  if ( left_data_len > 0 ) {
    copy( buffer_.begin(), buffer_.begin() + left_data_len, ans.end() );
    front_index_ = left_data_len;
  }
  store_nums_ -= len;
  front_index_%=buffer_.size();

  return ans;
}
bool LoopBuffer::push( const std::string& data )
{
  size_t data_size = data.size();
  if ( buffer_.size() - store_nums_ < data_size )
    return false;
  size_t right_data_len = min( buffer_.size() - back_index_, data_size );
  size_t left_data_len = data_size - right_data_len;
  if ( right_data_len > 0 ) {
    copy( data.begin(), data.begin() + right_data_len, buffer_.begin() + back_index_ );
    back_index_ += right_data_len;
  }
  if ( left_data_len > 0 ) {
    copy( data.begin() + right_data_len, data.end(), buffer_.begin() );
    back_index_ = left_data_len;
  }
  store_nums_ += data_size;

  return true;
}
LoopBuffer::LoopBuffer() : buffer_(), front_index_( 0 ), back_index_( 0 ), store_nums_( 0 ) {}
LoopBuffer::LoopBuffer( size_t size ) : buffer_( size, '0' ), front_index_( 0 ), back_index_( 0 ), store_nums_( 0 )
{}