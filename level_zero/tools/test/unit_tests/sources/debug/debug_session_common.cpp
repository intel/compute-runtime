/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/unit_tests/sources/debug/debug_session_common.h"

#include "shared/test/common/mocks/mock_sip.h"

namespace L0 {
namespace ult {

size_t threadSlotOffset(SIP::StateSaveAreaHeader *pStateSaveAreaHeader, int slice, int subslice, int eu, int thread) {
    return pStateSaveAreaHeader->versionHeader.size * 8 +
           pStateSaveAreaHeader->regHeader.state_area_offset +
           ((((slice * pStateSaveAreaHeader->regHeader.num_subslices_per_slice + subslice) * pStateSaveAreaHeader->regHeader.num_eus_per_subslice + eu) * pStateSaveAreaHeader->regHeader.num_threads_per_eu + thread) * pStateSaveAreaHeader->regHeader.state_save_size);
};

size_t regOffsetInThreadSlot(const SIP::regset_desc *regdesc, uint32_t start) {
    return regdesc->offset + regdesc->bytes * start;
};

void initStateSaveArea(std::vector<char> &stateSaveArea, SIP::version version) {
    auto stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(version.major);
    auto pStateSaveAreaHeader = reinterpret_cast<SIP::StateSaveAreaHeader *>(stateSaveAreaHeader.data());

    if (version.major >= 2) {
        stateSaveArea.resize(
            threadSlotOffset(pStateSaveAreaHeader, 0, 0, 0, 0) +
            pStateSaveAreaHeader->regHeader.num_subslices_per_slice * pStateSaveAreaHeader->regHeader.num_eus_per_subslice * pStateSaveAreaHeader->regHeader.num_threads_per_eu * pStateSaveAreaHeader->regHeader.state_save_size);
    } else {
        auto &hwInfo = *NEO::defaultHwInfo.get();
        auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
        stateSaveArea.resize(hwHelper.getSipKernelMaxDbgSurfaceSize(hwInfo) + MemoryConstants::pageSize);
    }

    memcpy(stateSaveArea.data(), pStateSaveAreaHeader, sizeof(*pStateSaveAreaHeader));

    auto fillRegForThread = [&](const SIP::regset_desc *regdesc, int slice, int subslice, int eu, int thread, int start, char value) {
        memset(stateSaveArea.data() + threadSlotOffset(pStateSaveAreaHeader, slice, subslice, eu, thread) + regOffsetInThreadSlot(regdesc, start), value, regdesc->bytes);
    };

    auto fillRegsetForThread = [&](const SIP::regset_desc *regdesc, int slice, int subslice, int eu, int thread, char value) {
        for (uint32_t reg = 0; reg < regdesc->num; ++reg) {
            fillRegForThread(regdesc, slice, subslice, eu, thread, reg, value + reg);
        }
        SIP::sr_ident *srIdent = reinterpret_cast<SIP::sr_ident *>(stateSaveArea.data() + threadSlotOffset(pStateSaveAreaHeader, slice, subslice, eu, thread) + pStateSaveAreaHeader->regHeader.sr_magic_offset);
        srIdent->count = 2;
        srIdent->version.major = version.major;
        srIdent->version.minor = version.minor;
        srIdent->version.patch = version.patch;
        strcpy(stateSaveArea.data() + threadSlotOffset(pStateSaveAreaHeader, slice, subslice, eu, thread) + pStateSaveAreaHeader->regHeader.sr_magic_offset, "srmagic"); // NOLINT(clang-analyzer-security.insecureAPI.strcpy)
    };

    // grfs for 0/0/0/0 - very first eu thread
    fillRegsetForThread(&pStateSaveAreaHeader->regHeader.grf, 0, 0, 0, 0, 'a');

    // grfs for 0/0/4/0 - requred to test resumeWA
    fillRegsetForThread(&pStateSaveAreaHeader->regHeader.grf, 0, 0, 4, 0, 'a');

    if (version.major < 2) {
        // grfs for 0/3/7/3 - somewhere in the middle
        fillRegsetForThread(&pStateSaveAreaHeader->regHeader.grf, 0, 3, 7, 3, 'a');

        // grfs for 0/5/15/6 - very last eu thread
        fillRegsetForThread(&pStateSaveAreaHeader->regHeader.grf, 0, 5, 15, 6, 'a');
    }
}

} // namespace ult
} // namespace L0
