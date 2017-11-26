/*
 * C code taken from libcrypto++ 5.6.0 sha.cpp (C++) implementation,
 * which is in the public domain.  Added a patch from the author to
 * fix a non-SSE2 bug; see
 * http://allmydata.org/trac/pycryptopp/ticket/24 for details.  Our
 * test program should be able to trigger that bug, if you can find a
 * non-SSE2 program to run it on.
 */
/*
 * This rather ugly "coding style" is intended to keep as close
 * alignment as possible with the upstream C++ version for now
 * (including indentation in most of it), while ripping out C++
 * dependencies and dependencies on other classes in libcrypto++.
 */

#include "sha256.h"
#undef NDEBUG                   /* just in case */
#define NDEBUG                  /* let's go fast! */
#include "numeric.h"
#include "stringUtils.h"
#include "typeDefs.h"

#ifdef __KERNEL__
#define assert(X)
#else
#include <assert.h>
#endif

#if BYTE_ORDER == BIG_ENDIAN
/*
 * It's probably going to be okay, but there's the possibility of some
 * accidentally unconditional byte-swap when it should be conditional,
 * etc.  Test it before removing this.
 */
# error "This code has not been tested on big-endian systems yet!"
#endif

typedef unsigned int word32;
typedef unsigned long long word64;

#define SHA256_BLOCK_SIZE      (4*16)
#define SHA256_STATE_SIZE      (4*16)
#define SHA256_DIGEST_LENGTH   SHA256_HASH_LEN
#define BLOCKSIZE              SHA256_BLOCK_SIZE
#define X86_SHA256_HashBlocks  pbit_X86_SHA256_HashBlocks

/* snarfed from config.h */

#define CRYPTOPP_GCC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)

#if defined(_M_X64) || defined(__x86_64__)
	#define CRYPTOPP_BOOL_X64 1
#else
	#define CRYPTOPP_BOOL_X64 0
#endif

// see http://predef.sourceforge.net/prearch.html
#if defined(_M_IX86) || defined(__i386__) || defined(__i386) || defined(_X86_) || defined(__I86__) || defined(__INTEL__)
	#define CRYPTOPP_BOOL_X86 1
#else
	#define CRYPTOPP_BOOL_X86 0
#endif

#ifndef CRYPTOPP_ALIGN_DATA
	#if defined(CRYPTOPP_MSVC6PP_OR_LATER)
		#define CRYPTOPP_ALIGN_DATA(x) __declspec(align(x))
	#elif defined(__GNUC__)
		#define CRYPTOPP_ALIGN_DATA(x) __attribute__((aligned(x)))
	#else
		#define CRYPTOPP_ALIGN_DATA(x)
	#endif
#endif

#ifndef CRYPTOPP_SECTION_ALIGN16
	#if defined(__GNUC__) && !defined(__APPLE__)
		// the alignment attribute doesn't seem to work without this section attribute when -fdata-sections is turned on
		#define CRYPTOPP_SECTION_ALIGN16 __attribute__((section ("CryptoPP_Align16")))
	#else
		#define CRYPTOPP_SECTION_ALIGN16
	#endif
#endif

/* ... */

#ifdef CRYPTOPP_DISABLE_X86ASM		// for backwards compatibility: this macro had both meanings
#define CRYPTOPP_DISABLE_ASM
#define CRYPTOPP_DISABLE_SSE2
#endif

#if !defined(CRYPTOPP_DISABLE_ASM) && ((defined(_MSC_VER) && defined(_M_IX86)) || (defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))))
	#define CRYPTOPP_X86_ASM_AVAILABLE

	#if !defined(CRYPTOPP_DISABLE_SSE2) && (defined(CRYPTOPP_MSVC6PP_OR_LATER) || CRYPTOPP_GCC_VERSION >= 30300)
		#define CRYPTOPP_BOOL_SSE2_ASM_AVAILABLE 1
	#else
		#define CRYPTOPP_BOOL_SSE2_ASM_AVAILABLE 0
	#endif

	// SSSE3 was actually introduced in GNU as 2.17, which was released 6/23/2006, but we can't tell what version of binutils is installed.
	// GCC 4.1.2 was released on 2/13/2007, so we'll use that as a proxy for the binutils version.
	#if !defined(CRYPTOPP_DISABLE_SSSE3) && ((defined(_MSC_VER) && _MSC_VER >= 1400) || CRYPTOPP_GCC_VERSION >= 40102)
		#define CRYPTOPP_BOOL_SSSE3_ASM_AVAILABLE 1
	#else
		#define CRYPTOPP_BOOL_SSSE3_ASM_AVAILABLE 0
	#endif
#endif

#if !defined(CRYPTOPP_DISABLE_ASM) && defined(_MSC_VER) && defined(_M_X64)
	#define CRYPTOPP_X64_MASM_AVAILABLE
#endif

#if !defined(CRYPTOPP_DISABLE_ASM) && defined(__GNUC__) && defined(__x86_64__)
	#define CRYPTOPP_X64_ASM_AVAILABLE
