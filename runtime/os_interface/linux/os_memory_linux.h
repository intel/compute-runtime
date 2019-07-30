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

class OSMemoryLinux : public OSMemory {
  public:
    OSMemoryLinux() = default;
    void *reserveCpuAddressRange(size_t sizeToReserve) override;
    void releaseCpuAddressRange(void *reservedCpuAddressRange, size_t reservedSize) override;

  protected:
    MOCKABLE_VIRTUAL void *mmapWrapper(void *, size_t, int, int, int, off_t);
    MOCKABLE_VIRTUAL int munmapWrapper(void *, size_t);
};

} // namespace NEO
