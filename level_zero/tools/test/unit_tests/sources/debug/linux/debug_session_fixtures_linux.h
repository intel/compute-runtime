/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/tools/source/debug/linux/debug_session.h"
#include "level_zero/tools/test/unit_tests/sources/debug/debug_session_common.h"

namespace L0 {
namespace ult {

struct DebugApiLinuxFixture : public DeviceFixture {
    void setUp() {
        setUp(nullptr);
    }

    void setUp(NEO::HardwareInfo *hwInfo);

    void tearDown() {
        DeviceFixture::tearDown();
    }
    DrmMock *mockDrm = nullptr;
    static constexpr uint8_t bufferSize = 16;
};

struct MockIoctlHandler : public L0::DebugSessionLinux::IoctlHandler {
    using EventPair = std::pair<char *, uint64_t>;
    using EventQueue = std::queue<EventPair>;
    int poll(pollfd *pollFd, unsigned long int numberOfFds, int timeout) override {
        passedTimeout = timeout;
        pollCounter++;

        if (eventQueue.empty() && pollRetVal >= 0) {
            return 0;
        }
        return pollRetVal;
    }

    int64_t pread(int fd, void *buf, size_t count, off_t offset) override {
        preadCalled++;
        preadOffset = offset;
        if ((midZeroReturn > 0) && (preadCalled > 1)) {
            midZeroReturn--;
            return 0;
        }

        if (!pReadArrayRef.empty()) {
            if (pread2RetVal && preadCalled == 2) {
                return pread2RetVal;
            }
            auto offsetInMemory = offset - pReadBase;
            auto result = memcpy_s(buf, count, pReadArrayRef.begin() + offsetInMemory, std::min(count, pReadArrayRef.size() - offsetInMemory));
            if (result == 0) {
                return count;
            }
            return -1;
        }

        if (count > 0 && preadRetVal > 0) {
            memset(buf, 0xaa, count);
        }
        if (pread2RetVal && preadCalled == 2) {
            return pread2RetVal;
        } else {
            return preadRetVal;
        }
    }

    int64_t pwrite(int fd, const void *buf, size_t count, off_t offset) override {
        pwriteCalled++;
        pwriteOffset = offset;
        if ((midZeroReturn > 0) && (pwriteCalled > 1)) {
            midZeroReturn--;
            return 0;
        }

        if (!pWriteArrayRef.empty()) {
            auto offsetInMemory = offset - pWriteBase;
            auto result = memcpy_s(pWriteArrayRef.begin() + offsetInMemory, pWriteArrayRef.size() - offsetInMemory, buf, count);
            if (result == 0) {
                return count;
            }
            return -1;
        }

        return pwriteRetVal;
    }

    void *mmap(void *addr, size_t size, int prot, int flags, int fd, off_t off) override {
        mmapCalled++;
        if (mmapFail) {
            return MAP_FAILED;
        }
        if (mmapRet) {
            return mmapRet + (off - mmapBase);
        } else {
            auto ret = new char[size];
            if (size > 0) {
                memset(ret, 0xaa, size);
            }
            return ret;
        }
    }

    int munmap(void *addr, size_t size) override {
        munmapCalled++;
        if (mmapRet) {
            return 0;
        }
        if (addr != MAP_FAILED && addr != 0) {
            if (*static_cast<char *>(addr) != static_cast<char>(0xaa)) {
                memoryModifiedInMunmap = true;
            }
            delete[] (char *)addr;
        } else {
            return -1;
        }
        return 0;
    }

    void setPreadMemory(char *memory, size_t size, uint64_t baseAddress) {
        pReadArrayRef = ArrayRef<char>(memory, size);
        pReadBase = baseAddress;
    }

    void setPwriteMemory(char *memory, size_t size, uint64_t baseAddress) {
        pWriteArrayRef = ArrayRef<char>(memory, size);
        pWriteBase = baseAddress;
    }

    int pollRetVal = 0;
    int passedTimeout = 0;
    ArrayRef<char> pReadArrayRef;
    uint64_t pReadBase = 0;
    int64_t preadCalled = 0;
    int64_t preadRetVal = 0;
    int64_t pread2RetVal = 0;

    ArrayRef<char> pWriteArrayRef;
    uint64_t pWriteBase = 0;
    int64_t pwriteCalled = 0;
    int64_t pwriteRetVal = 0;

    uint64_t preadOffset = 0;
    uint64_t pwriteOffset = 0;

    uint8_t midZeroReturn = 0;
    int64_t mmapCalled = 0;
    int64_t munmapCalled = 0;
    bool mmapFail = false;
    char *mmapRet = nullptr;
    uint64_t mmapBase = 0;
    bool memoryModifiedInMunmap = false;
    std::atomic<int> pollCounter = 0;
    EventQueue eventQueue;
};

} // namespace ult
} // namespace L0