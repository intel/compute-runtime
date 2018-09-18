/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "aub_mapper.h"
#include "runtime/aub_mem_dump/aub_mem_dump.inl"

namespace AubMemDump {

enum {
    device = DeviceValues::Bdw
};

// Instantiate these common template implementations.
template struct AubDump<Traits<device, 32>>;
template struct AubDump<Traits<device, 48>>;

template struct AubPageTableHelper32<Traits<device, 32>>;
template struct AubPageTableHelper64<Traits<device, 48>>;
} // namespace AubMemDump

namespace OCLRT {
using Family = BDWFamily;

static AubMemDump::LrcaHelperRcs rcs(0x000000);
static AubMemDump::LrcaHelperBcs bcs(0x020000);
static AubMemDump::LrcaHelperVcs vcs(0x010000);
static AubMemDump::LrcaHelperVecs vecs(0x018000);

const AubMemDump::LrcaHelper *AUBFamilyMapper<Family>::csTraits[EngineType::NUM_ENGINES] = {
    &rcs,
    &bcs,
    &vcs,
    &vecs};

const MMIOList AUBFamilyMapper<Family>::globalMMIO;

static const MMIOList mmioListRCS = {
    MMIOPair(0x000020d8, 0x00020000),
    MMIOPair(rcs.mmioBase + 0x229c, 0xffff8280),
};

static const MMIOList mmioListBCS = {
    MMIOPair(bcs.mmioBase + 0x229c, 0xffff8280),
};

static const MMIOList mmioListVCS = {
    MMIOPair(vcs.mmioBase + 0x229c, 0xffff8280),
};

static const MMIOList mmioListVECS = {
    MMIOPair(vecs.mmioBase + 0x229c, 0xffff8280),
};

const MMIOList *AUBFamilyMapper<Family>::perEngineMMIO[EngineType::NUM_ENGINES] = {
    &mmioListRCS,
    &mmioListBCS,
    &mmioListVCS,
    &mmioListVECS};
} // namespace OCLRT
