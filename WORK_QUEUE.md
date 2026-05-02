# simdtext Work Queue

## Current Run: Optimization Sprint #1
Started: 2026-05-02 08:06 UTC

## Priority Queue

### Phase 1: C++23 Modernization & Compile Times
- [ ] Replace raw pointers with `std::span` / `std::string_view` consistently
- [ ] Add `constexpr` / `consteval` where possible
- [ ] Use `std::expected` instead of custom error handling
- [ ] Replace `#pragma once` with include guards? (check convention)
- [ ] Use `[[likely]]` / `[[unlikely]]` on hot path branches
- [ ] Replace manual dispatch with `if constexpr` where applicable
- [ ] Audit `#include` chains for compile time reduction
- [ ] Forward-declare instead of include where possible
- [ ] Use explicit `std::span` overloads to avoid string_view → string copies

### Phase 2: Performance (from audit reports)
- [ ] SIMD UTF-8 validation (pshufb/tbl lookup4 algorithm, 5-15× potential)
- [ ] SIMD Base64 encode/decode (pshufb reshuffle, 10-20× potential)
- [ ] SIMD URL encode/decode (safe-byte classification, 5-10× potential)
- [ ] Highway backend unrolling (1→4 vectors, 20-40% improvement)
- [ ] AVX-512 runtime detection fix (already done ✅)
- [ ] Missing sfence after non-temporal stores (already done ✅)

### Phase 3: Code Quality (from audit reports)
- [ ] Deduplicate UTF-8 validation into shared header
- [ ] Deduplicate hex_decode_table (encode.cpp vs url.cpp)
- [ ] Deduplicate popcount/ctz helpers (3 files)
- [ ] Fix noexcept on functions calling vector::push_back
- [ ] Move hex_val from url.cpp to encode.hpp
- [ ] Remove unused expected.hpp polyfill
- [ ] Fix incomplete JSON \uXXXX unescape

### Phase 4: Testing & Hardening
- [ ] Add fuzz tests for UTF-8, Base64, URL parsers
- [ ] Add count_code_points test for 0x80 continuation byte
- [ ] Add swar_range_mask boundary tests
- [ ] Add parallel_valid_utf8 multi-byte boundary tests
- [ ] Run with ASan + UBSan

## Completed
- ✅ swar_range_mask inter-byte borrow fix
- ✅ count_code_points 0x80 fix (SSE2 + AVX2)
- ✅ parallel_valid_utf8 boundary fix
- ✅ SSE2 -mssse3 SIGILL fix
- ✅ Missing sfence after non-temporal stores
- ✅ AVX-512 runtime detection (not compile-time)
- ✅ NEON find_byte positional fix
- ✅ Version sync 0.2.0
- ✅ Dead code removal (SSE2 validate_utf8)
- ✅ parallel_find_byte CAS acquire/release
- ✅ Highway linkage PUBLIC→PRIVATE