#endif

#ifndef CRYPTOPP_L1_CACHE_LINE_SIZE
	// This should be a lower bound on the L1 cache line size. It's used for defense against timing attacks.
	#if defined(_M_X64) || defined(__x86_64__)
		#define CRYPTOPP_L1_CACHE_LINE_SIZE 64
	#else
		// L1 cache line size is 32 on Pentium III and earlier
		#define CRYPTOPP_L1_CACHE_LINE_SIZE 32
	#endif
#endif

/* ... */

/* misc.h */

#define GETBYTE(x, y) (unsigned int)(unsigned char)((x)>>(8*(y)))

/* cpu.h */

#if defined(CRYPTOPP_X86_ASM_AVAILABLE) || (_MSC_VER >= 1400 && CRYPTOPP_BOOL_X64)

#define CRYPTOPP_CPUID_AVAILABLE
#endif

/* cpu.cpp */

#if !CRYPTOPP_BOOL_X64

typedef void (*SigHandler)(int);

static jmp_buf s_jmpNoCPUID;
static void SigIllHandlerCPUID(int x)
{
	longjmp(s_jmpNoCPUID, 1 | x); /* use 'x' to suppress warning */
}

static bool CpuId(word32 input, word32 *output)
{
	SigHandler oldHandler = signal(SIGILL, SigIllHandlerCPUID);
	if (oldHandler == SIG_ERR)
		return false;

	bool result = true;
	if (setjmp(s_jmpNoCPUID))
		result = false;
	else
	{
		__asm__
		(
			// save ebx in case -fPIC is being used
#if CRYPTOPP_BOOL_X86
			"push %%ebx; cpuid; mov %%ebx, %%edi; pop %%ebx"
#else
			"pushq %%rbx; cpuid; mov %%ebx, %%edi; popq %%rbx"
#endif
			: "=a" (output[0]), "=D" (output[1]), "=c" (output[2]), "=d" (output[3])
			: "a" (input)
		);
	}

	signal(SIGILL, oldHandler);
	return result;
}

#ifdef CRYPTOPP_CPUID_AVAILABLE

static jmp_buf s_jmpNoSSE2;
static void SigIllHandlerSSE2(int x)
{
	longjmp(s_jmpNoSSE2, 1 | x); /* use 'x' to suppress warning */
}

static bool TrySSE2(void)
{
#if CRYPTOPP_BOOL_X64
	return true;
#elif defined(__GNUC__)
	SigHandler oldHandler = signal(SIGILL, SigIllHandlerSSE2);
	if (oldHandler == SIG_ERR)
		return false;

	bool result = true;
	if (setjmp(s_jmpNoSSE2))
		result = false;
	else
	{
#if CRYPTOPP_BOOL_SSE2_ASM_AVAILABLE
		__asm __volatile ("por %xmm0, %xmm0");
#elif CRYPTOPP_BOOL_SSE2_INTRINSICS_AVAILABLE
		__mm128i x = _mm_setzero_si128();
		result = _mm_cvtsi128_si32(x) == 0;
#endif
	}

	signal(SIGILL, oldHandler);
	return result;
#else
	return false;
#endif
}

static __thread bool g_x86DetectionDone = false;
static __thread bool g_hasSSE2          = false;
// HACK! See below.
//bool g_hasISSE = false, g_hasSSE2 = false, g_hasSSSE3 = false, g_hasMMX = false, g_isP4 = false;
//word32 g_cacheLineSize = CRYPTOPP_L1_CACHE_LINE_SIZE;

static void DetectX86Features(void)
{
	word32 cpuid[4], cpuid1[4];
	if (!CpuId(0, cpuid))
		return;
	if (!CpuId(1, cpuid1))
		return;

        /* Hack!  Make local variables for these so the compiler can
           throw them away, while we keep most of the body of the
           original function in case we want to use these and/or
           update from upstream someday.  */
        bool g_hasISSE = false, g_hasSSSE3 = false, g_hasMMX = false, g_isP4 = false;
        word32 g_cacheLineSize = CRYPTOPP_L1_CACHE_LINE_SIZE;

	g_hasMMX = (cpuid1[3] & (1 << 23)) != 0;
	if ((cpuid1[3] & (1 << 26)) != 0)
		g_hasSSE2 = TrySSE2();
	g_hasSSSE3 = g_hasSSE2 && (cpuid1[2] & (1<<9));

	if ((cpuid1[3] & (1 << 25)) != 0)
		g_hasISSE = true;
	else
	{
		word32 cpuid2[4];
		CpuId(0x080000000, cpuid2);
		if (cpuid2[0] >= 0x080000001)
		{
			CpuId(0x080000001, cpuid2);
			g_hasISSE = (cpuid2[3] & (1 << 22)) != 0;
		}
	}

//	std::swap(cpuid[2], cpuid[3]);
        {
          word32 temp = cpuid[2];
          cpuid[2] = cpuid[3];
          cpuid[3] = temp;
        }

	if (memcmp(cpuid+1, "GenuineIntel", 12) == 0)
	{
		g_isP4 = ((cpuid1[0] >> 8) & 0xf) == 0xf;
		g_cacheLineSize = 8 * GETBYTE(cpuid1[1], 1);
	}
	else if (memcmp(cpuid+1, "AuthenticAMD", 12) == 0)
	{
		CpuId(0x80000005, cpuid);
		g_cacheLineSize = GETBYTE(cpuid[2], 0);
	}

	if (!g_cacheLineSize)
		g_cacheLineSize = CRYPTOPP_L1_CACHE_LINE_SIZE;

	g_x86DetectionDone = true;
}

