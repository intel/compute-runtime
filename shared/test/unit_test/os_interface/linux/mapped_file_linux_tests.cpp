/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/mapped_file.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"

#include "gtest/gtest.h"

#include <array>
#include <sys/mman.h>
#include <sys/stat.h>

namespace NEO {

struct MockedFileState {
    bool openable = false;
    void *base = nullptr;
    uint64_t size = 0;
};

class MappedFileSysCallsMock {
  public:
    static constexpr int firstFileDescriptor = 100;
    static constexpr size_t maxFiles = 4;

    MappedFileSysCallsMock() {
        files = {};
        openCount = 0u;
        closeCount = 0u;
        fstatResult = 0;
        unmapCount = 0u;
        lastUnmappedAddress = nullptr;
        lastUnmappedSize = 0u;
    }

    void makeFileMappable(size_t openIndex, void *base, uint64_t size) {
        files[openIndex] = MockedFileState{true, base, size};
    }

    void makeFileNotOpenable(size_t openIndex) {
        files[openIndex] = MockedFileState{};
    }

    size_t getOpenCount() const {
        return openCount;
    }

    size_t getUnmapCount() const {
        return unmapCount;
    }

    static MockedFileState &getFile(int fileDescriptor) {
        return files[static_cast<size_t>(fileDescriptor - firstFileDescriptor)];
    }

    static inline std::array<MockedFileState, maxFiles> files{};
    static inline size_t openCount = 0u;
    static inline size_t closeCount = 0u;
    static inline int fstatResult = 0;
    static inline size_t unmapCount = 0u;
    static inline void *lastUnmappedAddress = nullptr;
    static inline size_t lastUnmappedSize = 0u;

  protected:
    VariableBackup<decltype(SysCalls::sysCallsOpen)> openBackup{&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
                                                                    auto openIndex = openCount++;
                                                                    if (openIndex >= maxFiles || !files[openIndex].openable) {
                                                                        return -1;
                                                                    }
                                                                    return firstFileDescriptor + static_cast<int>(openIndex);
                                                                }};
    VariableBackup<decltype(SysCalls::sysCallsFstat)> fstatBackup{&SysCalls::sysCallsFstat, [](int fd, struct stat *buf) -> int {
                                                                      buf->st_size = static_cast<off_t>(getFile(fd).size);
                                                                      return fstatResult;
                                                                  }};
    VariableBackup<decltype(SysCalls::sysCallsMmap)> mmapBackup{&SysCalls::sysCallsMmap, [](void *addr, size_t size, int prot, int flags, int fd, off_t off) -> void * {
                                                                    auto base = getFile(fd).base;
                                                                    return base ? base : MAP_FAILED;
                                                                }};
    VariableBackup<decltype(SysCalls::sysCallsMunmap)> munmapBackup{&SysCalls::sysCallsMunmap, [](void *addr, size_t size) -> int {
                                                                        unmapCount++;
                                                                        lastUnmappedAddress = addr;
                                                                        lastUnmappedSize = size;
                                                                        return 0;
                                                                    }};
    VariableBackup<decltype(SysCalls::sysCallsClose)> closeBackup{&SysCalls::sysCallsClose, [](int fileDescriptor) -> int {
                                                                      closeCount++;
                                                                      return 0;
                                                                  }};
};

struct MappedFileLinuxTests : public ::testing::Test {
    MappedFileSysCallsMock sysCallsMock;
    uint8_t fileView[64]{};
};

TEST_F(MappedFileLinuxTests, givenFileWhichCannotBeOpenedWhenOpeningMappedFileThenNullptrIsReturned) {
    sysCallsMock.makeFileNotOpenable(0u);

    EXPECT_EQ(nullptr, MappedFile::open("file.bin"));
    EXPECT_EQ(1u, MappedFileSysCallsMock::openCount);
    EXPECT_EQ(0u, MappedFileSysCallsMock::closeCount);
}

TEST_F(MappedFileLinuxTests, givenFstatFailureWhenOpeningMappedFileThenFileIsClosedAndNullptrIsReturned) {
    sysCallsMock.makeFileMappable(0u, fileView, sizeof(fileView));
    MappedFileSysCallsMock::fstatResult = -1;

    EXPECT_EQ(nullptr, MappedFile::open("file.bin"));
    EXPECT_EQ(1u, MappedFileSysCallsMock::closeCount);
}

TEST_F(MappedFileLinuxTests, givenEmptyFileWhenOpeningMappedFileThenFileIsClosedAndNullptrIsReturned) {
    sysCallsMock.makeFileMappable(0u, fileView, 0u);

    EXPECT_EQ(nullptr, MappedFile::open("file.bin"));
    EXPECT_EQ(1u, MappedFileSysCallsMock::closeCount);
}

TEST_F(MappedFileLinuxTests, givenMmapFailureWhenOpeningMappedFileThenFileIsClosedAndNullptrIsReturned) {
    sysCallsMock.makeFileMappable(0u, nullptr, sizeof(fileView));

    EXPECT_EQ(nullptr, MappedFile::open("file.bin"));
    EXPECT_EQ(1u, MappedFileSysCallsMock::closeCount);
    EXPECT_EQ(0u, MappedFileSysCallsMock::unmapCount);
}

TEST_F(MappedFileLinuxTests, givenMappableFileWhenOpeningMappedFileThenViewAndFileSizeAreReturnedAndViewIsUnmappedOnDestruction) {
    sysCallsMock.makeFileMappable(0u, fileView, sizeof(fileView));

    auto mappedFile = MappedFile::open("file.bin");
    ASSERT_NE(nullptr, mappedFile);
    EXPECT_EQ(fileView, mappedFile->getBase());
    EXPECT_EQ(sizeof(fileView), mappedFile->getSize());
    EXPECT_EQ(1u, MappedFileSysCallsMock::closeCount);

    mappedFile.reset();
    EXPECT_EQ(1u, MappedFileSysCallsMock::unmapCount);
    EXPECT_EQ(fileView, MappedFileSysCallsMock::lastUnmappedAddress);
    EXPECT_EQ(sizeof(fileView), MappedFileSysCallsMock::lastUnmappedSize);
}

} // namespace NEO

#include "shared/test/unit_test/aub/aub_center_tbx_shared_memory_tests.inl"
