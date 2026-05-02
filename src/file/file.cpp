#include "simdtext/simdtext.hpp"

#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
  #include <io.h>
#else
  #include <sys/mman.h>
  #include <sys/stat.h>
  #include <fcntl.h>
  #include <unistd.h>
#endif

#include <filesystem>

namespace simdtext {

// ── MappedFile ─────────────────────────────────────────────

class MappedFile::Impl {
public:
    Impl() = default;

    ~Impl() {
#ifdef _WIN32
        if (data_) {
            UnmapViewOfFile(data_);
        }
        if (mapping_) {
            CloseHandle(mapping_);
        }
        if (handle_ != INVALID_HANDLE_VALUE) {
            CloseHandle(handle_);
        }
#else
        if (data_) {
            munmap(const_cast<char*>(data_), size_);
        }
        if (fd_ >= 0) {
            close(fd_);
        }
#endif
    }

    bool open(std::filesystem::path path) {
#ifdef _WIN32
        handle_ = CreateFileW(
            path.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (handle_ == INVALID_HANDLE_VALUE) [[unlikely]] return false;

        LARGE_INTEGER file_size;
        if (!GetFileSizeEx(handle_, &file_size)) [[unlikely]] return false;
        size_ = static_cast<size_t>(file_size.QuadPart);

        if (size_ == 0) {
            data_ = nullptr;
            return true;
        }

        mapping_ = CreateFileMappingW(
            handle_,
            nullptr,
            PAGE_READONLY,
            0,
            0,
            nullptr);
        if (!mapping_) [[unlikely]] return false;

        data_ = static_cast<const char*>(MapViewOfFile(
            mapping_,
            FILE_MAP_READ,
            0,
            0,
            0));
        if (!data_) [[unlikely]] return false;
        return true;
#else
        // macOS and Linux both use POSIX mmap
        fd_ = ::open(path.c_str(), O_RDONLY);
        if (fd_ < 0) [[unlikely]] return false;

        struct stat st;
        if (fstat(fd_, &st) < 0) [[unlikely]] return false;
        size_ = static_cast<size_t>(st.st_size);

        if (size_ == 0) {
            data_ = nullptr;
            return true;
        }

        void* mapped = mmap(nullptr, size_, PROT_READ, MAP_PRIVATE, fd_, 0);
        if (mapped == MAP_FAILED) [[unlikely]] {
            data_ = nullptr;
            size_ = 0;
            return false;
        }

        data_ = static_cast<const char*>(mapped);
        return true;
#endif
    }

    std::string_view view() const noexcept {
        return {data_, size_};
    }

    size_t size() const noexcept { return size_; }

private:
    const char* data_ = nullptr;
    size_t size_ = 0;
#ifdef _WIN32
    HANDLE handle_ = INVALID_HANDLE_VALUE;
    HANDLE mapping_ = nullptr;
#else
    int fd_ = -1;
#endif
};

MappedFile::MappedFile() : impl_(std::make_unique<Impl>()) {}

MappedFile::MappedFile(const char* path) : impl_(std::make_unique<Impl>()) {
    impl_->open(path);
}

MappedFile::MappedFile(MappedFile&&) noexcept = default;
MappedFile& MappedFile::operator=(MappedFile&&) noexcept = default;
MappedFile::~MappedFile() = default;

bool MappedFile::open(const char* path) {
    return impl_->open(path);
}

std::string_view MappedFile::view() const noexcept {
    return impl_->view();
}

size_t MappedFile::size() const noexcept {
    return impl_->size();
}

// ── FileScanner ────────────────────────────────────────────

FileScanner::FileScanner(const char* path) : file_(path) {}

FileScanner::FileScanner(const std::string& path) : file_(path.c_str()) {}

bool FileScanner::is_open() const {
    return file_.size() > 0 || file_.view().data() != nullptr;
}

void FileScanner::each_line(std::function<void(std::string_view)> callback) const {
    for (const auto line : lines(file_.view())) {
        callback(line);
    }
}

void FileScanner::each_line_containing(std::string_view needle,
                                        std::function<void(std::string_view)> callback) const {
    for (const auto line : lines(file_.view())) {
        if (contains(line, needle)) {
            callback(line);
        }
    }
}

size_t FileScanner::count_lines() const {
    return count_newlines(file_.view());
}

size_t FileScanner::count_matching(std::string_view needle) const {
    size_t count = 0;
    for (const auto line : lines(file_.view())) {
        if (contains(line, needle)) ++count;
    }
    return count;
}

} // namespace simdtext
