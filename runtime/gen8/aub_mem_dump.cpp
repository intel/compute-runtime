/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
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
