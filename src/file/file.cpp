#include "simdtext/simdtext.hpp"

#ifdef __linux__
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace simdtext {

// ── MappedFile ─────────────────────────────────────────────

class MappedFile::Impl {
public:
    Impl() : data_(nullptr), size_(0) {}

    ~Impl() {
#ifdef __linux__
        if (data_) {
            munmap(const_cast<char*>(data_), size_);
        }
        if (fd_ >= 0) {
            close(fd_);
        }
#endif
    }

    bool open(const char* path) {
#ifdef __linux__
        fd_ = ::open(path, O_RDONLY);
        if (fd_ < 0) return false;

        struct stat st;
        if (fstat(fd_, &st) < 0) return false;
        size_ = static_cast<size_t>(st.st_size);

        if (size_ == 0) {
            data_ = nullptr;
            return true;
        }

        void* mapped = mmap(nullptr, size_, PROT_READ, MAP_PRIVATE, fd_, 0);
        if (mapped == MAP_FAILED) {
            data_ = nullptr;
            size_ = 0;
            return false;
        }

        data_ = static_cast<const char*>(mapped);
        return true;
#else
        // Fallback: read file into memory
        std::ifstream f(path, std::ios::binary | std::ios::ate);
        if (!f) return false;
        size_ = static_cast<size_t>(f.tellg());
        f.seekg(0);
        buffer_.resize(size_);
        f.read(buffer_.data(), static_cast<std::streamsize>(size_));
        data_ = buffer_.data();
        return true;
#endif
    }

    std::string_view view() const {
        return std::string_view(data_, size_);
    }

    size_t size() const { return size_; }

private:
    const char* data_;
    size_t size_;
    int fd_ = -1;
    std::string buffer_; // fallback for non-Linux
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

std::string_view MappedFile::view() const {
    return impl_->view();
}

size_t MappedFile::size() const {
    return impl_->size();
}

// ── FileScanner ────────────────────────────────────────────

FileScanner::FileScanner(const char* path) : file_(path) {}

FileScanner::FileScanner(const std::string& path) : file_(path.c_str()) {}

bool FileScanner::is_open() const {
    return file_.size() > 0 || file_.view().data() != nullptr;
}

void FileScanner::each_line(std::function<void(std::string_view)> callback) const {
    for (auto line : lines(file_.view())) {
        callback(line);
    }
}

void FileScanner::each_line_containing(std::string_view needle,
                                        std::function<void(std::string_view)> callback) const {
    for (auto line : lines(file_.view())) {
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
    for (auto line : lines(file_.view())) {
        if (contains(line, needle)) count++;
    }
    return count;
}

} // namespace simdtext
