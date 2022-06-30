/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_memory.h"

#include "shared/source/helpers/aligned_memory.h"

namespace NEO {

OSMemory::ReservedCpuAddressRange OSMemory::reserveCpuAddressRange(size_t sizeToReserve, size_t alignment) {
    return reserveCpuAddressRange(0, sizeToReserve, alignment);
}

OSMemory::ReservedCpuAddressRange OSMemory::reserveCpuAddressRange(void *baseAddress, size_t sizeToReserve, size_t alignment) {
    UNRECOVERABLE_IF(alignment && 0 != (alignment & (alignment - 1)));

    ReservedCpuAddressRange reservedCpuAddressRange;

    reservedCpuAddressRange.sizeToReserve = sizeToReserve;
    reservedCpuAddressRange.actualReservedSize = sizeToReserve + alignment;
    reservedCpuAddressRange.originalPtr = this->osReserveCpuAddressRange(baseAddress, reservedCpuAddressRange.actualReservedSize, false);
    reservedCpuAddressRange.alignedPtr = alignUp(reservedCpuAddressRange.originalPtr, alignment);

    return reservedCpuAddressRange;
}

void OSMemory::releaseCpuAddressRange(const ReservedCpuAddressRange &reservedCpuAddressRange) {
    if (reservedCpuAddressRange.originalPtr != nullptr) {
        this->osReleaseCpuAddressRange(reservedCpuAddressRange.originalPtr, reservedCpuAddressRange.actualReservedSize);
    }
}

} // namespace NEO
