#pragma  once

struct MD5_CTX {
	ULONG ib[2];				/* number of _bits_ handled mod 2^64 */
	ULONG sbuf[4];			/* scratch buffer */
	UCHAR in[64];			/* input buffer */
	union {					/* actual digest after MD5Final call */
		UCHAR digest[16];           
		USHORT us_digest[8];
		ULONG ul_digest[4];
		ULONG64 u64_digest[2];
		UUID ui_digest;
	};
} ;

EXTERN_C_START

NTDLL_V MD5Init(MD5_CTX *);
NTDLL_V MD5Update(MD5_CTX *, const void *, unsigned int);
NTDLL_V MD5Final(MD5_CTX *);

EXTERN_C_END

#define MD5_HASH_LEN 16
