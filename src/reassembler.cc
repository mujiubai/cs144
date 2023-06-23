#include "reassembler.hh"
#include <iostream>

using namespace std;

Reassembler::Reassembler() : unassem_index_( 0 ), unaccept_index_( 0 ), buffers_(), map_(), is_last_flag_( false )
{}

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring, Writer& output )
{
  is_last_flag_ = is_last_substring || is_last_flag_;
  if ( data.size() == 0 ) {
    if ( map_.empty() && is_last_flag_ ) // 不存在未读取数据且关闭标志
      output.close();
    return;
  }

  uint64_t avail_push_size = output.available_capacity();
  // 更改buffer空间，一般来说只会在第一次修改
  if ( buffers_.size() < avail_push_size )
    buffers_.resize( avail_push_size, '0' );
  unaccept_index_ = unassem_index_ + output.available_capacity();
  // 如果data已经不在接收窗口中：即大于等于不可接受指针或小于重整指针
  if ( first_index >= unaccept_index_ || first_index + data.size() - 1 < unassem_index_ )
    return;
  // 使用insert_start_index指针记录应该从data哪个位置进行插入，避免first_index小于unassem_index_情况
  uint64_t insert_start_index = first_index >= unassem_index_ ? first_index : unassem_index_;
  // 插入的字符串不能超过不可接受指针
  uint64_t insert_size
    = min( data.size() - ( insert_start_index - first_index ), unaccept_index_ - insert_start_index );
  std::string insert_str = data.substr( insert_start_index - first_index, insert_size );
  buffers_.replace( insert_start_index - unassem_index_, insert_size, insert_str );

  uint64_t insert_end_index = insert_start_index + insert_size - 1;

  // 向前和向后遍历，从而将已存在的的数据流合并
  map_[insert_start_index] = insert_end_index;
  auto cur_iter = map_.find( insert_start_index );
  auto next_iter = cur_iter;
  ++next_iter;
  // 向后合并
  while ( next_iter != map_.end() ) {
    if ( cur_iter->second >= next_iter->first - 1 ) {
      cur_iter->second = max( next_iter->second, cur_iter->second );
      map_.erase( next_iter );
      next_iter = cur_iter;
      ++next_iter;
    } else {
      break;
    }
  }
  // 向前合并
  auto pre_iter = cur_iter;
  --pre_iter;
  while ( cur_iter != map_.begin() ) {
    if ( pre_iter->second >= cur_iter->first - 1 ) {
      pre_iter->second = max( cur_iter->second, pre_iter->second );
      map_.erase( cur_iter );
      cur_iter = pre_iter;
      --pre_iter;
    } else {
      break;
    }
  }

  //如果合并后cur_iter的first处于待规整处，则说明此段可以被push进writer中。
  if ( cur_iter->first == unassem_index_ ) {
    // 取插入数据和可插入容量最小值
    unassem_index_ = min( cur_iter->second + 1, unassem_index_ + avail_push_size );
    std::string push_str = buffers_.substr( 0, unassem_index_ - cur_iter->first );
    output.push( push_str );
    buffers_.erase( 0, unassem_index_ - cur_iter->first );
    // 如果cur_iter只被插入了部分，那么需要在map中记录剩余部分
    if ( unassem_index_ <= cur_iter->second ) {
      map_[unassem_index_] = cur_iter->second;
    }
    map_.erase( cur_iter );
  }

  //只有map_不存在待规整数据且关闭标志为true时才关闭
  if ( map_.empty() && is_last_flag_ )
    output.close();
}

uint64_t Reassembler::bytes_pending() const
{
  // Your code here.
  uint64_t count = 0;
  auto cur_iter = map_.begin();
  while ( cur_iter != map_.end() ) {
    count += cur_iter->second - cur_iter->first + 1;
    ++cur_iter;
  }
  return { count };
}
