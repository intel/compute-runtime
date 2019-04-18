/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/os_memory.h"

#include <fcntl.h>
#include <sys/mman.h>

namespace NEO {

void *OSMemory::reserveCpuAddressRange(size_t sizeToReserve) {
    return mmap(0, sizeToReserve, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_HUGETLB, open("/dev/null", O_RDONLY), 0);
}

void OSMemory::releaseCpuAddressRange(void *reservedCpuAddressRange, size_t reservedSize) {
    munmap(reservedCpuAddressRange, reservedSize);
}

} // namespace NEO
