/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/sys_calls.h"
#include "shared/source/os_interface/mapped_file.h"

#include <fcntl.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/stat.h>

namespace NEO {

namespace {
void removeCreatedFile(const std::string &createdFilePath) {
    if (!createdFilePath.empty()) {
        SysCalls::unlink(createdFilePath);
    }
}
} // namespace

class MappedFileLinux : public MappedFile {
  public:
    MappedFileLinux(void *view, uint64_t viewSize, const std::string &createdFilePath) : MappedFile(static_cast<uint8_t *>(view), viewSize), createdFilePath(createdFilePath) {}

    ~MappedFileLinux() override {
        SysCalls::munmap(this->base, static_cast<size_t>(this->size));
        removeCreatedFile(this->createdFilePath);
    }

  protected:
    std::string createdFilePath;
};

namespace {
std::unique_ptr<MappedFile> mapFileDescriptor(int fileDescriptor, uint64_t fileSize, const std::string &createdFilePath) {
    void *view = SysCalls::mmap(nullptr, static_cast<size_t>(fileSize), PROT_READ | PROT_WRITE, MAP_SHARED, fileDescriptor, 0);
    SysCalls::close(fileDescriptor);
    if (view == MAP_FAILED) {
        removeCreatedFile(createdFilePath);
        return nullptr;
    }

    return std::make_unique<MappedFileLinux>(view, fileSize, createdFilePath);
}
} // namespace

std::unique_ptr<MappedFile> MappedFile::open(const std::string &filePath) {
    int fileDescriptor = SysCalls::open(filePath.c_str(), O_RDWR);
    if (fileDescriptor < 0) {
        return nullptr;
    }

    struct stat fileStatus = {};
    if (SysCalls::fstat(fileDescriptor, &fileStatus) != 0 || fileStatus.st_size <= 0) {
        SysCalls::close(fileDescriptor);
        return nullptr;
    }

    return mapFileDescriptor(fileDescriptor, static_cast<uint64_t>(fileStatus.st_size), std::string());
}

std::unique_ptr<MappedFile> MappedFile::create(const std::string &filePath, uint64_t fileSize) {
    int fileDescriptor = SysCalls::openWithMode(filePath.c_str(), O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    if (fileDescriptor < 0) {
        return nullptr;
    }

    if (SysCalls::ftruncate(fileDescriptor, static_cast<off_t>(fileSize)) != 0) {
        SysCalls::close(fileDescriptor);
        removeCreatedFile(filePath);
        return nullptr;
    }

    return mapFileDescriptor(fileDescriptor, fileSize, filePath);
}

} // namespace NEO
