#include "adler.h"

//memset the adlerhold to zero to begin

//This algorithm is inefficient apparently. Will change to the zlib one at some stage.
unsigned long int ChecksumAdler32B(ADLER_STRUCTURE *adlerhold, unsigned char *data, size_t len)
{
	unsigned long int a;
	unsigned long int b;

	a=adlerhold->a;
	b=adlerhold->b;

    while (len != 0)
    {
        a = (a + *data++) % MOD_ADLER;
        b = (b + a) % MOD_ADLER;

        len--;
    }

	adlerhold->a=a;
	adlerhold->b=b;

    return (b << 16) | a;
}



#define NMAX 5552
/* NMAX is the largest n such that 255n(n+1)/2 + (n+1)(BASE-1) <= 2^32-1 */

#define DO1(buf,i)  {a += buf[i]; b += a;}
#define DO2(buf,i)  DO1(buf,i); DO1(buf,i+1);
#define DO4(buf,i)  DO2(buf,i); DO2(buf,i+2);
#define DO8(buf,i)  DO4(buf,i); DO4(buf,i+4);
#define DO16(buf)   DO8(buf,0); DO8(buf,8);

// This should be more efficient
unsigned long int ChecksumAdler32(ADLER_STRUCTURE *adlerhold, unsigned char *data, size_t len)
{
	unsigned long int a;
	unsigned long int b;

	int k;

	a=adlerhold->a;
	b=adlerhold->b;

    while (len > 0) {
        k = len < NMAX ? len : NMAX;
        len -= k;
        while (k >= 16) {
            DO16(data);
            data += 16;
            k -= 16;
        }
        if (k != 0) do {
            a += *data++;
            b += a;
        } while (--k);
        a %= MOD_ADLER;
        b %= MOD_ADLER;
    }

	adlerhold->a=a;
	adlerhold->b=b;

	return (b << 16) | a;
}
