/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/os_interface/windows/shared_section.h"

#include <cstring>

namespace NEO {
class MockSharedSection : public SharedSection {
  public:
    using SharedSection::gmmParams;
    using SharedSection::usableSize;

    explicit MockSharedSection(void *view) : SharedSection(view) {}
};

inline void writeSharedSectionTail(void *section, size_t sectionSize, const GMM_RESCREATE_PARAMS &gmmParams, uint32_t securityDescriptorSize) {
    SharedSection::SharedMemoryContentTail tail = {};
    memcpy(&tail.gmmParams, &gmmParams, sizeof(gmmParams));
    tail.serializedSecurityDescriptorStringSize = securityDescriptorSize;
    memcpy(ptrOffset(section, sectionSize - sizeof(tail)), &tail, sizeof(tail));
}
} // namespace NEO
