/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/os_memory.h"

#include <fcntl.h>
#include <sys/mman.h>

namespace NEO {

class OSMemoryLinux : public OSMemory {
  public:
    OSMemoryLinux() = default;

    void getMemoryMaps(MemoryMaps &memoryMaps) override;

  protected:
    void *osReserveCpuAddressRange(void *baseAddress, size_t sizeToReserve, bool topDownHint) override;
    void osReleaseCpuAddressRange(void *reservedCpuAddressRange, size_t reservedSize) override;

    MOCKABLE_VIRTUAL void *mmapWrapper(void *, size_t, int, int, int, off_t);
    MOCKABLE_VIRTUAL int munmapWrapper(void *, size_t);
};

} // namespace NEO
