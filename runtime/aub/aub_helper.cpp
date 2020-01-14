/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/aub/aub_helper.h"

#include "core/debug_settings/debug_settings_manager.h"
#include "core/helpers/basic_math.h"
#include "runtime/aub_mem_dump/aub_mem_dump.h"

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
