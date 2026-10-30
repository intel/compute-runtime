/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/constants.h"
#include "shared/source/os_interface/mapped_file.h"
#include "shared/test/common/os_interface/windows/mock_sys_calls.h"

#include "gtest/gtest.h"

namespace NEO {
namespace SysCalls {
extern size_t createFileACalled;
extern size_t createFileACreateAlwaysCalled;
extern const size_t createFileAResultsCount;
extern HANDLE createFileAResults[];
extern size_t closeHandleCalled;
extern size_t getFileSizeExCalled;
extern BOOL getFileSizeExResult;
extern const size_t getFileSizeExFileSizesCount;
extern LONGLONG getFileSizeExFileSizes[];
extern size_t createFileMappingACalled;
extern uint64_t createFileMappingALastMaximumSize;
extern const size_t createFileMappingAResultsCount;
extern HANDLE createFileMappingAResults[];
extern size_t mapViewOfFileCalled;
extern const size_t mapViewOfFileResultsCount;
extern LPVOID mapViewOfFileResults[];
extern size_t unmapViewOfFileCalled;
extern LPCVOID unmapViewOfFileLastAddress;
extern DWORD createFileALastFlagsAndAttributes;
} // namespace SysCalls

class MappedFileSysCallsMock {
  public:
    MappedFileSysCallsMock() {
        resetMocks();
    }

    ~MappedFileSysCallsMock() {
        resetMocks();
    }

    void makeFileMappable(size_t openIndex, void *base, uint64_t size) {
        SysCalls::createFileAResults[openIndex] = reinterpret_cast<HANDLE>(dummyHandle);
        SysCalls::getFileSizeExFileSizes[openIndex] = static_cast<LONGLONG>(size);
        SysCalls::createFileMappingAResults[openIndex] = reinterpret_cast<HANDLE>(dummyHandle);
        SysCalls::mapViewOfFileResults[openIndex] = base;
    }

    void makeFileNotOpenable(size_t openIndex) {
        SysCalls::createFileAResults[openIndex] = INVALID_HANDLE_VALUE;
    }

    size_t getOpenCount() const {
        return SysCalls::createFileACalled;
    }

    size_t getUnmapCount() const {
        return SysCalls::unmapViewOfFileCalled;
    }

    size_t getCreateCount() const {
        return SysCalls::createFileACreateAlwaysCalled;
    }

    uint64_t getLastCreatedSize() const {
        return SysCalls::createFileMappingALastMaximumSize;
    }

