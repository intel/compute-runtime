/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/os_interface/linux/os_memory_linux.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using ::testing::_;

namespace NEO {

class MockOSMemoryLinux : public OSMemoryLinux {
  public:
    static std::unique_ptr<MockOSMemoryLinux> create() {
        return std::make_unique<MockOSMemoryLinux>();
    }

    MOCK_METHOD6(mmapWrapper, void *(void *, size_t, int, int, int, off_t));
    MOCK_METHOD2(munmapWrapper, int(void *, size_t));
};

TEST(OSMemoryLinux, givenOSMemoryLinuxWhenReserveCpuAddressRangeIsCalledThenMinusOneIsPassedToMmapAsFdParam) {
    auto mockOSMemoryLinux = MockOSMemoryLinux::create();

    EXPECT_CALL(*mockOSMemoryLinux, mmapWrapper(_, _, _, _, -1, _));

    size_t size = 0x1024;
    auto reservedCpuAddr = mockOSMemoryLinux->reserveCpuAddressRange(size);

    EXPECT_CALL(*mockOSMemoryLinux, munmapWrapper(reservedCpuAddr, size));

    mockOSMemoryLinux->releaseCpuAddressRange(reservedCpuAddr, size);
}

}; // namespace NEO
