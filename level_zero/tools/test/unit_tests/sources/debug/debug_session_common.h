/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

#include "common/StateSaveAreaHeader.h"

namespace L0 {
namespace ult {

size_t threadSlotOffset(SIP::StateSaveAreaHeader *pStateSaveAreaHeader, int slice, int subslice, int eu, int thread);

size_t regOffsetInThreadSlot(const SIP::regset_desc *regdesc, uint32_t start);

void initStateSaveArea(std::vector<char> &stateSaveArea, SIP::version version);

} // namespace ult
} // namespace L0