  protected:
    void resetMocks() {
        for (size_t i = 0; i < SysCalls::createFileAResultsCount; i++) {
            SysCalls::createFileAResults[i] = nullptr;
        }
        for (size_t i = 0; i < SysCalls::getFileSizeExFileSizesCount; i++) {
            SysCalls::getFileSizeExFileSizes[i] = 0;
        }
        for (size_t i = 0; i < SysCalls::createFileMappingAResultsCount; i++) {
            SysCalls::createFileMappingAResults[i] = nullptr;
        }
        for (size_t i = 0; i < SysCalls::mapViewOfFileResultsCount; i++) {
            SysCalls::mapViewOfFileResults[i] = nullptr;
        }
        SysCalls::createFileACalled = 0u;
        SysCalls::createFileACreateAlwaysCalled = 0u;
        SysCalls::createFileMappingALastMaximumSize = 0u;
        SysCalls::closeHandleCalled = 0u;
        SysCalls::getFileSizeExCalled = 0u;
        SysCalls::getFileSizeExResult = TRUE;
        SysCalls::createFileMappingACalled = 0u;
        SysCalls::mapViewOfFileCalled = 0u;
        SysCalls::unmapViewOfFileCalled = 0u;
        SysCalls::unmapViewOfFileLastAddress = nullptr;
        SysCalls::createFileALastFlagsAndAttributes = 0u;
    }
};

struct MappedFileWinTests : public ::testing::Test {
    MappedFileSysCallsMock sysCallsMock;
    uint8_t fileView[64]{};
};

TEST_F(MappedFileWinTests, givenFileWhichCannotBeOpenedWhenOpeningMappedFileThenNullptrIsReturned) {
    sysCallsMock.makeFileNotOpenable(0u);

    EXPECT_EQ(nullptr, MappedFile::open("file.bin"));
    EXPECT_EQ(1u, SysCalls::createFileACalled);
    EXPECT_EQ(0u, SysCalls::getFileSizeExCalled);
    EXPECT_EQ(0u, SysCalls::closeHandleCalled);
}

TEST_F(MappedFileWinTests, givenGetFileSizeFailureWhenOpeningMappedFileThenFileIsClosedAndNullptrIsReturned) {
    sysCallsMock.makeFileMappable(0u, fileView, sizeof(fileView));
    SysCalls::getFileSizeExResult = FALSE;

    EXPECT_EQ(nullptr, MappedFile::open("file.bin"));
    EXPECT_EQ(1u, SysCalls::getFileSizeExCalled);
    EXPECT_EQ(0u, SysCalls::createFileMappingACalled);
    EXPECT_EQ(1u, SysCalls::closeHandleCalled);
}

TEST_F(MappedFileWinTests, givenEmptyFileWhenOpeningMappedFileThenFileIsClosedAndNullptrIsReturned) {
    sysCallsMock.makeFileMappable(0u, fileView, 0u);

    EXPECT_EQ(nullptr, MappedFile::open("file.bin"));
    EXPECT_EQ(0u, SysCalls::createFileMappingACalled);
    EXPECT_EQ(1u, SysCalls::closeHandleCalled);
}

TEST_F(MappedFileWinTests, givenCreateFileMappingFailureWhenOpeningMappedFileThenFileIsClosedAndNullptrIsReturned) {
    sysCallsMock.makeFileMappable(0u, fileView, sizeof(fileView));
    SysCalls::createFileMappingAResults[0] = nullptr;

    EXPECT_EQ(nullptr, MappedFile::open("file.bin"));
    EXPECT_EQ(1u, SysCalls::createFileMappingACalled);
    EXPECT_EQ(0u, SysCalls::mapViewOfFileCalled);
    EXPECT_EQ(1u, SysCalls::closeHandleCalled);
}

TEST_F(MappedFileWinTests, givenMapViewOfFileFailureWhenOpeningMappedFileThenMappingAndFileAreClosedAndNullptrIsReturned) {
    sysCallsMock.makeFileMappable(0u, nullptr, sizeof(fileView));

    EXPECT_EQ(nullptr, MappedFile::open("file.bin"));
    EXPECT_EQ(1u, SysCalls::mapViewOfFileCalled);
    EXPECT_EQ(2u, SysCalls::closeHandleCalled);
    EXPECT_EQ(0u, SysCalls::unmapViewOfFileCalled);
}

TEST_F(MappedFileWinTests, givenMappableFileWhenOpeningMappedFileThenViewAndFileSizeAreReturnedAndReleasedOnDestruction) {
    sysCallsMock.makeFileMappable(0u, fileView, sizeof(fileView));

    auto mappedFile = MappedFile::open("file.bin");
    ASSERT_NE(nullptr, mappedFile);
    EXPECT_EQ(static_cast<DWORD>(FILE_ATTRIBUTE_NORMAL), SysCalls::createFileALastFlagsAndAttributes);
    EXPECT_EQ(fileView, mappedFile->getBase());
    EXPECT_EQ(sizeof(fileView), mappedFile->getSize());
    EXPECT_EQ(0u, SysCalls::closeHandleCalled);

    mappedFile.reset();
    EXPECT_EQ(1u, SysCalls::unmapViewOfFileCalled);
    EXPECT_EQ(fileView, SysCalls::unmapViewOfFileLastAddress);
    EXPECT_EQ(2u, SysCalls::closeHandleCalled);
}

TEST_F(MappedFileWinTests, givenFileWhichCannotBeCreatedWhenCreatingMappedFileThenNullptrIsReturned) {
    sysCallsMock.makeFileNotOpenable(0u);

    EXPECT_EQ(nullptr, MappedFile::create("file.bin", MemoryConstants::pageSize64k));
    EXPECT_EQ(1u, SysCalls::createFileACreateAlwaysCalled);
    EXPECT_EQ(0u, SysCalls::createFileMappingACalled);
}

TEST_F(MappedFileWinTests, givenCreatableFileWhenCreatingMappedFileThenFileIsOverwrittenAndMappedWithRequestedSizeAndDeletedOnClose) {
    sysCallsMock.makeFileMappable(0u, fileView, 0u);
    constexpr uint64_t fileSize = 0x100000000ull + MemoryConstants::pageSize64k;

    auto mappedFile = MappedFile::create("file.bin", fileSize);
    ASSERT_NE(nullptr, mappedFile);
    EXPECT_EQ(1u, SysCalls::createFileACreateAlwaysCalled);
    EXPECT_EQ(static_cast<DWORD>(FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE), SysCalls::createFileALastFlagsAndAttributes);
    EXPECT_EQ(0u, SysCalls::getFileSizeExCalled);
    EXPECT_EQ(fileSize, SysCalls::createFileMappingALastMaximumSize);
    EXPECT_EQ(fileView, mappedFile->getBase());
    EXPECT_EQ(fileSize, mappedFile->getSize());

    mappedFile.reset();
    EXPECT_EQ(1u, SysCalls::unmapViewOfFileCalled);
    EXPECT_EQ(2u, SysCalls::closeHandleCalled);
}

} // namespace NEO

#include "shared/test/unit_test/aub/aub_center_tbx_shared_memory_tests.inl"
