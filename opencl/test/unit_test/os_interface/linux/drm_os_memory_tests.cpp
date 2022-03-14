/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/file_io.h"
#include "shared/source/os_interface/linux/os_memory_linux.h"
#include "shared/source/utilities/stackvec.h"

#include "gtest/gtest.h"

namespace NEO {

class MockOSMemoryLinux : public OSMemoryLinux {
  public:
    static std::unique_ptr<MockOSMemoryLinux> create() {
        return std::make_unique<MockOSMemoryLinux>();
    }

    MockOSMemoryLinux() = default;

    void *mmapWrapper(void *addr, size_t size, int prot, int flags, int fd, off_t off) override {
        mmapWrapperCalled++;
        mmapWrapperParamsPassed.push_back({addr, size, prot, flags, fd, off});
        return this->baseMmapWrapper(addr, size, prot, flags, fd, off);
    }

    struct MmapWrapperParams {
        void *addr;
        size_t size;
        int prot;
        int flags;
        int fd;
        off_t off;
    };

    uint32_t mmapWrapperCalled = 0u;
    StackVec<MmapWrapperParams, 1> mmapWrapperParamsPassed{};

    int munmapWrapper(void *addr, size_t size) override {
        munmapWrapperCalled++;
        munmapWrapperParamsPassed.push_back({addr, size});
        return this->baseMunmapWrapper(addr, size);
    }

    struct MunmapWrapperParams {
        void *addr;
        size_t size;
    };

    uint32_t munmapWrapperCalled = 0u;
    StackVec<MunmapWrapperParams, 1> munmapWrapperParamsPassed{};

    void *baseMmapWrapper(void *addr, size_t size, int prot, int flags, int fd, off_t off) {
        return OSMemoryLinux::mmapWrapper(addr, size, prot, flags, fd, off);
    }

    int baseMunmapWrapper(void *addr, size_t size) {
        return OSMemoryLinux::munmapWrapper(addr, size);
    }
};

TEST(OSMemoryLinux, givenOSMemoryLinuxWhenReserveCpuAddressRangeIsCalledThenMinusOneIsPassedToMmapAsFdParam) {
    auto mockOSMemoryLinux = MockOSMemoryLinux::create();

    auto reservedCpuRange = mockOSMemoryLinux->reserveCpuAddressRange(MemoryConstants::pageSize, MemoryConstants::pageSize64k);

    mockOSMemoryLinux->releaseCpuAddressRange(reservedCpuRange);

    EXPECT_EQ(-1, mockOSMemoryLinux->mmapWrapperParamsPassed[0].fd);
    EXPECT_EQ(reservedCpuRange.originalPtr, mockOSMemoryLinux->munmapWrapperParamsPassed[0].addr);
    EXPECT_EQ(reservedCpuRange.actualReservedSize, mockOSMemoryLinux->munmapWrapperParamsPassed[0].size);
}

TEST(OSMemoryLinux, givenOSMemoryLinuxWhenReserveCpuAddressRangeIsCalledAndBaseAddressIsSpecifiedThenCorrectValueIsPassedToMmapAsAddrParam) {
    auto mockOSMemoryLinux = MockOSMemoryLinux::create();

    auto reservedCpuRange = mockOSMemoryLinux->reserveCpuAddressRange(reinterpret_cast<void *>(0x10000000), MemoryConstants::pageSize, MemoryConstants::pageSize64k);

    mockOSMemoryLinux->releaseCpuAddressRange(reservedCpuRange);

    EXPECT_EQ(reinterpret_cast<void *>(0x10000000), mockOSMemoryLinux->mmapWrapperParamsPassed[0].addr);
    EXPECT_EQ(-1, mockOSMemoryLinux->mmapWrapperParamsPassed[0].fd);
    EXPECT_EQ(reservedCpuRange.originalPtr, mockOSMemoryLinux->munmapWrapperParamsPassed[0].addr);
    EXPECT_EQ(reservedCpuRange.actualReservedSize, mockOSMemoryLinux->munmapWrapperParamsPassed[0].size);
}

TEST(OSMemoryLinux, givenOSMemoryLinuxWhenReserveCpuAddressRangeIsCalledAndBaseAddressIsNotSpecifiedThenoZeroIsPassedToMmapAsAddrParam) {
    auto mockOSMemoryLinux = MockOSMemoryLinux::create();

    auto reservedCpuRange = mockOSMemoryLinux->reserveCpuAddressRange(MemoryConstants::pageSize, MemoryConstants::pageSize64k);

    mockOSMemoryLinux->releaseCpuAddressRange(reservedCpuRange);

    EXPECT_EQ(nullptr, mockOSMemoryLinux->mmapWrapperParamsPassed[0].addr);
    EXPECT_EQ(-1, mockOSMemoryLinux->mmapWrapperParamsPassed[0].fd);
    EXPECT_EQ(reservedCpuRange.originalPtr, mockOSMemoryLinux->munmapWrapperParamsPassed[0].addr);
    EXPECT_EQ(reservedCpuRange.actualReservedSize, mockOSMemoryLinux->munmapWrapperParamsPassed[0].size);
}

TEST(OSMemoryLinux, GivenProcSelfMapsFileExistsWhenGetMemoryMapsIsQueriedThenValidValueIsReturned) {
    auto mockOSMemoryLinux = MockOSMemoryLinux::create();

    std::string mapsFile = "test_files/linux/proc/self/maps";
    EXPECT_TRUE(fileExists(mapsFile));

    OSMemory::MemoryMaps memoryMaps;
    mockOSMemoryLinux->getMemoryMaps(memoryMaps);

    static const OSMemory::MappedRegion referenceMaps[] = {
        {0x564fcd1fa000, 0x564fcd202000}, {0x564fcd401000, 0x564fcd402000}, {0x564fcd402000, 0x564fcd403000}, {0x564fcdf40000, 0x564fcdf61000}, {0x7fded3d79000, 0x7fded4879000}, {0x7fded4879000, 0x7fded4a60000}, {0x7fded4a60000, 0x7fded4c60000}, {0x7fded4c60000, 0x7fded4c64000}, {0x7fded4c64000, 0x7fded4c66000}, {0x7fded4c66000, 0x7fded4c6a000}, {0x7fded4c6a000, 0x7fded4c91000}, {0x7fded4e54000, 0x7fded4e78000}, {0x7fded4e91000, 0x7fded4e92000}, {0x7fded4e92000, 0x7fded4e93000}, {0x7fded4e93000, 0x7fded4e94000}, {0x7ffd6dfa2000, 0x7ffd6dfc3000}, {0x7ffd6dfe8000, 0x7ffd6dfeb000}, {0x7ffd6dfeb000, 0x7ffd6dfec000}, {0xffffffffff600000, 0xffffffffff601000}};

    EXPECT_FALSE(memoryMaps.empty());
    EXPECT_EQ(memoryMaps.size(), GTEST_ARRAY_SIZE_(referenceMaps));

    for (size_t i = 0; i < memoryMaps.size(); ++i) {
        EXPECT_EQ(memoryMaps[i].start, referenceMaps[i].start);
        EXPECT_EQ(memoryMaps[i].end, referenceMaps[i].end);
    }
}

}; // namespace NEO
