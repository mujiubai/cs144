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

  // 参考：https://www.cnblogs.com/kangyupl/p/stanford_cs144_labs.html
  // 计算当前值到checkpoint对应的wrap32值的距离，
  // 还没搞懂为啥反着走ret会小于0，后面待调试
  // 这两个值相减，如果最高位是0，那么此值一定更接近checkpoint，是到checkpoint的最近步数
  // 但如果最高位是1，那么应该加上uint32_len（此时min_step为负数）此值才是到checkpoint的最小步数；而如果
  int32_t min_step = raw_value_ - wrap( checkpoint, zero_point ).raw_value_;
  // 将步数加到checkpoint上
  // int32_t转uint64时会将左边第一位复制到其左边第1~32位上。
  // 上面两值相减，如果最高位为0，此时会正常相加；而如果最高位为1，转换时在其高32为都会为1，那么ret最高位必定为1，也就必为负数(不一定为1，还是没弄明白咋回事)
  int64_t ret = checkpoint + min_step;

  // 如果反着走的话要加2^32
  // 如果为ret为负，则说明在高位多加了32位1，再加2^32则可以通过溢出来减去
  return ret >= 0 ? static_cast<uint64_t>( ret ) : ret + ( 1ul << 32 );
}
