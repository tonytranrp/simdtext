# Disassembly Analysis — simdtext

**Build flags:** `-O3 -march=native -fno-omit-frame-pointer`
**Target:** x86-64 with AVX-512 (machine supports AVX-512BW)

---

## 1. `simdtext::detail::scalar::count_byte`

### Assembly findings
- **SWAR main loop** (0x60-0x8a): 7 instructions/iteration, processes 8 bytes. Uses `xor + lea + andn + shr + and + popcnt + add`. ✅ Good.
- **Auto-vectorized path** (0x1e0-0x280): Compiler generated AVX-512 code (zmm registers, vpxorq, vpopcntq) for large buffers. This runs in the "scalar" function — defeats the purpose of a scalar fallback.
- **Small tail** (0x1c0-0x1cf): Byte-by-byte. ✅ Acceptable for <8 bytes.

### Issues
- **CRITICAL**: The scalar fallback uses AVX-512, causing:
  - ZMM register save/restore overhead (~100 cycle penalty on mix with SSE/AVX2)
  - Frequency downclocking on some CPUs
  - No actual scalar fallback when AVX-512 is unavailable

### Recommendation
- Add `__attribute__((target("no-avx512f,no-avx512bw")))` to prevent AVX-512 in scalar functions
- OR add `#pragma GCC target("no-avx512f")` around the scalar implementation

---

## 2. `simdtext::detail::sse2::count_byte`

### Assembly findings
- **4x unrolled main loop** (0x30-0x93): pcmpeqb + pmovmskb + popcnt pattern ✅. 64 bytes/iteration. ~20 instructions/iteration.
- **16-byte single loop** (0xb3-0xd1): pcmpeqb + pmovmskb + popcnt ✅.
- **Mid-range tail (16-64 bytes)** (0x110-0x1b0): **TERRIBLE** — uses pmovsxbw → pmovsxwd → pmovsxdq chain with psubq accumulation instead of pcmpeqb+pmovmskb+popcnt. This is ~25 instructions per 16-byte chunk vs ~5 with popcnt. The compiler does this because it can't prove the remaining count is safe for the simple path.

### Issues
- **HIGH**: Mid-range tail (remaining >14 bytes but <64) generates extremely inefficient code — sign-extension chain instead of simple popcnt

### Recommendation
- Restructure loop to always use the 16-byte pcmpeqb+pmovmskb+popcnt path for any remaining ≥16 bytes
- The simple loop at 0xb3 is correct and efficient; the compiler is generating the bad path for the mid-range tail

---

## 3. `simdtext::detail::avx2::count_byte`

### Assembly findings
- **4x unrolled main loop** (0x30-0x88): vpcmpeqb + vpmovmskb + popcnt ✅. 128 bytes/iteration. ~20 instructions/iteration.
- **2x pair loop** (0xa3-0xd3): vpcmpeqb + vpmovmskb + popcnt ✅. 64 bytes/iteration.
- **Single 32-byte loop** (0xf3-0x10f): ✅.
- **Tail SSE2** (0x11a-0x12d): pcmpeqb + pmovmskb + popcnt ✅.
- **AVX-512 fallback** (0x153-0x218): For tail > 64 bytes, uses AVX-512 vpcmpeqb with k-mask operations. Same issue as scalar — unnecessary AVX-512 in AVX2 code.

### Issues
- **MEDIUM**: AVX-512 code generated for AVX2 function's tail. Same frequency/downclocking concern.

### Recommendation
- Add `__attribute__((target("no-avx512f")))` to AVX2 functions
- OR use `#pragma GCC target` to constrain

---

## 4. `simdtext::detail::scalar::is_ascii`

### Assembly findings
- **SWAR loop** (0x490-0x4a4): Loads 8 bytes, ANDs with 0x8080..., checks nonzero. ✅ Excellent.
- ~5 instructions/iteration. Clean and efficient.
- **Byte tail** (0x4c8-0x4cb): Checks sign bit. ✅.

### Issues
- None. This is well-optimized.

---

## 5. `simdtext::detail::sse2::find_byte`

### Assembly findings
- **Main loop** (0xad0-0xaec): movdqu + pcmpeqb + pmovmskb + test + jne. ✅.
- **Match found**: Uses `tzcnt` (0xb20) ✅ — not bsf. tzcnt is defined for zero input.
- **Byte tail** (0xb08-0xb0e): Simple cmp + jne loop. ✅.

### Issues
- None. Already optimal.

---

## 6. `simdtext::base64_encode_to`

### Assembly findings
- **Main loop** (0x53d0-0x544f): Loads 3 bytes, shifts/or into 24-bit value, 4 table lookups, writes 4 bytes as single DWORD. ✅ Excellent.
- ~25 instructions per 3-byte group. Compiler merged 4 byte stores into one 32-bit write.
- **Padding path** (0x5455-0x54f1): Handles 1-2 remaining bytes with = padding. ✅.

### Issues
- None. Well-optimized by the compiler.

---

## 7. `simdtext::url_decode_to`

### Assembly findings
- **Main loop**: Uses hex_decode_table for branchless hex lookup ✅.
- Per '%': load 2 hex chars → table lookup → branch on negative → combine. ~10 instructions for the happy path.
- Per '+': single branch + write. ✅.
- Per normal char: single write. ✅.

### Issues
- The hex_decode_table is duplicated in url.cpp and encode.cpp. Minor — linker may deduplicate constexpr arrays.
- No SIMD acceleration for the common case (no percent encoding). Could be improved but out of scope.

---

## Overall Patterns

### What's Good
- SSE2/AVX2 count_byte uses proper pcmpeqb+pmovmskb+popcnt pattern
- 4x loop unrolling in SIMD paths
- SWAR is_ascii is clean and efficient  
- sse2::find_byte uses tzcnt correctly
- base64_encode compiler-optimized to merged 32-bit stores
- url_decode uses table-based hex decode

### What Needs Fixing
1. **AVX-512 leakage into scalar/AVX2 functions** — causes frequency downclocking and defeats fallback purpose
2. **SSE2 count_byte mid-range tail** — compiler generates inefficient pmovsx chain instead of simple popcnt path
3. **No `__attribute__((target))` constraints** — functions should be explicitly constrained to their intended ISA level

### Priority Fixes
1. Add `__attribute__((target("no-avx512f,no-avx512bw,no-avx512vl")))` to scalar and AVX2 source files
2. Restructure SSE2 count_byte tail to use simple 16-byte loop
3. Add `#pragma GCC optimize("no-tree-vectorize")` to scalar file as belt-and-suspenders
