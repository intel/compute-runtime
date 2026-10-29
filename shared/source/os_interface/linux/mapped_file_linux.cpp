/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/sys_calls.h"
#include "shared/source/os_interface/mapped_file.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

namespace NEO {

class MappedFileLinux : public MappedFile {
  public:
    MappedFileLinux(void *view, uint64_t viewSize) : MappedFile(static_cast<uint8_t *>(view), viewSize) {}

    ~MappedFileLinux() override {
        SysCalls::munmap(this->base, static_cast<size_t>(this->size));
    }
};

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

    auto fileSize = static_cast<uint64_t>(fileStatus.st_size);
    void *view = SysCalls::mmap(nullptr, static_cast<size_t>(fileSize), PROT_READ | PROT_WRITE, MAP_SHARED, fileDescriptor, 0);
    SysCalls::close(fileDescriptor);
    if (view == MAP_FAILED) {
        return nullptr;
    }

    return std::make_unique<MappedFileLinux>(view, fileSize);
}

} // namespace NEO