#endif /* CRYPTOPP_CPUID_AVAILABLE */
#endif /* ! CRYPTOPP_BOOL_X64 */

#if CRYPTOPP_BOOL_X86 && defined(FAKE_NO_SSE2)
/* Fudge things to test the non-SSE2 asm code path, without actually
   requiring non-SSE2 hardware, and blow up if we accidentally still
   follow the SSE2 code path; 32-bit mode only, as 64-bit always has
   the support.  */
# define g_hasSSE2       0
# define movdqa          ud2 /* illegal insn */; movdqa /* eat movdqa ops */
#endif

static inline bool
HasSSE2(void)
{
#if CRYPTOPP_BOOL_X64
	return 1;
#elif defined CRYPTOPP_CPUID_AVAILABLE
	if (!g_x86DetectionDone)
		DetectX86Features();
	return g_hasSSE2;
#else
        return 0;
#endif
}

/* stripped down from misc.h templates */
static inline word32 rotlFixed(word32 x, unsigned int y)
{
	assert(y < sizeof(word32)*8);
	return (word32)((x<<y) | (x>>(sizeof(word32)*8-y)));
}
static inline word32 rotrFixed(word32 x, unsigned int y)
{
	assert(y < sizeof(word32)*8);
	return (word32)((x>>y) | (x<<(sizeof(word32)*8-y)));
}

/* Used for ByteReverse (only used on x86) and in fix_byte_order
   (conditional on endianness).  */
static inline word32
byte_swap(word32 value)
{
  /* snarfed from libcrypto++; byteswap.h bswap_32() might do as well */
#if defined __GNUC__ && defined __i386__
  __asm__ ("bswap %0" : "=r" (value) : "0" (value));
  return value;
#else
#if 1 /* fast rotate? */
	// 5 instructions with rotate instruction, 9 without
	return (rotrFixed(value, 8U) & 0xff00ff00) | (rotlFixed(value, 8U) & 0x00ff00ff);
#else
	// 6 instructions with rotate instruction, 8 without
	value = ((value & 0xFF00FF00) >> 8) | ((value & 0x00FF00FF) << 8);
	return rotlFixed(value, 16U);
#endif
#endif
}

/* only used for x86 optimizations */
static inline void
ByteReverse(word32 *out, const word32 *in, size_t len)
{
  size_t count = len/4;
  size_t i;
  for (i = 0; i < count; i++)
    out[i] = byte_swap(in[i]);
}

/* There are minor calling-sequence speedups possible on x86, relative
   to the standard calling convention, but the compiler can already do
   at least some of them for static functions, like passing arguments
   in registers.  Don't bother for now.  */
#define CRYPTOPP_FASTCALL

/* start of snarfed code */

/* cpu.h */

	// define these in two steps to allow arguments to be expanded
	#define GNU_AS1(x) #x ";"
	#define GNU_AS2(x, y) #x ", " #y ";"
	#define GNU_AS3(x, y, z) #x ", " #y ", " #z ";"
	#define GNU_ASL(x) "\n" #x ":"
	#define GNU_ASJ(x, y, z) #x " " #y #z ";"
	#define GNU_AS_LINE(l) " .loc 1 " #l " 0; "
	#define AS_LINE(l) GNU_AS_LINE(l)
	#define AS1(x) AS_LINE(__LINE__) GNU_AS1(x)
	#define AS2(x, y) AS_LINE(__LINE__) GNU_AS2(x, y)
	#define AS3(x, y, z) AS_LINE(__LINE__) GNU_AS3(x, y, z)
	#define ASS(x, y, a, b, c, d) #x ", " #y ", " #a "*64+" #b "*16+" #c "*4+" #d ";"
	#define ASL(x) GNU_ASL(x)
	#define ASJ(x, y, z) AS_LINE(__LINE__) GNU_ASJ(x, y, z)

#if CRYPTOPP_BOOL_X86
	#define AS_REG_1 ecx
	#define AS_REG_2 edx
	#define AS_REG_3 esi
	#define AS_REG_4 edi
	#define AS_REG_5 eax
	#define AS_REG_6 ebx
	#define AS_REG_7 ebp
	#define AS_REG_1d ecx
	#define AS_REG_2d edx
	#define AS_REG_3d esi
	#define AS_REG_4d edi
	#define AS_REG_5d eax
	#define AS_REG_6d ebx
	#define AS_REG_7d ebp
	#define WORD_SZ 4
	#define WORD_REG(x)	e##x
	#define WORD_PTR DWORD PTR
	#define AS_PUSH_IF86(x) AS1(push e##x)
	#define AS_POP_IF86(x) AS1(pop e##x)
	#define AS_JCXZ jecxz
