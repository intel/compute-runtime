/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/unit_tests/sources/debug/debug_session_common.h"

#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/test/common/helpers/default_hw_info.h"
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

void initStateSaveArea(std::vector<char> &stateSaveArea, SIP::version version, L0::Device *device) {
    auto stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(version.major);
    auto pStateSaveAreaHeader = reinterpret_cast<SIP::StateSaveAreaHeader *>(stateSaveAreaHeader.data());

    auto stateSaveSize = pStateSaveAreaHeader->regHeader.num_subslices_per_slice * pStateSaveAreaHeader->regHeader.num_eus_per_subslice * pStateSaveAreaHeader->regHeader.num_threads_per_eu * pStateSaveAreaHeader->regHeader.state_save_size;

    stateSaveArea.resize(threadSlotOffset(pStateSaveAreaHeader, 0, 0, 0, 0) + stateSaveSize);

    memcpy_s(stateSaveArea.data(), stateSaveArea.size(), pStateSaveAreaHeader, stateSaveAreaHeader.size());

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
        const uint32_t numSlices = pStateSaveAreaHeader->regHeader.num_slices;
        const uint32_t numSubslicesPerSlice = pStateSaveAreaHeader->regHeader.num_subslices_per_slice;
        const uint32_t numEusPerSubslice = pStateSaveAreaHeader->regHeader.num_eus_per_subslice;
        const uint32_t numThreadsPerEu = pStateSaveAreaHeader->regHeader.num_threads_per_eu;

        const uint32_t midSlice = (numSlices > 1) ? (numSlices / 2) : 0;
        const uint32_t midSubslice = (numSubslicesPerSlice > 1) ? (numSubslicesPerSlice / 2) : 0;
        const uint32_t midEu = (numEusPerSubslice > 1) ? (numEusPerSubslice / 2) : 0;
        const uint32_t midThread = (numThreadsPerEu > 1) ? (numThreadsPerEu / 2) : 0;

        // grfs for an eu thread that is somewhere in the middle
        fillRegsetForThread(&pStateSaveAreaHeader->regHeader.grf, midSlice, midSubslice, midEu, midThread, 'a');

        // grfs for the very last eu thread
        fillRegsetForThread(&pStateSaveAreaHeader->regHeader.grf, numSlices - 1, numSubslicesPerSlice - 1, numEusPerSubslice - 1, numThreadsPerEu - 1, 'a');
    }
}

} // namespace ult
} // namespace L0
