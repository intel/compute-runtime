/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/aub/aub_helper.h"
#include "runtime/aub_mem_dump/aub_mem_dump.h"

namespace OCLRT {

int AubHelper::getMemTrace(uint64_t pdEntryBits) {
    return AubMemDump::AddressSpaceValues::TraceNonlocal;
}

uint64_t AubHelper::getPTEntryBits(uint64_t pdEntryBits) {
    return pdEntryBits;
}

void AubHelper::checkPTEAddress(uint64_t address) {
}

} // namespace OCLRT
