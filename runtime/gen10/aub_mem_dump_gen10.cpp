/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "config.h"
#include "aub_mapper.h"
#include "runtime/aub_mem_dump/aub_mem_dump.inl"
#include "runtime/helpers/completion_stamp.h"

namespace AubMemDump {

enum {
    device = DeviceValues::Cnl
};

// Instantiate these common template implementations.
template struct AubDump<Traits<device, 32>>;
template struct AubDump<Traits<device, 48>>;

template struct AubPageTableHelper32<Traits<device, 32>>;
template struct AubPageTableHelper64<Traits<device, 48>>;
} // namespace AubMemDump

namespace OCLRT {
using Family = CNLFamily;

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
    MMIOPair(0x0000C800, 0x00000009),
    MMIOPair(0x0000C804, 0x00000038),
    MMIOPair(0x0000C808, 0x0000003B),
    MMIOPair(0x0000C80C, 0x00000039),
    MMIOPair(0x0000C810, 0x00000037),
    MMIOPair(0x0000C814, 0x00000039),
    MMIOPair(0x0000C818, 0x00000037),
    MMIOPair(0x0000C81C, 0x0000001B),
    MMIOPair(0x0000C820, 0x00060037),
    MMIOPair(0x0000C824, 0x00000032),
    MMIOPair(0x0000C828, 0x00000033),
    MMIOPair(0x0000C82C, 0x0000003B),
    MMIOPair(0x0000C8C0, 0x00000037),
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
