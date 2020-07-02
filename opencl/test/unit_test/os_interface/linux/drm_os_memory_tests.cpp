/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/file_io.h"
#include "shared/source/os_interface/linux/os_memory_linux.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using ::testing::_;

namespace NEO {

class MockOSMemoryLinux : public OSMemoryLinux {
  public:
    static std::unique_ptr<MockOSMemoryLinux> create() {
        return std::make_unique<MockOSMemoryLinux>();
    }

    MockOSMemoryLinux() {
        ON_CALL(*this, mmapWrapper).WillByDefault([this](void *addr, size_t size, int prot, int flags, int fd, off_t off) {
            return this->baseMmapWrapper(addr, size, prot, flags, fd, off);
        });

        ON_CALL(*this, munmapWrapper).WillByDefault([this](void *addr, size_t size) {
            return this->baseMunmapWrapper(addr, size);
        });
    }

    MOCK_METHOD6(mmapWrapper, void *(void *, size_t, int, int, int, off_t));
    MOCK_METHOD2(munmapWrapper, int(void *, size_t));

    void *baseMmapWrapper(void *addr, size_t size, int prot, int flags, int fd, off_t off) {
        return OSMemoryLinux::mmapWrapper(addr, size, prot, flags, fd, off);
    }

    int baseMunmapWrapper(void *addr, size_t size) {
        return OSMemoryLinux::munmapWrapper(addr, size);
    }
};

TEST(OSMemoryLinux, givenOSMemoryLinuxWhenReserveCpuAddressRangeIsCalledThenMinusOneIsPassedToMmapAsFdParam) {
    auto mockOSMemoryLinux = MockOSMemoryLinux::create();

    EXPECT_CALL(*mockOSMemoryLinux, mmapWrapper(_, _, _, _, -1, _));

    auto reservedCpuRange = mockOSMemoryLinux->reserveCpuAddressRange(MemoryConstants::pageSize, MemoryConstants::pageSize64k);

    EXPECT_CALL(*mockOSMemoryLinux, munmapWrapper(reservedCpuRange.originalPtr, reservedCpuRange.actualReservedSize));

    mockOSMemoryLinux->releaseCpuAddressRange(reservedCpuRange);
}

TEST(OSMemoryLinux, givenOSMemoryLinuxWhenReserveCpuAddressRangeIsCalledAndBaseAddressIsSpecifiedThenCorrectValueIsPassedToMmapAsAddrParam) {
    auto mockOSMemoryLinux = MockOSMemoryLinux::create();

    EXPECT_CALL(*mockOSMemoryLinux, mmapWrapper(reinterpret_cast<void *>(0x10000000), _, _, _, -1, _));

    auto reservedCpuRange = mockOSMemoryLinux->reserveCpuAddressRange(reinterpret_cast<void *>(0x10000000), MemoryConstants::pageSize, MemoryConstants::pageSize64k);

    EXPECT_CALL(*mockOSMemoryLinux, munmapWrapper(reservedCpuRange.originalPtr, reservedCpuRange.actualReservedSize));

    mockOSMemoryLinux->releaseCpuAddressRange(reservedCpuRange);
}

TEST(OSMemoryLinux, givenOSMemoryLinuxWhenReserveCpuAddressRangeIsCalledAndBaseAddressIsNotSpecifiedThenoZeroIsPassedToMmapAsAddrParam) {
    auto mockOSMemoryLinux = MockOSMemoryLinux::create();

    EXPECT_CALL(*mockOSMemoryLinux, mmapWrapper(nullptr, _, _, _, -1, _));

    auto reservedCpuRange = mockOSMemoryLinux->reserveCpuAddressRange(MemoryConstants::pageSize, MemoryConstants::pageSize64k);

    EXPECT_CALL(*mockOSMemoryLinux, munmapWrapper(reservedCpuRange.originalPtr, reservedCpuRange.actualReservedSize));

    mockOSMemoryLinux->releaseCpuAddressRange(reservedCpuRange);
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
