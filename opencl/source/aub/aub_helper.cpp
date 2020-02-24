/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/aub/aub_helper.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/basic_math.h"

#include "opencl/source/aub_mem_dump/aub_mem_dump.h"

namespace NEO {

uint64_t AubHelper::getTotalMemBankSize() {
    return 2 * GB;
}

int AubHelper::getMemTrace(uint64_t pdEntryBits) {
    return AubMemDump::AddressSpaceValues::TraceNonlocal;
}

uint64_t AubHelper::getPTEntryBits(uint64_t pdEntryBits) {
    return pdEntryBits;
}

uint32_t AubHelper::getMemType(uint32_t addressSpace) {
    return 0;
}

uint64_t AubHelper::getMemBankSize(const HardwareInfo *pHwInfo) {
    return getTotalMemBankSize();
}

void AubHelper::setAdditionalMmioList() {
}

} // namespace NEO
