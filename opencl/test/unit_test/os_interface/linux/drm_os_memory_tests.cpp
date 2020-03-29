/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/memory_constants.h"
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

}; // namespace NEO
