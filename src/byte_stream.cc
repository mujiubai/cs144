#include <stdexcept>

#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ),buffer_(),close_(false),err_(false),totalPushBytes_(0),totalPopBytes_(0) {}

void Writer::push( string data )
{
  // Your code here.
  (void)data;
  int pushSize=min(data.size(),capacity_-buffer_.size());
  for(int i=0;i<pushSize;++i){
    buffer_.push_back(data[i]);
  }
  totalPushBytes_+=pushSize;
}

void Writer::close()
{
  // Your code here.
  close_=true;
}

void Writer::set_error()
{
  // Your code here.
  err_=true;
}

bool Writer::is_closed() const
{
  // Your code here.
  return close_;
}

uint64_t Writer::available_capacity() const
{
  // Your code here.
  return capacity_-buffer_.size();
}

uint64_t Writer::bytes_pushed() const
{
  // Your code here.
  return totalPushBytes_;
}

string_view Reader::peek() const
{
  // Your code here.
  return {std::string_view(&buffer_.front(),1)};
}

bool Reader::is_finished() const
{
  // Your code here.
  return close_&&buffer_.empty();
}

bool Reader::has_error() const
{
  // Your code here.
  return err_;
}

void Reader::pop( uint64_t len )
{
  // Your code here.
  (void)len;
  if(buffer_.size()<len) len=buffer_.size();
  for(uint64_t i=0;i<len;++i){
    buffer_.pop_front();
  }
  totalPopBytes_+=len;
}

uint64_t Reader::bytes_buffered() const
{
  // Your code here.
  return {buffer_.size()};
}

uint64_t Reader::bytes_popped() const
{
  // Your code here.
  return {totalPopBytes_};
}
