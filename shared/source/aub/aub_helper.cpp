/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub/aub_helper.h"

#include "shared/source/aub_mem_dump/aub_mem_dump.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/basic_math.h"

#include "third_party/aub_stream/headers/aubstream.h"

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

void AubHelper::setTbxConfiguration() {
    aub_stream::setTbxServerIp(DebugManager.flags.TbxServer.get());
    aub_stream::setTbxServerPort(DebugManager.flags.TbxPort.get());
    aub_stream::setTbxFrontdoorMode(DebugManager.flags.TbxFrontdoorMode.get());
}
} // namespace NEO
