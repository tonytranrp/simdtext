# Benchmark Results

**Date:** 2026-05-02  
**Platform:** 2× 2400 MHz CPU (VPS)  
**Compiler:** GCC 13.3.0, `-O2 -mavx2 -mavx512bw`  
**Build:** Release, Highway disabled  

## CPU Cache Hierarchy

| Level | Size      | Boundary Buffer Size |
|-------|-----------|---------------------|
| L1D   | 48 KiB    | ~32–64 KiB          |
| L2    | 1024 KiB  | ~512 KiB–1 MiB      |
| L3    | 32768 KiB | ~16–32 MiB          |

## Results Summary

### CountNewlines

| Implementation | 1 KiB | 64 KiB | 1 MiB | 16 MiB | 256 MiB |
|---|---|---|---|---|---|
| **SimdText** | 10.2 ns (93.8 GiB/s) | 553 ns (110.4 GiB/s) | 9.3 µs (104.6 GiB/s) | 164 µs (95.0 GiB/s) | 9.0 ms (27.8 GiB/s) |
| **ScalarLoop** | 102 ns (9.3 GiB/s) | 6.5 µs (9.3 GiB/s) | 104 µs (9.4 GiB/s) | 1.68 ms (9.3 GiB/s) | 27.1 ms (9.2 GiB/s) |
| **STL** | 207 ns (4.6 GiB/s) | 14.9 µs (4.1 GiB/s) | 239 µs (4.1 GiB/s) | 3.82 ms (4.1 GiB/s) | 61.2 ms (4.1 GiB/s) |

**Speedup vs STL:** 20× (L1) → 27× (L2) → 22× (L3) — SimdText dominates across all sizes.

### IsASCII

| Implementation | 1 KiB | 64 KiB | 1 MiB | 16 MiB | 256 MiB |
|---|---|---|---|---|---|
| **SimdText** | 5.7 ns (170.6 GiB/s) | 331 ns (184.4 GiB/s) | 7.1 µs (137.3 GiB/s) | 159 µs (98.5 GiB/s) | 8.8 ms (28.4 GiB/s) |
| **STL** | 143 ns (6.7 GiB/s) | 9.1 µs (6.7 GiB/s) | 166 µs (5.9 GiB/s) | 2.78 ms (5.6 GiB/s) | 45.9 ms (5.4 GiB/s) |

**Speedup vs STL:** 25× (L1) → 27× (L2) → 6.5× (beyond L3) — Outstanding L1/L2 performance; memory-bound beyond L3.

### Lowercase

| Implementation | 1 KiB | 64 KiB | 1 MiB | 16 MiB | 256 MiB |
|---|---|---|---|---|---|
| **SimdText** | 41 ns (23.1 GiB/s) | 1.4 µs (42.1 GiB/s) | 32.2 µs (30.3 GiB/s) | 788 µs (19.8 GiB/s) | 106 ms (2.4 GiB/s) |
| **ScalarBitwise** | 42 ns (22.5 GiB/s) | 1.7 µs (36.6 GiB/s) | 37.8 µs (26.5 GiB/s) | 952 µs (16.5 GiB/s) | 135 ms (1.9 GiB/s) |
| **STL** | 1.2 µs (818 MiB/s) | 75.9 µs (823 MiB/s) | 1.21 ms (825 MiB/s) | 20.1 ms (797 MiB/s) | 412 ms (621 MiB/s) |

**Speedup vs STL:** 29× (L1) → 51× (L2) → 6× (beyond L3). SimdText and scalar-bitwise are close — the win is over `std::tolower` locale overhead, not SIMD per se.

### Lines (Split)

| Implementation | 1 KiB | 64 KiB | 1 MiB | 16 MiB |
|---|---|---|---|---|
| **SimdText** | 99 ns (9.7 GiB/s) | 6.3 µs (9.7 GiB/s) | 225 µs (4.3 GiB/s) | 4.41 ms (3.5 GiB/s) |
| **Getline** | 334 ns (2.9 GiB/s) | 13.0 µs (4.7 GiB/s) | 497 µs (2.0 GiB/s) | 9.10 ms (1.7 GiB/s) |

**Speedup vs Getline:** 3× (L1) → 2.5× (L3). Lines is allocation-heavy (string per line), limiting SIMD gains.

### HexEncode

| Implementation | 1 KiB | 64 KiB | 1 MiB | 16 MiB |
|---|---|---|---|---|
| **SimdText** | 870 ns (1.1 GiB/s) | 68.2 µs (916 MiB/s) | 1.09 ms (921 MiB/s) | 28.3 ms (566 MiB/s) |
| **ScalarSprintf** | 20.9 µs (47 MiB/s) | 1.33 ms (47 MiB/s) | 22.5 ms (44 MiB/s) | 409 ms (39 MiB/s) |

**Speedup vs Scalar:** 24× (L1) → 14× (beyond L3). `sprintf` is catastrophically slow; SimdText's LUT approach is far better.

### Base64Encode

| Implementation | 1 KiB | 64 KiB | 1 MiB | 16 MiB |
|---|---|---|---|---|
| **SimdText** | 535 ns (1.8 GiB/s) | 28.4 µs (2.2 GiB/s) | 713 µs (1.4 GiB/s) | 11.4 ms (1.4 GiB/s) |
| **Scalar** | 1.6 µs (611 MiB/s) | 99.7 µs (627 MiB/s) | 1.58 ms (634 MiB/s) | 25.2 ms (635 MiB/s) |

**Speedup vs Scalar:** 2.9× (L1) → 2.2× (L2+). Moderate gain; Base64's complex shuffle limits SIMD advantage.

