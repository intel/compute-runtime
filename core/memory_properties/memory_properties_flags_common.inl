/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

namespace NEO {

struct MemoryPropertiesFlagsBase {
    bool readWrite = false;
    bool writeOnly = false;
    bool readOnly = false;
    bool useHostPtr = false;
    bool allocHostPtr = false;
    bool copyHostPtr = false;

    bool hostWriteOnly = false;
    bool hostReadOnly = false;
    bool hostNoAccess = false;

    bool kernelReadAndWrite = false;

    bool forceLinearStorage = false;
    bool accessFlagsUnrestricted = false;
    bool noAccess = false;

    bool locallyUncachedResource = false;
    bool allowUnrestrictedSize = false;

    bool forceSharedPhysicalMemory = false;
};

} // namespace NEO
