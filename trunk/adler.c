#include "adler.h"

//memset the adlerhold to zero to begin
unsigned long int ChecksumAdler32(ADLER_STRUCTURE *adlerhold, unsigned char *data, size_t len)
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