#elif CRYPTOPP_BOOL_X64
	#ifdef CRYPTOPP_GENERATE_X64_MASM
		#define AS_REG_1 rcx
		#define AS_REG_2 rdx
		#define AS_REG_3 r8
		#define AS_REG_4 r9
		#define AS_REG_5 rax
		#define AS_REG_6 r10
		#define AS_REG_7 r11
		#define AS_REG_1d ecx
		#define AS_REG_2d edx
		#define AS_REG_3d r8d
		#define AS_REG_4d r9d
		#define AS_REG_5d eax
		#define AS_REG_6d r10d
		#define AS_REG_7d r11d
	#else
		#define AS_REG_1 rdi
		#define AS_REG_2 rsi
		#define AS_REG_3 rdx
		#define AS_REG_4 rcx
		#define AS_REG_5 r8
		#define AS_REG_6 r9
		#define AS_REG_7 r10
		#define AS_REG_1d edi
		#define AS_REG_2d esi
		#define AS_REG_3d edx
		#define AS_REG_4d ecx
		#define AS_REG_5d r8d
		#define AS_REG_6d r9d
		#define AS_REG_7d r10d
	#endif
	#define WORD_SZ 8
	#define WORD_REG(x)	r##x
	#define WORD_PTR QWORD PTR
	#define AS_PUSH_IF86(x)
	#define AS_POP_IF86(x)
	#define AS_JCXZ jrcxz
#endif

// GNU assembler doesn't seem to have mod operator
#define ASM_MOD(x, y) ((x)-((x)/(y))*(y))
// GAS 2.15 doesn't support XMMWORD PTR. it seems necessary only for MASM
#define XMMWORD_PTR

/* ... */

/* snarfed from sha.cpp, and tweaked to be C99 code */

