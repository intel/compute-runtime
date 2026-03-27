/*
 * Copyright (C) 2019-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/os_memory_linux.h"

#include "shared/source/os_interface/linux/os_inc.h"
#include "shared/source/os_interface/linux/sys_calls.h"

#include <cinttypes>
#include <sstream>
#include <string>

namespace NEO {

std::unique_ptr<OSMemory> OSMemory::create() {
    return std::make_unique<OSMemoryLinux>();
}

void *OSMemoryLinux::osReserveCpuAddressRange(void *baseAddress, size_t sizeToReserve, bool topDownHint) {
    return mmapWrapper(baseAddress, sizeToReserve, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
}

void OSMemoryLinux::osReleaseCpuAddressRange(void *reservedCpuAddressRange, size_t reservedSize) {
    munmapWrapper(reservedCpuAddressRange, reservedSize);
}

void *OSMemoryLinux::mmapWrapper(void *addr, size_t size, int prot, int flags, int fd, off_t off) {
    return SysCalls::mmap(addr, size, prot, flags, fd, off);
}

int OSMemoryLinux::munmapWrapper(void *addr, size_t size) {
    return SysCalls::munmap(addr, size);
}

void OSMemoryLinux::getMemoryMaps(MemoryMaps &memoryMaps) {

    /*
     ============ ===============================================================

     The /proc/PID/maps file contains the currently mapped memory regions and
     their access permissions.

     The format is::

     address           perms offset  dev   inode      pathname

     08048000-08049000 r-xp 00000000 03:00 8312       /opt/test

    */

    auto fd = SysCalls::open((std::string(Os::sysFsProcPathPrefix) + "/self/maps").c_str(), O_RDONLY);
    if (fd < 0) {
        return;
    }

    constexpr size_t chunkSize = 1024 * 1024;
    std::string content;
    off_t offset = 0;

    while (true) {
        content.resize(static_cast<size_t>(offset) + chunkSize);
        auto bytesRead = SysCalls::pread(fd, content.data() + offset, chunkSize, offset);
        if (bytesRead <= 0) {
            content.resize(static_cast<size_t>(offset));
            break;
        }
        offset += static_cast<off_t>(bytesRead);
        content.resize(static_cast<size_t>(offset));
        if (static_cast<size_t>(bytesRead) < chunkSize) {
            break;
        }
    }

    SysCalls::close(fd);

    if (content.empty()) {
        return;
    }

    std::istringstream iss(content);
    parseMemoryMaps(iss, memoryMaps);
}

void OSMemoryLinux::parseMemoryMaps(std::istream &stream, MemoryMaps &memoryMaps) {
    std::string line;
    while (std::getline(stream, line)) {
        uint64_t start = 0, end = 0;
        sscanf(line.c_str(), "%" SCNx64 "-%" SCNx64, &start, &end);
        memoryMaps.push_back({start, end});
    }
}

} // namespace NEO
