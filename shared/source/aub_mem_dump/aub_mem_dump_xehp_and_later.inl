/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub_mem_dump/aub_alloc_dump.inl"
#include "shared/source/aub_mem_dump/aub_mem_dump.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/gen_common/reg_configs_common.h"
#include "shared/source/helpers/array_count.h"
#include "shared/source/helpers/completion_stamp.h"
#include "shared/source/helpers/gfx_core_helper.h"

#include "aub_mapper_common.h"
#include "config.h"

namespace NEO {

static const AubMemDump::LrcaHelperRcs rcs(0x002000);
static const AubMemDump::LrcaHelperBcs bcs(0x022000);
static const AubMemDump::LrcaHelperVcs vcs(0x1c0000);
static const AubMemDump::LrcaHelperVecs vecs(0x1c8000);
static const AubMemDump::LrcaHelperCcs ccs(0x1a000);
static const AubMemDump::LrcaHelperCcs ccs1(0x1c000);
static const AubMemDump::LrcaHelperCcs ccs2(0x1e000);
static const AubMemDump::LrcaHelperCcs ccs3(0x26000);

const AubMemDump::LrcaHelper *const AUBFamilyMapper<Family>::csTraits[aub_stream::NUM_ENGINES] = {
    &rcs,
    &bcs,
    &vcs,
    &vecs,
    &ccs,
    &ccs1,
    &ccs2,
    &ccs3};

} // namespace NEO

namespace AubAllocDump {
using namespace NEO;

template SurfaceInfo *getDumpSurfaceInfo<Family>(GraphicsAllocation &gfxAllocation, const GmmHelper &gmmHelper, DumpFormat dumpFormat);

template uint32_t getImageSurfaceTypeFromGmmResourceType<Family>(GMM_RESOURCE_TYPE gmmResourceType);
} // namespace AubAllocDump