### URLDecode

| Implementation | 1 KiB | 64 KiB | 1 MiB | 16 MiB |
|---|---|---|---|---|
| **SimdText** | 2.1 µs (467 MiB/s) | 129 µs (484 MiB/s) | 2.06 ms (485 MiB/s) | 37.7 ms (425 MiB/s) |
| **Scalar** | 11.5 µs (85 MiB/s) | 724 µs (86 MiB/s) | 11.5 ms (87 MiB/s) | 189 ms (85 MiB/s) |

**Speedup vs Scalar:** 5.5× (L1) → 5.6× (L2+). Consistent speedup; branchy %XX decode benefits from SIMD.

### ValidUTF8

| Implementation | 1 KiB | 64 KiB | 1 MiB | 16 MiB | 256 MiB |
|---|---|---|---|---|---|
| **SimdText** | 763 ns (1.25 GiB/s) | 47.8 µs (1.28 GiB/s) | 685 µs (1.43 GiB/s) | 8.50 ms (1.84 GiB/s) | 166 ms (1.51 GiB/s) |

No scalar/STL baseline in this benchmark. Throughput is low relative to other ops — UTF-8 validation is inherently branchy.

## Cache Hierarchy Analysis

### Performance Drop-offs

| Operation | L1→L2 Drop | L2→L3 Drop | Beyond L3 Drop |
|---|---|---|---|
| CountNewlines | 110→105 GiB/s (5%) | 105→95 GiB/s (10%) | 95→28 GiB/s (**71%**) |
| IsASCII | 184→137 GiB/s (26%) | 137→99 GiB/s (28%) | 99→28 GiB/s (**72%**) |
| Lowercase | 42→30 GiB/s (29%) | 30→20 GiB/s (33%) | 20→2.4 GiB/s (**88%**) |
| Lines | 9.7→4.3 GiB/s (56%) | 4.3→3.5 GiB/s (19%) | — |
| HexEncode | 1.1→0.9 GiB/s (18%) | 0.9→0.6 GiB/s (33%) | — |
| Base64Encode | 2.2→1.4 GiB/s (36%) | 1.4→1.4 GiB/s (0%) | — |
| URLDecode | 0.5→0.5 GiB/s (0%) | 0.5→0.4 GiB/s (15%) | — |
| ValidUTF8 | 1.3→1.4 GiB/s (↑8%) | 1.4→1.8 GiB/s (↑31%) | 1.8→1.5 GiB/s (17%) |

**Key finding:** Read-only operations (CountNewlines, IsASCII) hit 110+ GiB/s in L2 — near theoretical bandwidth for this VPS. The beyond-L3 cliff is severe (70-88% drop), indicating the memory bus is the bottleneck for large buffers.

**Anomaly:** ValidUTF8 *improves* from L1 to L3. Likely cause: the 1 KiB benchmark is too small to amortize validation setup overhead (multi-pass SIMD approach). Once pipelines are warm, throughput scales well.

### Theoretical Memory Bandwidth Comparison

With 2 cores at 2.4 GHz and DDR4, theoretical peak is ~25-38 GiB/s (dual-channel). SimdText achieves:
- **CountNewlines L2:** 110 GiB/s (reads are L2-cache resident, ~2.9× memory bandwidth)
- **CountNewlines beyond L3:** 28 GiB/s (matches memory bandwidth — **bandwidth-bound**)
- **IsASCII L2:** 184 GiB/s (very cache-friendly, ~4.8× memory bandwidth)
- **Lowercase beyond L3:** 2.4 GiB/s (read+write, **8.5% of memory bandwidth** — write-heavy workload)

## Top Bottlenecks

1. **Memory bandwidth beyond L3** — All operations degrade severely past the 32 MiB L3. CountNewlines and IsASCII become purely memory-bound. No software fix; this is a hardware limit.

2. **Lowercase write throughput** — Only 2.4 GiB/s for 256 MiB buffers (read+write = 2× traffic). Effective 4.8 GiB/s on the bus vs ~38 GiB/s theoretical. Likely cause: store-to-load forwarding stalls or non-temporal stores not used.

3. **Lines allocation overhead** — Lines splits into per-line strings, making heap allocation the bottleneck rather than SIMD scanning. Only 3.5 GiB/s at 16 MiB.

4. **HexEncode output expansion** — 1:2 expansion ratio + non-trivial LUT shuffle limits throughput to ~1 GiB/s even in L1.

5. **ValidUTF8 multi-pass complexity** — Multiple SIMD passes for different byte classes reduce throughput to ~1.5 GiB/s, far below the ~100 GiB/s of simpler scans.

## Recommendations

1. **Non-temporal stores for Lowercase/encode ops** on buffers > L3 size. This avoids cache pollution and RFO traffic, potentially doubling write-heavy throughput.

2. **Prefetch hints** for CountNewlines/IsASCII on buffers > 16 MiB to hide memory latency.

3. **Lines: batch/string-view mode** — Return line spans (offset+length) instead of allocating `std::string` per line. Would eliminate the heap bottleneck and likely reach 8+ GiB/s.

4. **HexEncode: wider LUT** — A 512-byte LUT (4 entries per byte, pre-packed) could eliminate the per-nibble lookup and reduce to a single gather + shuffle per byte.

5. **ValidUTF8: consider Highway port** — The current scalar-SIMD approach may benefit from Highway's portable SIMD intrinsics, which can target AVX-512 VBMI for more efficient byte shuffling.

6. **Benchmark on bare metal** — VPS results have noisy neighbors and shared caches. Real-world throughput on dedicated hardware could be 2-3× higher for memory-bound operations.
