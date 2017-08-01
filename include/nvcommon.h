/*
 * Copyright (c) 2012 NVIDIA Corporation.
 * All rights reserved.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef SDK_NVCOMMON_H
#define SDK_NVCOMMON_H

#include <stddef.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C"
{
#endif

typedef unsigned char      NvU8;  // 0 to 255
typedef unsigned short     NvU16; // 0 to 65535
typedef unsigned int       NvU32; // 0 to 4294967295
typedef signed char        NvS8;  // -128 to 127
typedef signed short       NvS16; // -32768 to 32767
typedef signed int         NvS32; // -2147483648 to 2147483647

// Explicitly sized floats
typedef float              NvF32; // IEEE Single Precision (S1E8M23)

typedef double __attribute__ ((aligned(8))) NvF64; // IEEE Double Precision (S1E11M52)
typedef signed long long __attribute__ ((aligned(8)))   NvS64; // 2^-63 to 2^63-1
typedef unsigned long long __attribute__ ((aligned(8))) NvU64; // 0 to 18446744073709551615

// Min/Max values for NvF32
#define NV_MIN_F32  (1.1754944e-38f)
#define NV_MAX_F32  (3.4028234e+38f)

// Boolean type
enum { NV_FALSE = 0, NV_TRUE = 1 };
typedef NvU8 NvBool;

typedef NvU32 NvUPtr;
typedef NvS32 NvSPtr;

#define NV_INLINE __inline__
#define NV_FORCE_INLINE __attribute__((always_inline)) __inline__

#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_NVCOMMON_H
