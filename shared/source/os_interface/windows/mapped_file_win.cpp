/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/mapped_file.h"
#include "shared/source/os_interface/windows/sys_calls.h"

namespace NEO {

class MappedFileWin : public MappedFile {
  public:
    MappedFileWin(HANDLE fileHandle, HANDLE mappingHandle, void *view, uint64_t viewSize)
        : MappedFile(static_cast<uint8_t *>(view), viewSize), fileHandle(fileHandle), mappingHandle(mappingHandle) {}

    ~MappedFileWin() override {
        SysCalls::unmapViewOfFile(this->base);
        SysCalls::closeHandle(this->mappingHandle);
        SysCalls::closeHandle(this->fileHandle);
    }

  protected:
    HANDLE fileHandle;
    HANDLE mappingHandle;
};

namespace {
HANDLE openFileHandle(const std::string &filePath, DWORD creationDisposition, DWORD flagsAndAttributes) {
    return SysCalls::createFileA(filePath.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                 nullptr, creationDisposition, flagsAndAttributes, nullptr);
}

std::unique_ptr<MappedFile> mapFileHandle(HANDLE fileHandle, uint64_t fileSize) {
    HANDLE mappingHandle = SysCalls::createFileMappingA(fileHandle, nullptr, PAGE_READWRITE, static_cast<DWORD>(fileSize >> 32), static_cast<DWORD>(fileSize), nullptr);
    if (mappingHandle == nullptr) {
        SysCalls::closeHandle(fileHandle);
        return nullptr;
    }

    void *view = SysCalls::mapViewOfFile(mappingHandle, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (view == nullptr) {
        SysCalls::closeHandle(mappingHandle);
        SysCalls::closeHandle(fileHandle);
        return nullptr;
    }

    return std::make_unique<MappedFileWin>(fileHandle, mappingHandle, view, fileSize);
}
} // namespace

std::unique_ptr<MappedFile> MappedFile::open(const std::string &filePath) {
    HANDLE fileHandle = openFileHandle(filePath, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL);
    if (fileHandle == INVALID_HANDLE_VALUE) {
        return nullptr;
    }

    LARGE_INTEGER fileSize{};
    if (!SysCalls::getFileSizeEx(fileHandle, &fileSize) || fileSize.QuadPart <= 0) {
        SysCalls::closeHandle(fileHandle);
        return nullptr;
    }

    return mapFileHandle(fileHandle, static_cast<uint64_t>(fileSize.QuadPart));
}

std::unique_ptr<MappedFile> MappedFile::create(const std::string &filePath, uint64_t fileSize) {
    HANDLE fileHandle = openFileHandle(filePath, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE);
    if (fileHandle == INVALID_HANDLE_VALUE) {
        return nullptr;
    }

    return mapFileHandle(fileHandle, fileSize);
}

} // namespace NEO
