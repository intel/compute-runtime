/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/os_memory_linux.h"

namespace NEO {

std::unique_ptr<OSMemory> OSMemory::create() {
    return std::make_unique<OSMemoryLinux>();
}

void *OSMemoryLinux::reserveCpuAddressRange(size_t sizeToReserve) {
    return mmapWrapper(0, sizeToReserve, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_HUGETLB, -1, 0);
}

void OSMemoryLinux::releaseCpuAddressRange(void *reservedCpuAddressRange, size_t reservedSize) {
    munmapWrapper(reservedCpuAddressRange, reservedSize);
}

void *OSMemoryLinux::mmapWrapper(void *addr, size_t size, int prot, int flags, int fd, off_t off) {
    return mmap(addr, size, prot, flags, fd, off);
}

int OSMemoryLinux::munmapWrapper(void *addr, size_t size) {
    return munmap(addr, size);
}

} // namespace NEO
