#include "int_types.h"
#include "crush_ln_table.h"
#include "crush_hash.h"
#include "crush.h"
#include <iostream>
using namespace std;

static __u64 crush_ln(unsigned int xin)
{
	unsigned int x = xin;
	int iexpon, index1, index2;
	__u64 RH, LH, LL, xl64, result;
	x++;
	/* normalize input */
	iexpon = 15;
	// figure out number of bits we need to shift and
	// do it in one step instead of iteratively
	if (!(x & 0x18000)) {
	  int bits = __builtin_clz(x & 0x1FFFF) - 16;
	  x <<= bits;
	  iexpon = 15 - bits;
	}
	index1 = (x >> 8) << 1;
	/* RH ~ 2^56/index1 */
	RH = __RH_LH_tbl[index1 - 256];
	/* LH ~ 2^48 * log2(index1/256) */
	LH = __RH_LH_tbl[index1 + 1 - 256];
	/* RH*x ~ 2^48 * (2^15 + xf), xf<2^8 */
	xl64 = (__s64)x * RH;
	xl64 >>= 48;
	result = iexpon;
	result <<= (12 + 32);
	index2 = xl64 & 0xff;
	/* LL ~ 2^48*log2(1.0+index2/2^15) */
	LL = __LL_tbl[index2];
	LH = LH + LL;
	LH >>= (48 - 12 - 32);
	result += LH;
	return result;
}

//生成指数分布的随机数
static inline __s64 generate_exponential_distribution(int type, int x, int y, int z, int weight)
{
	unsigned int u = crush_hash32_3(type, x, y, z);
	u &= 0xffff;
	__s64 ln = crush_ln(u) - 0x1000000000000ll;
	return div64_s64(ln, weight);
}