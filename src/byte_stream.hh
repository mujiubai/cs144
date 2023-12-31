#pragma once

#include <deque>
#include <iostream>
#include <queue>
#include <stdexcept>
#include <string>
#include <string_view>

class Reader;
class Writer;

class LoopBuffer
{
private:
  std::string buffer_;
  uint32_t front_index_;
  uint32_t back_index_;
  uint32_t store_nums_;

public:
  void resize( size_t size );
  std::string pop( size_t len );
  bool push( const std::string& data );
  LoopBuffer();
  LoopBuffer( size_t size );
  uint32_t get_store_nums_() const { return store_nums_; }
  std::string_view peek() const
  {
    return { std::string_view( &buffer_[front_index_], 1 ) };
  }
};

class ByteStream
{
protected:
  uint64_t capacity_; // 缓冲区容量限制
  // Please add any additional state to the ByteStream here, and not to the Writer and Reader interfaces.
  // std::deque<char> buffer_; // 缓冲区
  LoopBuffer buffer_;
  bool close_;              // 关闭标志
  bool err_;                // 错误标志
  uint64_t totalPushBytes_; // 总共放入字节数
  uint64_t totalPopBytes_;  // 总共弹出字节数

public:
  explicit ByteStream( uint64_t capacity );

  // Helper functions (provided) to access the ByteStream's Reader and Writer interfaces
  Reader& reader();
  const Reader& reader() const;
  Writer& writer();
  const Writer& writer() const;
};

class Writer : public ByteStream
{
public:
  void push( std::string data ); // Push data to stream, but only as much as available capacity allows.

  void close();     // Signal that the stream has reached its ending. Nothing more will be written.
  void set_error(); // Signal that the stream suffered an error.

  bool is_closed() const;              // Has the stream been closed?
  uint64_t available_capacity() const; // How many bytes can be pushed to the stream right now?
  uint64_t bytes_pushed() const;       // Total number of bytes cumulatively pushed to the stream
};

class Reader : public ByteStream
{
public:
  std::string_view peek() const; // Peek at the next bytes in the buffer
  void pop( uint64_t len );      // Remove `len` bytes from the buffer

  bool is_finished() const; // Is the stream finished (closed and fully popped)?
  bool has_error() const;   // Has the stream had an error?

  uint64_t bytes_buffered() const; // Number of bytes currently buffered (pushed and not popped)
  uint64_t bytes_popped() const;   // Total number of bytes cumulatively popped from stream
};

/*
 * read: A (provided) helper function thats peeks and pops up to `len` bytes
 * from a ByteStream Reader into a string;
 */
void read( Reader& reader, uint64_t len, std::string& out );
