#include <windows.h>

#define MOD_ADLER 65521

/*Because I will be calculating adler-32 for 'parts' of file
  (e.g. in blocks that checksum wraps around, or chunks of
  data that are not all in a file) I need to hold the adler value*/
struct adlerstruct
{
	unsigned long int a;
	unsigned long int b;
};

typedef struct adlerstruct ADLER_STRUCTURE;

unsigned long int ChecksumAdler32(ADLER_STRUCTURE *adlerhold, unsigned char *data, size_t len);
unsigned long int ChecksumAdler32B(ADLER_STRUCTURE *adlerhold, unsigned char *data, size_t len);
