/*
 * Copyright (C) 2019-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/array_count.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/string.h"
#include "shared/source/os_interface/linux/os_memory_linux.h"
#include "shared/source/utilities/stackvec.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"

#include "gtest/gtest.h"

#include <algorithm>
#include <sstream>

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

    void getMemoryMaps(MemoryMaps &memoryMaps) override {
        static constexpr std::string_view mapsContent =
            "564fcd1fa000-564fcd202000 r-xp 00000000 08:03 3670041 /bin/cat\n"
            "564fcd401000-564fcd402000 r--p 00007000 08:03 3670041 /bin/cat\n"
            "564fcd402000-564fcd403000 rw-p 00008000 08:03 3670041 /bin/cat\n"
            "564fcdf40000-564fcdf61000 rw-p 00000000 00:00 0 [heap]\n"
            "7fded3d79000-7fded4879000 r--p 00000000 08:03 3938831 /usr/lib/locale/locale-archive\n"
            "7fded4879000-7fded4a60000 r-xp 00000000 08:03 4199137 /lib/x86_64-linux-gnu/libc-2.27.so\n"
            "7fded4a60000-7fded4c60000 ---p 001e7000 08:03 4199137 /lib/x86_64-linux-gnu/libc-2.27.so\n"
            "7fded4c60000-7fded4c64000 r--p 001e7000 08:03 4199137 /lib/x86_64-linux-gnu/libc-2.27.so\n"
            "7fded4c64000-7fded4c66000 rw-p 001eb000 08:03 4199137 /lib/x86_64-linux-gnu/libc-2.27.so\n"
            "7fded4c66000-7fded4c6a000 rw-p 00000000 00:00 0\n"
            "7fded4c6a000-7fded4c91000 r-xp 00000000 08:03 4199109 /lib/x86_64-linux-gnu/ld-2.27.so\n"
            "7fded4e54000-7fded4e78000 rw-p 00000000 00:00 0\n"
            "7fded4e91000-7fded4e92000 r--p 00027000 08:03 4199109 /lib/x86_64-linux-gnu/ld-2.27.so\n"
            "7fded4e92000-7fded4e93000 rw-p 00028000 08:03 4199109 /lib/x86_64-linux-gnu/ld-2.27.so\n"
            "7fded4e93000-7fded4e94000 rw-p 00000000 00:00 0\n"
            "7ffd6dfa2000-7ffd6dfc3000 rw-p 00000000 00:00 0 [stack]\n"
            "7ffd6dfe8000-7ffd6dfeb000 r--p 00000000 00:00 0 [vvar]\n"
            "7ffd6dfeb000-7ffd6dfec000 r-xp 00000000 00:00 0 [vdso]\n"
            "ffffffffff600000-ffffffffff601000 --xp 00000000 00:00 0 [vsyscall]\n";
        std::istringstream iss{std::string(mapsContent)};
        parseMemoryMaps(iss, memoryMaps);
    }

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

TEST(OSMemoryLinux, GivenOSMemoryLinuxWhenGetMemoryMapsIsCalledThenMapsAreParsed) {
    VariableBackup<decltype(SysCalls::sysCallsOpen)> openBkp(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsPread)> preadBkp(&SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        constexpr std::string_view content =
            "564fcd1fa000-564fcd202000 r-xp 00000000 08:03 3670041 /bin/cat\n"
            "7ffd6dfa2000-7ffd6dfc3000 rw-p 00000000 00:00 0 [stack]\n";
        memcpy_s(buf, count, content.data(), content.size());
        return static_cast<ssize_t>(content.size());
    });
    VariableBackup<decltype(SysCalls::sysCallsClose)> closeBkp(&SysCalls::sysCallsClose, [](int fd) -> int {
        return 0;
    });

    OSMemoryLinux osMemory;
    OSMemory::MemoryMaps memoryMaps;
    osMemory.getMemoryMaps(memoryMaps);
    EXPECT_EQ(2u, memoryMaps.size());
    EXPECT_EQ(0x564fcd1fa000u, memoryMaps[0].start);
    EXPECT_EQ(0x7ffd6dfc3000u, memoryMaps[1].end);
}

TEST(OSMemoryLinux, GivenOSMemoryLinuxWhenMapsFileSpansMultipleChunksThenAllMapsAreParsed) {
    static int preadCallCount;
    preadCallCount = 0;

    VariableBackup<decltype(SysCalls::sysCallsOpen)> openBkp(&SysCalls::sysCallsOpen,
                                                             [](const char *, int) -> int { return 1; });
    VariableBackup<decltype(SysCalls::sysCallsPread)> preadBkp(&SysCalls::sysCallsPread,
                                                               [](int, void *buf, size_t count, off_t offset) -> ssize_t {
                                                                   ++preadCallCount;
                                                                   // line is exactly 64 bytes so 1 MiB / 64 = 16384 exact repetitions
                                                                   if (offset == 0) {
                                                                       constexpr std::string_view line = "564fcd1fa000-564fcd202000 r-xp 00000000 08:03 03670041 /bin/cat\n";
                                                                       char *dst = static_cast<char *>(buf);
                                                                       size_t written = 0;
                                                                       while (written + line.size() <= count) {
                                                                           memcpy_s(dst + written, count - written, line.data(), line.size());
                                                                           written += line.size();
                                                                       }
                                                                       return static_cast<ssize_t>(count);
                                                                   }
                                                                   constexpr std::string_view chunk2 = "7ffd6dfa2000-7ffd6dfc3000 rw-p 00000000 00:00 0 [stack]\n";
                                                                   memcpy_s(buf, count, chunk2.data(), chunk2.size());
                                                                   return static_cast<ssize_t>(chunk2.size());
                                                               });
    VariableBackup<decltype(SysCalls::sysCallsClose)> closeBkp(&SysCalls::sysCallsClose,
                                                               [](int) -> int { return 0; });

    OSMemoryLinux osMemory;
    OSMemory::MemoryMaps memoryMaps;
    osMemory.getMemoryMaps(memoryMaps);

    constexpr size_t chunk1Lines = 1024u * 1024u / 64u; // 16384
    EXPECT_EQ(2, preadCallCount);
    EXPECT_EQ(chunk1Lines + 1u, memoryMaps.size());

    auto chunk1Count = std::count_if(memoryMaps.begin(), memoryMaps.end(),
                                     [](const OSMemory::MappedRegion &r) {
                                         return r.start == 0x564fcd1fa000u && r.end == 0x564fcd202000u;
                                     });
    EXPECT_EQ(static_cast<ptrdiff_t>(chunk1Lines), chunk1Count);

    EXPECT_EQ(0x7ffd6dfa2000u, memoryMaps.back().start);
    EXPECT_EQ(0x7ffd6dfc3000u, memoryMaps.back().end);
}

TEST(OSMemoryLinux, GivenOSMemoryLinuxWhenMapsFileIsEmptyThenMemoryMapsIsEmpty) {
    VariableBackup<decltype(SysCalls::sysCallsOpen)> openBkp(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return 1;
    });
    VariableBackup<decltype(SysCalls::sysCallsPread)> preadBkp(&SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        return 0;
    });
    VariableBackup<decltype(SysCalls::sysCallsClose)> closeBkp(&SysCalls::sysCallsClose, [](int fd) -> int {
        return 0;
    });

    OSMemoryLinux osMemory;
    OSMemory::MemoryMaps memoryMaps;
    osMemory.getMemoryMaps(memoryMaps);
    EXPECT_TRUE(memoryMaps.empty());
}

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

    OSMemory::MemoryMaps memoryMaps;
    mockOSMemoryLinux->getMemoryMaps(memoryMaps);

    static const OSMemory::MappedRegion referenceMaps[] = {
        {0x564fcd1fa000, 0x564fcd202000}, {0x564fcd401000, 0x564fcd402000}, {0x564fcd402000, 0x564fcd403000}, {0x564fcdf40000, 0x564fcdf61000}, {0x7fded3d79000, 0x7fded4879000}, {0x7fded4879000, 0x7fded4a60000}, {0x7fded4a60000, 0x7fded4c60000}, {0x7fded4c60000, 0x7fded4c64000}, {0x7fded4c64000, 0x7fded4c66000}, {0x7fded4c66000, 0x7fded4c6a000}, {0x7fded4c6a000, 0x7fded4c91000}, {0x7fded4e54000, 0x7fded4e78000}, {0x7fded4e91000, 0x7fded4e92000}, {0x7fded4e92000, 0x7fded4e93000}, {0x7fded4e93000, 0x7fded4e94000}, {0x7ffd6dfa2000, 0x7ffd6dfc3000}, {0x7ffd6dfe8000, 0x7ffd6dfeb000}, {0x7ffd6dfeb000, 0x7ffd6dfec000}, {0xffffffffff600000, 0xffffffffff601000}};

    EXPECT_FALSE(memoryMaps.empty());
    EXPECT_EQ(memoryMaps.size(), arrayCount(referenceMaps));

    for (size_t i = 0; i < memoryMaps.size(); ++i) {
        EXPECT_EQ(memoryMaps[i].start, referenceMaps[i].start);
        EXPECT_EQ(memoryMaps[i].end, referenceMaps[i].end);
    }
}

}; // namespace NEO
