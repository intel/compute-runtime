/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/aub/aub_helper.h"
#include "runtime/aub_mem_dump/aub_mem_dump.h"
#include "runtime/helpers/basic_math.h"
#include "runtime/os_interface/debug_settings_manager.h"

namespace OCLRT {

int AubHelper::getMemTrace(uint64_t pdEntryBits) {
    return AubMemDump::AddressSpaceValues::TraceNonlocal;
}

uint64_t AubHelper::getPTEntryBits(uint64_t pdEntryBits) {
    return pdEntryBits;
}

void AubHelper::checkPTEAddress(uint64_t address) {
}

uint32_t AubHelper::getMemType(uint32_t addressSpace) {
    return 0;
}

uint64_t AubHelper::getMemBankSize() {
    return 2 * GB;
}

uint32_t AubHelper::getDevicesCount(const HardwareInfo *pHwInfo) {
    return DebugManager.flags.CreateMultipleDevices.get() > 0 ? DebugManager.flags.CreateMultipleDevices.get() : 1u;
}

} // namespace OCLRT
