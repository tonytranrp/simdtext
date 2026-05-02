#pragma once

/// @file file.hpp
/// @brief Memory-mapped file access and line-by-line scanning.

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace simdtext {

/// Memory-mapped file (zero-copy read access).
class MappedFile {
public:
    MappedFile();
    explicit MappedFile(const char* path);
    ~MappedFile();

    MappedFile(MappedFile&&) noexcept;
    MappedFile& operator=(MappedFile&&) noexcept;
    MappedFile(const MappedFile&) = delete;
    MappedFile& operator=(const MappedFile&) = delete;

    bool open(const char* path);
    std::string_view view() const noexcept;
    size_t size() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/// High-level file scanner for line-by-line processing.
class FileScanner {
public:
    explicit FileScanner(const char* path);
    explicit FileScanner(const std::string& path);

    bool is_open() const;

    /// Iterate over each line in the file.
    void each_line(std::function<void(std::string_view)> callback) const;

    /// Iterate over lines containing the needle.
    void each_line_containing(std::string_view needle,
                               std::function<void(std::string_view)> callback) const;

    /// Count total lines in the file.
    size_t count_lines() const;

    /// Count lines containing the needle.
    size_t count_matching(std::string_view needle) const;

private:
    MappedFile file_;
};

} // namespace simdtext
