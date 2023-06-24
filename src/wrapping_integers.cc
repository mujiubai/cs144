#include "wrapping_integers.hh"
#include <cmath>
#include <iostream>

using namespace std;

static const uint64_t UINT32_LEN = static_cast<uint64_t>( UINT32_MAX ) + 1;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  // Your code here.
  // return Wrap32 { zero_point + static_cast<uint32_t>( n % UINT32_LEN ) };
  return Wrap32 { static_cast<uint32_t>( n ) + zero_point.raw_value_ }; // 直接利用溢出来计算
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  // 计算当前值到checkpoint对应的wrap32值的距离，
  // 参考：https://www.cnblogs.com/kangyupl/p/stanford_cs144_labs.html
  // 还没搞懂为啥反着走ret会小于0，后面待调试
  int32_t min_step = raw_value_ - wrap( checkpoint, zero_point ).raw_value_;
  // 将步数加到checkpoint上
  int64_t ret = checkpoint + min_step;

  // 如果反着走的话要加2^32
  return ret >= 0 ? static_cast<uint64_t>( ret ) : ret + ( 1ul << 32 );
}
