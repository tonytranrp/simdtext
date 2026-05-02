#include "simdtext/diff.hpp"
#include "simdtext/scan.hpp"
#include <cstring>
#include <algorithm>

namespace simdtext {

namespace {

std::vector<std::string_view> split_lines(std::string_view text) {
    std::vector<std::string_view> lines;
    size_t start = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\n') {
            lines.push_back(text.substr(start, i - start));
            start = i + 1;
        }
    }
    if (start < text.size()) {
        lines.push_back(text.substr(start));
    }
    return lines;
}

} // anonymous namespace

std::vector<DiffOp> line_diff(std::string_view a, std::string_view b) {
    auto a_lines = split_lines(a);
    auto b_lines = split_lines(b);

    const size_t m = a_lines.size();
    const size_t n = b_lines.size();

    // LCS via dynamic programming
    // Use only two rows to save memory
    std::vector<size_t> prev(n + 1, 0);
    std::vector<size_t> curr(n + 1, 0);

    // Also need to track the full DP table for backtracking
    // For small inputs, use full table; for large, use optimization
    std::vector<std::vector<size_t>> dp;
    if (m * n <= 1000000) { // reasonable size
        dp.assign(m + 1, std::vector<size_t>(n + 1, 0));
        for (size_t i = 1; i <= m; ++i) {
            for (size_t j = 1; j <= n; ++j) {
                if (a_lines[i-1] == b_lines[j-1]) {
                    dp[i][j] = dp[i-1][j-1] + 1;
                } else {
                    dp[i][j] = std::max(dp[i-1][j], dp[i][j-1]);
                }
            }
        }
    }

    // Backtrack to produce diff
    std::vector<DiffOp> result;
    size_t i = m, j = n;
    std::vector<DiffOp> reversed;

    if (!dp.empty()) {
        while (i > 0 || j > 0) {
            if (i > 0 && j > 0 && a_lines[i-1] == b_lines[j-1]) {
                reversed.push_back({DiffOp::Equal, a_lines[i-1], b_lines[j-1], i, j});
                --i; --j;
            } else if (j > 0 && (i == 0 || dp[i][j-1] >= dp[i-1][j])) {
                reversed.push_back({DiffOp::Insert, {}, b_lines[j-1], 0, j});
                --j;
            } else {
                reversed.push_back({DiffOp::Delete, a_lines[i-1], {}, i, 0});
                --i;
            }
        }
    } else {
        // Fallback: simple line-by-line comparison (no LCS)
        i = 0; j = 0;
        while (i < m && j < n) {
            if (a_lines[i] == b_lines[j]) {
                reversed.push_back({DiffOp::Equal, a_lines[i], b_lines[j], i+1, j+1});
                ++i; ++j;
            } else {
                reversed.push_back({DiffOp::Delete, a_lines[i], {}, i+1, 0});
                reversed.push_back({DiffOp::Insert, {}, b_lines[j], 0, j+1});
                ++i; ++j;
            }
        }
        while (i < m) { reversed.push_back({DiffOp::Delete, a_lines[i], {}, i+1, 0}); ++i; }
        while (j < n) { reversed.push_back({DiffOp::Insert, {}, b_lines[j], 0, j+1}); ++j; }
    }

    result.reserve(reversed.size());
    for (auto it = reversed.rbegin(); it != reversed.rend(); ++it) {
        result.push_back(*it);
    }
    return result;
}

size_t count_diff_lines(std::string_view a, std::string_view b) {
    auto ops = line_diff(a, b);
    size_t count = 0;
    for (const auto& op : ops) {
        if (op.type != DiffOp::Equal) ++count;
    }
    return count;
}

bool text_equal(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) return false;
    if (a.data() == b.data()) return true;
    return std::memcmp(a.data(), b.data(), a.size()) == 0;
}

size_t common_prefix_length(std::string_view a, std::string_view b) noexcept {
    size_t len = std::min(a.size(), b.size());
    size_t i = 0;
    // Compare 8 bytes at a time
    while (i + 8 <= len) {
        uint64_t va, vb;
        __builtin_memcpy(&va, a.data() + i, 8);
        __builtin_memcpy(&vb, b.data() + i, 8);
        if (va != vb) {
            // Find first differing byte
            break;
        }
        i += 8;
    }
    // Byte-by-byte for the rest
    while (i < len && a[i] == b[i]) ++i;
    return i;
}

size_t common_suffix_length(std::string_view a, std::string_view b) noexcept {
    size_t ai = a.size();
    size_t bi = b.size();
    size_t count = 0;
    while (ai > 0 && bi > 0 && a[ai-1] == b[bi-1]) {
        --ai; --bi; ++count;
    }
    return count;
}

} // namespace simdtext
