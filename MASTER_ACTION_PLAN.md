# simdtext Master Action Plan — Updated 2026-05-02

## Phase 1: Critical Bug Fixes ✅ DONE
- [x] Fix NEON find_byte truncation
- [x] Fix SSE2 popcount without popcnt target (SWAR helper)
- [x] Fix UTF-8 streaming validator (overlong, surrogates, >U+10FFFF)
- [x] Add 11 UTF-8 edge case tests
- [x] Fix SWAR range mask inter-byte borrow bug

## Phase 2: SIMD Optimizations ✅ DONE
- [x] ILP-friendly 4× unrolled count_byte_vec
- [x] ILP-friendly 4× unrolled is_ascii_vec (OR accumulation)
- [x] Lookup-table UTF-8 validation (simdutf lookup4 algorithm)
  - check_special_cases() with 3 pshufb lookup tables
  - check_multibyte_lengths() with inter-lane carry
  - is_incomplete() for EOF boundary handling
  - SIMD fast path + scalar correctness fallback
- [x] Base64 SIMD batch encoding (12→16 byte processing)
- [x] SIMD hex encode with pshufb nibble lookup

## Phase 3: Documentation ✅ DONE
- [x] ARCHITECTURE.md with full architecture diagram
- [x] Doxygen @param/@return for scan.hpp, ascii.hpp, encode.hpp, utf8.hpp
- [x] RESEARCH_optimization_v2.md with complete findings

## Phase 4: Research ✅ DONE
- [x] 3 sub-agent audits (bugs, quality, comparison)
- [x] Aggressive internet research (simdutf, aklomp/base64, libhat, simdjson)
- [x] arXiv paper research
- [x] Optimization priority matrix

## Phase 5: Remaining Work

### High Priority
- [ ] Debug UTF-8 lookup tables to match simdutf exactly → remove scalar fallback
- [ ] Implement full SIMD Base64 with 2-step pshufb for AVX2
- [ ] Add Google Benchmark suite and measure actual GB/s numbers
- [ ] Wire hex_encode_simd() into the public API (dispatch from encode.cpp)

### Medium Priority
- [ ] Implement JSON tokenizer SIMD (structural character detection)
- [ ] Implement XML tokenizer SIMD
- [ ] Implement pattern scan SIMD (pshufb multi-offset)
- [ ] Add AVX-512 specific paths (multishift for Base64)
- [ ] Fix compiler warnings (nodiscard in tests)

### Low Priority
- [ ] URL decode SIMD (% detection + hex lookup)
- [ ] CSV parse SIMD (comma/newline detection)
- [ ] Remove dead code (unused headers, expected.hpp)
- [ ] Add noexcept to functions that can't throw
- [ ] Add [[nodiscard]] to all return-value functions