#if CRYPTOPP_BOOL_SSE2_ASM_AVAILABLE
CRYPTOPP_ALIGN_DATA(16) static const word32 SHA256_K[64] CRYPTOPP_SECTION_ALIGN16 = {
#else
static const word32 SHA256_K[64] = {
#endif
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
	0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
	0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
	0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
	0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
	0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
	0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
	0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
	0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};
// ...

#if defined(CRYPTOPP_X86_ASM_AVAILABLE) || defined(CRYPTOPP_GENERATE_X64_MASM)

//MS #pragma warning(disable: 4731)	// frame pointer register 'ebp' modified by inline assembly code

#ifdef __GNUC__
/* The assembly code used here appears to scribble on other state if
   we try to expand it inline.  Investigate some other time.  */
static void CRYPTOPP_FASTCALL
X86_SHA256_HashBlocks(word32 *state, const word32 *data, size_t len)
    __attribute__((__noinline__));
#endif

static void CRYPTOPP_FASTCALL X86_SHA256_HashBlocks(word32 *state, const word32 *data, size_t len
#if defined(_MSC_VER) && (_MSC_VER == 1200)
	, ...	// VC60 workaround: prevent VC 6 from inlining this function
#endif
	)
{
#if defined(_MSC_VER) && (_MSC_VER == 1200)
	AS2(mov ecx, [state])
	AS2(mov edx, [data])
#endif

	#define LOCALS_SIZE	8*4 + 16*4 + 4*WORD_SZ
	#define H(i)		[BASE+ASM_MOD(1024+7-(i),8)*4]
	#define G(i)		H(i+1)
	#define F(i)		H(i+2)
	#define E(i)		H(i+3)
	#define D(i)		H(i+4)
	#define C(i)		H(i+5)
	#define B(i)		H(i+6)
	#define A(i)		H(i+7)
	#define Wt(i)		BASE+8*4+ASM_MOD(1024+15-(i),16)*4
	#define Wt_2(i)		Wt((i)-2)
	#define Wt_15(i)	Wt((i)-15)
	#define Wt_7(i)		Wt((i)-7)
	#define K_END		[BASE+8*4+16*4+0*WORD_SZ]
	#define STATE_SAVE	[BASE+8*4+16*4+1*WORD_SZ]
	#define DATA_SAVE	[BASE+8*4+16*4+2*WORD_SZ]
	#define DATA_END	[BASE+8*4+16*4+3*WORD_SZ]
	#define Kt(i)		WORD_REG(si)+(i)*4
#if CRYPTOPP_BOOL_X86
	#define BASE		esp+4
#elif defined(__GNUC__)
	#define BASE		r8
#else
	#define BASE		rsp
#endif

#define RA0(i, edx, edi)		\
	AS2(	add edx, [Kt(i)]	)\
	AS2(	add edx, [Wt(i)]	)\
	AS2(	add edx, H(i)		)\

#define RA1(i, edx, edi)

#define RB0(i, edx, edi)

#define RB1(i, edx, edi)	\
	AS2(	mov AS_REG_7d, [Wt_2(i)]	)\
	AS2(	mov edi, [Wt_15(i)])\
	AS2(	mov ebx, AS_REG_7d	)\
	AS2(	shr AS_REG_7d, 10		)\
	AS2(	ror ebx, 17		)\
	AS2(	xor AS_REG_7d, ebx	)\
	AS2(	ror ebx, 2		)\
	AS2(	xor ebx, AS_REG_7d	)/* s1(W_t-2) */\
	AS2(	add ebx, [Wt_7(i)])\
	AS2(	mov AS_REG_7d, edi	)\
	AS2(	shr AS_REG_7d, 3		)\
	AS2(	ror edi, 7		)\
	AS2(	add ebx, [Wt(i)])/* s1(W_t-2) + W_t-7 + W_t-16 */\
	AS2(	xor AS_REG_7d, edi	)\
	AS2(	add edx, [Kt(i)])\
	AS2(	ror edi, 11		)\
	AS2(	add edx, H(i)	)\
	AS2(	xor AS_REG_7d, edi	)/* s0(W_t-15) */\
	AS2(	add AS_REG_7d, ebx	)/* W_t = s1(W_t-2) + W_t-7 + s0(W_t-15) W_t-16*/\
	AS2(	mov [Wt(i)], AS_REG_7d)\
	AS2(	add edx, AS_REG_7d	)\

#define ROUND(i, r, eax, ecx, edi, edx)\
	/* in: edi = E	*/\
	/* unused: eax, ecx, temp: ebx, AS_REG_7d, out: edx = T1 */\
	AS2(	mov edx, F(i)	)\
	AS2(	xor edx, G(i)	)\
	AS2(	and edx, edi	)\
	AS2(	xor edx, G(i)	)/* Ch(E,F,G) = (G^(E&(F^G))) */\
	AS2(	mov AS_REG_7d, edi	)\
	AS2(	ror edi, 6		)\
	AS2(	ror AS_REG_7d, 25		)\
	RA##r(i, edx, edi		)/* H + Wt + Kt + Ch(E,F,G) */\
	AS2(	xor AS_REG_7d, edi	)\
	AS2(	ror edi, 5		)\
	AS2(	xor AS_REG_7d, edi	)/* S1(E) */\
	AS2(	add edx, AS_REG_7d	)/* T1 = S1(E) + Ch(E,F,G) + H + Wt + Kt */\
	RB##r(i, edx, edi		)/* H + Wt + Kt + Ch(E,F,G) */\
	/* in: ecx = A, eax = B^C, edx = T1 */\
	/* unused: edx, temp: ebx, AS_REG_7d, out: eax = A, ecx = B^C, edx = E */\
	AS2(	mov ebx, ecx	)\
	AS2(	xor ecx, B(i)	)/* A^B */\
	AS2(	and eax, ecx	)\
	AS2(	xor eax, B(i)	)/* Maj(A,B,C) = B^((A^B)&(B^C) */\
	AS2(	mov AS_REG_7d, ebx	)\
	AS2(	ror ebx, 2		)\
	AS2(	add eax, edx	)/* T1 + Maj(A,B,C) */\
	AS2(	add edx, D(i)	)\
	AS2(	mov D(i), edx	)\
	AS2(	ror AS_REG_7d, 22		)\
	AS2(	xor AS_REG_7d, ebx	)\
	AS2(	ror ebx, 11		)\
	AS2(	xor AS_REG_7d, ebx	)\
	AS2(	add eax, AS_REG_7d	)/* T1 + S0(A) + Maj(A,B,C) */\
	AS2(	mov H(i), eax	)\

#define SWAP_COPY(i)		\
	AS2(	mov		WORD_REG(bx), [WORD_REG(dx)+i*WORD_SZ])\
	AS1(	bswap	WORD_REG(bx))\
	AS2(	mov		[Wt(i*(1+CRYPTOPP_BOOL_X64)+CRYPTOPP_BOOL_X64)], WORD_REG(bx))

#if defined(__GNUC__)
	#if CRYPTOPP_BOOL_X64
//		FixedSizeAlignedSecBlock<byte, LOCALS_SIZE> workspace;
          CRYPTOPP_ALIGN_DATA(16) unsigned char workspace[LOCALS_SIZE];
	#endif
	__asm__ __volatile__
	(
	#if CRYPTOPP_BOOL_X64
		"lea %4, %%r8;"
	#endif
	".intel_syntax noprefix;"
#elif defined(CRYPTOPP_GENERATE_X64_MASM)
		ALIGN   8
	X86_SHA256_HashBlocks	PROC FRAME
		rex_push_reg rsi
		push_reg rdi
		push_reg rbx
		push_reg rbp
		alloc_stack(LOCALS_SIZE+8)
		.endprolog
		mov rdi, r8
		lea rsi, [?SHA256_K@CryptoPP@@3QBIB + 48*4]
#endif

#if CRYPTOPP_BOOL_X86
	#ifndef __GNUC__
		AS2(	mov		edi, [len])
		AS2(	lea		WORD_REG(si), [SHA256_K+48*4])
	#endif
	#if !defined(_MSC_VER) || (_MSC_VER < 1400)
		AS_PUSH_IF86(bx)
	#endif

	AS_PUSH_IF86(bp)
	AS2(	mov		ebx, esp)
	AS2(	and		esp, -16)
	AS2(	sub		WORD_REG(sp), LOCALS_SIZE)
	AS_PUSH_IF86(bx)
#endif
	AS2(	mov		STATE_SAVE, WORD_REG(cx))
	AS2(	mov		DATA_SAVE, WORD_REG(dx))
	AS2(	lea		WORD_REG(ax), [WORD_REG(di) + WORD_REG(dx)])
	AS2(	mov		DATA_END, WORD_REG(ax))
	AS2(	mov		K_END, WORD_REG(si))

#if CRYPTOPP_BOOL_SSE2_ASM_AVAILABLE
#if CRYPTOPP_BOOL_X86
	AS2(	test	edi, 1)
	ASJ(	jnz,	2, f)
	AS1(	dec		DWORD PTR K_END)
#endif
	AS2(	movdqa	xmm0, XMMWORD_PTR [WORD_REG(cx)+0*16])
	AS2(	movdqa	xmm1, XMMWORD_PTR [WORD_REG(cx)+1*16])
#endif

#if CRYPTOPP_BOOL_X86
#if CRYPTOPP_BOOL_SSE2_ASM_AVAILABLE
	ASJ(	jmp,	0, f)
#endif
	ASL(2)	// non-SSE2
	AS2(	mov		esi, ecx)
	AS2(	lea		edi, A(0))
	AS2(	mov		ecx, 8)
	AS1(	rep movsd)
	AS2(	mov		esi, K_END)
	ASJ(	jmp,	3, f)
#endif

#if CRYPTOPP_BOOL_SSE2_ASM_AVAILABLE
	ASL(0)
	AS2(	movdqa	E(0), xmm1)
	AS2(	movdqa	A(0), xmm0)
#endif
#if CRYPTOPP_BOOL_X86
	ASL(3)
#endif
	AS2(	sub		WORD_REG(si), 48*4)
	SWAP_COPY(0)	SWAP_COPY(1)	SWAP_COPY(2)	SWAP_COPY(3)
	SWAP_COPY(4)	SWAP_COPY(5)	SWAP_COPY(6)	SWAP_COPY(7)
#if CRYPTOPP_BOOL_X86
	SWAP_COPY(8)	SWAP_COPY(9)	SWAP_COPY(10)	SWAP_COPY(11)
	SWAP_COPY(12)	SWAP_COPY(13)	SWAP_COPY(14)	SWAP_COPY(15)
#endif
	AS2(	mov		edi, E(0))	// E
	AS2(	mov		eax, B(0))	// B
	AS2(	xor		eax, C(0))	// B^C
	AS2(	mov		ecx, A(0))	// A

	ROUND(0, 0, eax, ecx, edi, edx)
	ROUND(1, 0, ecx, eax, edx, edi)
	ROUND(2, 0, eax, ecx, edi, edx)
	ROUND(3, 0, ecx, eax, edx, edi)
	ROUND(4, 0, eax, ecx, edi, edx)
	ROUND(5, 0, ecx, eax, edx, edi)
	ROUND(6, 0, eax, ecx, edi, edx)
	ROUND(7, 0, ecx, eax, edx, edi)
	ROUND(8, 0, eax, ecx, edi, edx)
	ROUND(9, 0, ecx, eax, edx, edi)
	ROUND(10, 0, eax, ecx, edi, edx)
	ROUND(11, 0, ecx, eax, edx, edi)
	ROUND(12, 0, eax, ecx, edi, edx)
	ROUND(13, 0, ecx, eax, edx, edi)
	ROUND(14, 0, eax, ecx, edi, edx)
	ROUND(15, 0, ecx, eax, edx, edi)

	ASL(1)
	AS2(add WORD_REG(si), 4*16)
	ROUND(0, 1, eax, ecx, edi, edx)
	ROUND(1, 1, ecx, eax, edx, edi)
	ROUND(2, 1, eax, ecx, edi, edx)
	ROUND(3, 1, ecx, eax, edx, edi)
	ROUND(4, 1, eax, ecx, edi, edx)
	ROUND(5, 1, ecx, eax, edx, edi)
	ROUND(6, 1, eax, ecx, edi, edx)
	ROUND(7, 1, ecx, eax, edx, edi)
	ROUND(8, 1, eax, ecx, edi, edx)
	ROUND(9, 1, ecx, eax, edx, edi)
	ROUND(10, 1, eax, ecx, edi, edx)
	ROUND(11, 1, ecx, eax, edx, edi)
	ROUND(12, 1, eax, ecx, edi, edx)
	ROUND(13, 1, ecx, eax, edx, edi)
	ROUND(14, 1, eax, ecx, edi, edx)
	ROUND(15, 1, ecx, eax, edx, edi)
	AS2(	cmp		WORD_REG(si), K_END)
	ASJ(	jb,		1, b)

	AS2(	mov		WORD_REG(dx), DATA_SAVE)
	AS2(	add		WORD_REG(dx), 64)
	AS2(	mov		AS_REG_7, STATE_SAVE)
	AS2(	mov		DATA_SAVE, WORD_REG(dx))

#if CRYPTOPP_BOOL_SSE2_ASM_AVAILABLE
#if CRYPTOPP_BOOL_X86
	AS2(	test	DWORD PTR K_END, 1)
	ASJ(	jz,		4, f)
#endif
	AS2(	movdqa	xmm1, XMMWORD_PTR [AS_REG_7+1*16])
	AS2(	movdqa	xmm0, XMMWORD_PTR [AS_REG_7+0*16])
	AS2(	paddd	xmm1, E(0))
	AS2(	paddd	xmm0, A(0))
	AS2(	movdqa	[AS_REG_7+1*16], xmm1)
	AS2(	movdqa	[AS_REG_7+0*16], xmm0)
	AS2(	cmp		WORD_REG(dx), DATA_END)
	ASJ(	jb,		0, b)
#endif

#if CRYPTOPP_BOOL_X86
#if CRYPTOPP_BOOL_SSE2_ASM_AVAILABLE
	ASJ(	jmp,	5, f)
	ASL(4)	// non-SSE2
#endif
	AS2(	add		[AS_REG_7+0*4], ecx)	// A
	AS2(	add		[AS_REG_7+4*4], edi)	// E
	AS2(	mov		eax, B(0))
	AS2(	mov		ebx, C(0))
	AS2(	mov		ecx, D(0))
	AS2(	add		[AS_REG_7+1*4], eax)
	AS2(	add		[AS_REG_7+2*4], ebx)
	AS2(	add		[AS_REG_7+3*4], ecx)
	AS2(	mov		eax, F(0))
	AS2(	mov		ebx, G(0))
	AS2(	mov		ecx, H(0))
	AS2(	add		[AS_REG_7+5*4], eax)
	AS2(	add		[AS_REG_7+6*4], ebx)
	AS2(	add		[AS_REG_7+7*4], ecx)
	AS2(	mov		ecx, AS_REG_7d)
	AS2(	cmp		WORD_REG(dx), DATA_END)
	ASJ(	jb,		2, b)
#if CRYPTOPP_BOOL_SSE2_ASM_AVAILABLE
	ASL(5)
#endif
#endif

	AS_POP_IF86(sp)
	AS_POP_IF86(bp)
	#if !defined(_MSC_VER) || (_MSC_VER < 1400)
		AS_POP_IF86(bx)
	#endif

#ifdef CRYPTOPP_GENERATE_X64_MASM
	add		rsp, LOCALS_SIZE+8
	pop		rbp
	pop		rbx
	pop		rdi
	pop		rsi
	ret
	X86_SHA256_HashBlocks ENDP
#endif

#ifdef __GNUC__
	".att_syntax prefix;"
	:
	: "c" (state), "d" (data), "S" (SHA256_K+48), "D" (len)
	#if CRYPTOPP_BOOL_X64
		, "m" (workspace[0])
	#endif
	: "memory", "cc", "%eax"
	#if CRYPTOPP_BOOL_X64
		, "%rbx", "%r8", "%r10"
	#endif
	);
#endif
}

#endif	// #if defined(CRYPTOPP_X86_ASM_AVAILABLE) || defined(CRYPTOPP_GENERATE_X64_MASM)

/* ... */

#define blk2(i) (W[i&15]+=s1(W[(i-2)&15])+W[(i-7)&15]+s0(W[(i-15)&15]))

#define Ch(x,y,z) (z^(x&(y^z)))
#define Maj(x,y,z) (y^((x^y)&(y^z)))

#define a(i) T[(0-i)&7]
#define b(i) T[(1-i)&7]
#define c(i) T[(2-i)&7]
#define d(i) T[(3-i)&7]
#define e(i) T[(4-i)&7]
#define f(i) T[(5-i)&7]
#define g(i) T[(6-i)&7]
#define h(i) T[(7-i)&7]

#define R(i) h(i)+=S1(e(i))+Ch(e(i),f(i),g(i))+SHA256_K[i+j]+(j?blk2(i):blk0(i));\
	d(i)+=h(i);h(i)+=S0(a(i))+Maj(a(i),b(i),c(i))

// for SHA256
#define S0(x) (rotrFixed(x,2)^rotrFixed(x,13)^rotrFixed(x,22))
#define S1(x) (rotrFixed(x,6)^rotrFixed(x,11)^rotrFixed(x,25))
#define s0(x) (rotrFixed(x,7)^rotrFixed(x,18)^(x>>3))
#define s1(x) (rotrFixed(x,17)^rotrFixed(x,19)^(x>>10))

static void SHA256Transform(word32 *state, const word32 *data);
static void HashMultipleBlocks(word32 *state, const word32 *input, size_t len);

//void SHA256::Transform(word32 *state, const word32 *data)
static void SHA256Transform(word32 *state, const word32 *data)
{
	CRYPTOPP_ALIGN_DATA(16) word32 W[16];
#if defined(CRYPTOPP_X86_ASM_AVAILABLE) || defined(CRYPTOPP_X64_MASM_AVAILABLE)
	// this byte reverse is a waste of time, but this function is only called by MDC
	ByteReverse(W, data, BLOCKSIZE);
	X86_SHA256_HashBlocks(state, W, BLOCKSIZE - !HasSSE2());
#else
	word32 T[8];
    /* Copy context->state[] to working vars */
	memcpy(T, state, sizeof(T));
    /* 64 operations, partially loop unrolled */
	for (unsigned int j=0; j<64; j+=16)
	{
		R( 0); R( 1); R( 2); R( 3);
		R( 4); R( 5); R( 6); R( 7);
		R( 8); R( 9); R(10); R(11);
		R(12); R(13); R(14); R(15);
	}
    /* Add the working vars back into context.state[] */
    state[0] += a(0);
    state[1] += b(0);
    state[2] += c(0);
    state[3] += d(0);
    state[4] += e(0);
    state[5] += f(0);
    state[6] += g(0);
    state[7] += h(0);
#endif
}

/* end of snarfed code */

static void HashMultipleBlocks(word32 *state, const word32 *input, size_t len)
{
#if defined(CRYPTOPP_X86_ASM_AVAILABLE) || defined(CRYPTOPP_X64_MASM_AVAILABLE)
	X86_SHA256_HashBlocks(state, input, len - !HasSSE2());
#else
#error "not supported"
#endif
}

static inline void
fix_byte_order(word32 *ptr, int count)
{
#if BYTE_ORDER == LITTLE_ENDIAN
  int i;
  for (i = 0; i < count; i++)
    ptr[i] = byte_swap(ptr[i]);
#elif BYTE_ORDER == BIG_ENDIAN
  /* nothing to do */
#else
# error "byte order determination failed!"
#endif
}

//  SHA256().CalculateDigest(ret_hash, reinterpret_cast<const byte*>(data), len);

void
sha256(const void *data, size_t len, unsigned char ret_hash[SHA256_HASH_LEN])
{
  CRYPTOPP_ALIGN_DATA(16) word32 state[SHA256_STATE_SIZE/4];
  word64 bit_len = (word64)len * 8;
  const char *bytePtr = (const char *) data;
  word32 temp[SHA256_BLOCK_SIZE/4];

  // SHA256::InitState(HashWordType *state)
  {
    static const word32 s[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    memcpy(state, s, sizeof(s));
  }
  // foreach block { fetch bits; fix byte order; transform; }
  // Newer implementation has transform-multiple-blocks as one call.
  size_t nBlocks = len / sizeof(temp);
  if (nBlocks > 0) {
    size_t mbLen = nBlocks * sizeof(temp);
    HashMultipleBlocks(state, (const word32 *) bytePtr, mbLen);
    bytePtr += mbLen;
    len -= mbLen;
    assert (len < sizeof(temp));
  }
  // Final - the libcrypto++ version is hairy, so rewritten from spec.
  /*
   * Pad last block (0x80 then zeros as needed), add length as 64-bit
   * big-endian value, transform, copy out state as digest.
   */
  if (len + 9 > sizeof(temp)) {
    /*
     * Too big to add padding plus length in one block, must split.
     * First, tail plus 0x80, pad with zeros.
     */
    memcpy(temp, bytePtr, len);
    ((unsigned char *)temp)[len] = 0x80;
    if (len < sizeof(temp)-1)
      memset((unsigned char *)temp + len + 1, 0, sizeof(temp) - len - 1);
    fix_byte_order(temp, (len+4)/4);
    SHA256Transform(state, temp);

    /* Then, more zeros followed by the length (below).  */
    memset(temp, 0, sizeof(temp) - 8);
  } else {
    /* It'll all fit together.  */
    memcpy(temp, bytePtr, len);
    ((unsigned char *)temp)[len] = 0x80;
    if (len < sizeof(temp)-9)
      memset((unsigned char *)temp + len + 1, 0, sizeof(temp) - len - 9);
    fix_byte_order(temp, sizeof(temp)/4);
  }
  /*
   * Fill in the length here, at the end of the last block.  This will
   * be in host order, as sha256_transform needs, so no byte swapping
   * here.  Also, if the rest of the block is zero, it would just be a
   * waste of cycles.  So byte swapping on N-2 words is done above if
   * needed.
   */
  temp[(SHA256_BLOCK_SIZE/4)-2] = bit_len >> 32;
  temp[(SHA256_BLOCK_SIZE/4)-1] = bit_len & 0xFFFFFFFF;
  SHA256Transform(state, temp);

  /*
   * Result is calculated in integers, but the output is defined to be
   * those values in big-endian order.
   */
  fix_byte_order(state, SHA256_DIGEST_LENGTH/4);
  memcpy(ret_hash, state, SHA256_DIGEST_LENGTH);
}
