/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub_mem_dump/aub_alloc_dump.inl"
#include "shared/source/aub_mem_dump/aub_mem_dump.inl"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/gen_common/reg_configs_common.h"
#include "shared/source/helpers/array_count.h"
#include "shared/source/helpers/completion_stamp.h"
#include "shared/source/helpers/gfx_core_helper.h"

#include "aub_mapper_common.h"
#include "config.h"

namespace AubMemDump {

// Instantiate these common template implementations.
template struct AubDump<Traits<32>>;
template struct AubDump<Traits<48>>;

template struct AubPageTableHelper32<Traits<32>>;
template struct AubPageTableHelper64<Traits<48>>;
} // namespace AubMemDump

namespace NEO {

static const AubMemDump::LrcaHelperRcs rcs(0x002000);
static const AubMemDump::LrcaHelperBcs bcs(0x022000);
static const AubMemDump::LrcaHelperVcs vcs(0x1c0000);
static const AubMemDump::LrcaHelperVecs vecs(0x1c8000);
static const AubMemDump::LrcaHelperCcs ccs(0x1a000);
static const AubMemDump::LrcaHelperCcs ccs1(0x1c000);
static const AubMemDump::LrcaHelperCcs ccs2(0x1e000);
static const AubMemDump::LrcaHelperCcs ccs3(0x26000);
static const AubMemDump::LrcaHelperCccs cccs(-1);
static const AubMemDump::LrcaHelperLinkBcs linkBcs1(-1, 1);
static const AubMemDump::LrcaHelperLinkBcs linkBcs2(-1, 2);
static const AubMemDump::LrcaHelperLinkBcs linkBcs3(-1, 3);
static const AubMemDump::LrcaHelperLinkBcs linkBcs4(-1, 4);
static const AubMemDump::LrcaHelperLinkBcs linkBcs5(-1, 5);
static const AubMemDump::LrcaHelperLinkBcs linkBcs6(-1, 6);
static const AubMemDump::LrcaHelperLinkBcs linkBcs7(-1, 7);
static const AubMemDump::LrcaHelperLinkBcs linkBcs8(-1, 8);

const AubMemDump::LrcaHelper *const AUBFamilyMapper<Family>::csTraits[aub_stream::NUM_ENGINES] = {
    &rcs,
    &bcs,
    &vcs,
    &vecs,
    &ccs,
    &ccs1,
    &ccs2,
    &ccs3,
    &cccs,
    &linkBcs1,
    &linkBcs2,
    &linkBcs3,
    &linkBcs4,
    &linkBcs5,
    &linkBcs6,
    &linkBcs7,
    &linkBcs8};

const MMIOList AUBFamilyMapper<Family>::globalMMIO = {
    // GLOBAL_MOCS
    MMIOPair(0x00004000, 0x00000008),
    MMIOPair(0x00004004, 0x00000038),
    MMIOPair(0x00004008, 0x00000038),
    MMIOPair(0x0000400C, 0x00000008),
    MMIOPair(0x00004010, 0x00000018),
    MMIOPair(0x00004014, 0x00060038),
    MMIOPair(0x00004018, 0x00000000),
    MMIOPair(0x0000401C, 0x00000033),
    MMIOPair(0x00004020, 0x00060037),
    MMIOPair(0x00004024, 0x0000003B),
    MMIOPair(0x00004028, 0x00000032),
    MMIOPair(0x0000402C, 0x00000036),
    MMIOPair(0x00004030, 0x0000003A),
    MMIOPair(0x00004034, 0x00000033),
    MMIOPair(0x00004038, 0x00000037),
    MMIOPair(0x0000403C, 0x0000003B),
    MMIOPair(0x00004040, 0x00000030),
    MMIOPair(0x00004044, 0x00000034),
    MMIOPair(0x00004048, 0x00000038),
    MMIOPair(0x0000404C, 0x00000031),
    MMIOPair(0x00004050, 0x00000032),
    MMIOPair(0x00004054, 0x00000036),
    MMIOPair(0x00004058, 0x0000003A),
    MMIOPair(0x0000405C, 0x00000033),
    MMIOPair(0x00004060, 0x00000037),
    MMIOPair(0x00004064, 0x0000003B),
    MMIOPair(0x00004068, 0x00000032),
    MMIOPair(0x0000406C, 0x00000036),
    MMIOPair(0x00004070, 0x0000003A),
    MMIOPair(0x00004074, 0x00000033),
    MMIOPair(0x00004078, 0x00000037),
    MMIOPair(0x0000407C, 0x0000003B),
    MMIOPair(0x00004080, 0x00000030),
    MMIOPair(0x00004084, 0x00000034),
    MMIOPair(0x00004088, 0x00000038),
    MMIOPair(0x0000408C, 0x00000031),
    MMIOPair(0x00004090, 0x00000032),
    MMIOPair(0x00004094, 0x00000036),
    MMIOPair(0x00004098, 0x0000003A),
    MMIOPair(0x0000409C, 0x00000033),
    MMIOPair(0x000040A0, 0x00000037),
    MMIOPair(0x000040A4, 0x0000003B),
    MMIOPair(0x000040A8, 0x00000032),
    MMIOPair(0x000040AC, 0x00000036),
    MMIOPair(0x000040B0, 0x0000003A),
    MMIOPair(0x000040B4, 0x00000033),
    MMIOPair(0x000040B8, 0x00000037),
    MMIOPair(0x000040BC, 0x0000003B),
    MMIOPair(0x000040C0, 0x00000038),
    MMIOPair(0x000040C4, 0x00000034),
    MMIOPair(0x000040C8, 0x00000038),
    MMIOPair(0x000040CC, 0x00000031),
    MMIOPair(0x000040D0, 0x00000032),
    MMIOPair(0x000040D4, 0x00000036),
    MMIOPair(0x000040D8, 0x0000003A),
    MMIOPair(0x000040DC, 0x00000033),
    MMIOPair(0x000040E0, 0x00000037),
    MMIOPair(0x000040E4, 0x0000003B),
    MMIOPair(0x000040E8, 0x00000032),
    MMIOPair(0x000040EC, 0x00000036),
    MMIOPair(0x000040F0, 0x00000038),
    MMIOPair(0x000040F4, 0x00000038),
    MMIOPair(0x000040F8, 0x00000038),
    MMIOPair(0x000040FC, 0x00000038),

    // LNCF_MOCS
    MMIOPair(0x0000B020, 0x00300010),
    MMIOPair(0x0000B024, 0x00300010),
    MMIOPair(0x0000B028, 0x00300030),
    MMIOPair(0x0000B02C, 0x00000000),
    MMIOPair(0x0000B030, 0x0030001F),
    MMIOPair(0x0000B034, 0x00170013),
    MMIOPair(0x0000B038, 0x0000001F),
    MMIOPair(0x0000B03C, 0x00000000),
    MMIOPair(0x0000B040, 0x00100000),
    MMIOPair(0x0000B044, 0x00170013),
    MMIOPair(0x0000B048, 0x0010001F),
    MMIOPair(0x0000B04C, 0x00170013),
    MMIOPair(0x0000B050, 0x0030001F),
    MMIOPair(0x0000B054, 0x00170013),
    MMIOPair(0x0000B058, 0x0000001F),
    MMIOPair(0x0000B05C, 0x00000000),
    MMIOPair(0x0000B060, 0x00100000),
    MMIOPair(0x0000B064, 0x00170013),
    MMIOPair(0x0000B068, 0x0010001F),
    MMIOPair(0x0000B06C, 0x00170013),
    MMIOPair(0x0000B070, 0x0030001F),
    MMIOPair(0x0000B074, 0x00170013),
    MMIOPair(0x0000B078, 0x0000001F),
    MMIOPair(0x0000B07C, 0x00000000),
    MMIOPair(0x0000B080, 0x00300030),
    MMIOPair(0x0000B084, 0x00170013),
    MMIOPair(0x0000B088, 0x0010001F),
    MMIOPair(0x0000B08C, 0x00170013),
    MMIOPair(0x0000B090, 0x0030001F),
    MMIOPair(0x0000B094, 0x00170013),
    MMIOPair(0x0000B098, 0x00300010),
    MMIOPair(0x0000B09C, 0x00300010),

    // PAT_INDEX
    MMIOPair(0x00004100, 0x0000000),
    MMIOPair(0x00004104, 0x0000000),
    MMIOPair(0x00004108, 0x0000000),
    MMIOPair(0x0000410c, 0x0000000),
    MMIOPair(0x00004110, 0x0000000),
    MMIOPair(0x00004114, 0x0000000),
    MMIOPair(0x00004118, 0x0000000),
    MMIOPair(0x0000411c, 0x0000000),

    MMIOPair(0x00004b80, 0xffff1001), // GACB_PERF_CTRL_REG
    MMIOPair(0x00007000, 0xffff0000), // CACHE_MODE_0
    MMIOPair(0x00007004, 0xffff0000), // CACHE_MODE_1
    MMIOPair(0x000043F8, 0x00000000), // Gen12 (A-step) chicken bit for AuxT granularity
    MMIOPair(0x00009008, 0x00000200), // IDICR
    MMIOPair(0x0000900c, 0x00001b40), // SNPCR
    MMIOPair(0x0000b120, 0x14000002), // LTCDREG
    MMIOPair(0x00042080, 0x00000000), // CHICKEN_MISC_1
    MMIOPair(0x000020D4, 0xFFFF0000), // Chicken bit for CSFE
    MMIOPair(0x0000B0A0, 0x00000000), // SCRATCH 2 for LNCF unit
    MMIOPair(0x000094D4, 0x00000000), // Slice unit Level Clock Gating Control

    // Capture Perf MMIO register programming
    MMIOPair(0x0000B004, 0x2FC0100B), // KM_ARBITER_CTRL_REG
    MMIOPair(0x0000B404, 0x00000160), // KM_GLOBAL_INVALIDATION_REG
    MMIOPair(0x00008708, 0x00000000), // KM_GEN12_IDI_CONTROL_REGISTER

    // Tiled Resources VA Translation Table L3 Pointer
    MMIOPair(0x00004410, 0xffffffff), // GEN12_TRTT_NULL_TILE_REG
    MMIOPair(0x00004414, 0xfffffffe), // GEN12_TRTT_INVD_TILE_REG
    MMIOPair(0x00004404, 0x000000ff), // GEN12_TRTT_VA_MASKDATA_REG
    MMIOPair(0x00004408, 0x00000000), // LDWORD GMM_GEN12_TRTT_L3_POINTER
    MMIOPair(0x0000440C, 0x00000000), // UDWORD GMM_GEN12_TRTT_L3_POINTER
    MMIOPair(0x00004400, 0x00000001), // GEN12_TRTT_TABLE_CONTROL
    MMIOPair(0x00004DFC, 0x00000000), // GEN9_TR_CHICKEN_BIT_VECTOR
};

static const MMIOList mmioListRCS = {
    MMIOPair(AubMemDump::computeRegisterOffset(rcs.mmioBase, 0x00002058), 0x00000000), // CTX_WA_PTR_RCSUNIT
    MMIOPair(AubMemDump::computeRegisterOffset(rcs.mmioBase, 0x000020a8), 0x00000000), // IMR
    MMIOPair(AubMemDump::computeRegisterOffset(rcs.mmioBase, 0x0000229c), 0xffff8280), // GFX_MODE

    MMIOPair(0x00002090, 0xffff0000), // CHICKEN_PWR_CTX_RASTER_1
    MMIOPair(0x000020e0, 0xffff4000), // FF_SLICE_CS_CHICKEN1_RCSUNIT
    MMIOPair(0x000020e4, 0xffff0000), // FF_SLICE_CS_CHICKEN2_RCSUNIT
    MMIOPair(0x000020ec, 0xffff0051), // CS_DEBUG_MODE1

    // FORCE_TO_NONPRIV
    MMIOPair(AubMemDump::computeRegisterOffset(rcs.mmioBase, 0x000024d0), 0x00007014),
    MMIOPair(AubMemDump::computeRegisterOffset(rcs.mmioBase, 0x000024d4), 0x0000e000),
    MMIOPair(AubMemDump::computeRegisterOffset(rcs.mmioBase, 0x000024d8), 0x0000e000),
    MMIOPair(AubMemDump::computeRegisterOffset(rcs.mmioBase, 0x000024dc), 0x0000e000),
    MMIOPair(AubMemDump::computeRegisterOffset(rcs.mmioBase, 0x000024e0), 0x0000e000),
    MMIOPair(AubMemDump::computeRegisterOffset(rcs.mmioBase, 0x000024e4), 0x0000e000),
    MMIOPair(AubMemDump::computeRegisterOffset(rcs.mmioBase, 0x000024e8), 0x0000e000),
    MMIOPair(AubMemDump::computeRegisterOffset(rcs.mmioBase, 0x000024ec), 0x0000e000),
    MMIOPair(AubMemDump::computeRegisterOffset(rcs.mmioBase, 0x000024f0), 0x0000e000),
    MMIOPair(AubMemDump::computeRegisterOffset(rcs.mmioBase, 0x000024f4), 0x0000e000),
    MMIOPair(AubMemDump::computeRegisterOffset(rcs.mmioBase, 0x000024f8), 0x0000e000),
    MMIOPair(AubMemDump::computeRegisterOffset(rcs.mmioBase, 0x000024fc), 0x0000e000),

    MMIOPair(0x00002580, 0xffff0005), // CS_CHICKEN1
    MMIOPair(0x0000e194, 0xffff0002), // CHICKEN_SAMPLER_2

    MMIOPair(0x0000B134, 0xA0000000) // L3ALLOCREG
};

static const MMIOList mmioListBCS = {
    MMIOPair(AubMemDump::computeRegisterOffset(bcs.mmioBase, 0x0000229c), 0xffff8280), // GFX_MODE
};

static const MMIOList mmioListVCS = {
    MMIOPair(AubMemDump::computeRegisterOffset(vcs.mmioBase, 0x0000229c), 0xffff8280), // GFX_MODE
};

static const MMIOList mmioListVECS = {
    MMIOPair(AubMemDump::computeRegisterOffset(vecs.mmioBase, 0x0000229c), 0xffff8280), // GFX_MODE
};

static MMIOList mmioListCCSInstance(uint32_t mmioBase) {
    MMIOList mmioList;

    mmioList.push_back(MMIOPair(0x0000ce90, 0x00030003));                                              // GFX_MULT_CTXT_CTL - enable multi-context with 4CCS
    mmioList.push_back(MMIOPair(0x0000b170, 0x00030003));                                              // MULT_CTXT_CTL - enable multi-context with 4CCS
    mmioList.push_back(MMIOPair(0x00014800, 0xFFFF0001));                                              // RCU_MODE
    mmioList.push_back(MMIOPair(AubMemDump::computeRegisterOffset(mmioBase, 0x0000229c), 0xffff8280)); // GFX_MODE

    // FORCE_TO_NONPRIV
    mmioList.push_back(MMIOPair(AubMemDump::computeRegisterOffset(mmioBase, 0x000024d0), 0x0000e000));
    mmioList.push_back(MMIOPair(AubMemDump::computeRegisterOffset(mmioBase, 0x000024d4), 0x0000e000));
    mmioList.push_back(MMIOPair(AubMemDump::computeRegisterOffset(mmioBase, 0x000024d8), 0x0000e000));
    mmioList.push_back(MMIOPair(AubMemDump::computeRegisterOffset(mmioBase, 0x000024dc), 0x0000e000));
    mmioList.push_back(MMIOPair(AubMemDump::computeRegisterOffset(mmioBase, 0x000024e0), 0x0000e000));
    mmioList.push_back(MMIOPair(AubMemDump::computeRegisterOffset(mmioBase, 0x000024e4), 0x0000e000));
    mmioList.push_back(MMIOPair(AubMemDump::computeRegisterOffset(mmioBase, 0x000024e8), 0x0000e000));
    mmioList.push_back(MMIOPair(AubMemDump::computeRegisterOffset(mmioBase, 0x000024ec), 0x0000e000));
    mmioList.push_back(MMIOPair(AubMemDump::computeRegisterOffset(mmioBase, 0x000024f0), 0x0000e000));
    mmioList.push_back(MMIOPair(AubMemDump::computeRegisterOffset(mmioBase, 0x000024f4), 0x0000e000));
    mmioList.push_back(MMIOPair(AubMemDump::computeRegisterOffset(mmioBase, 0x000024f8), 0x0000e000));
    mmioList.push_back(MMIOPair(AubMemDump::computeRegisterOffset(mmioBase, 0x000024fc), 0x0000e000));

    mmioList.push_back(MMIOPair(0x0000B234, 0xA0000000)); // L3ALLOCREG_CCS0

    return mmioList;
};

static const MMIOList mmioListCCS = mmioListCCSInstance(ccs.mmioBase);
static const MMIOList mmioListCCS1 = mmioListCCSInstance(ccs1.mmioBase);
static const MMIOList mmioListCCS2 = mmioListCCSInstance(ccs2.mmioBase);
static const MMIOList mmioListCCS3 = mmioListCCSInstance(ccs3.mmioBase);
static const MMIOList mmioListCCCS = {};
static const MMIOList mmioListLinkBCS = {};

const MMIOList *AUBFamilyMapper<Family>::perEngineMMIO[aub_stream::NUM_ENGINES] = {
    &mmioListRCS,
    &mmioListBCS,
    &mmioListVCS,
    &mmioListVECS,
    &mmioListCCS,
    &mmioListCCS1,
    &mmioListCCS2,
    &mmioListCCS3,
    &mmioListCCCS,
    &mmioListLinkBCS,
    &mmioListLinkBCS,
    &mmioListLinkBCS,
    &mmioListLinkBCS,
    &mmioListLinkBCS,
    &mmioListLinkBCS,
    &mmioListLinkBCS,
    &mmioListLinkBCS};
} // namespace NEO

namespace AubAllocDump {
using namespace NEO;

template SurfaceInfo *getDumpSurfaceInfo<Family>(GraphicsAllocation &gfxAllocation, const GmmHelper &gmmHelper, DumpFormat dumpFormat);

template uint32_t getImageSurfaceTypeFromGmmResourceType<Family>(GMM_RESOURCE_TYPE gmmResourceType);
} // namespace AubAllocDump
