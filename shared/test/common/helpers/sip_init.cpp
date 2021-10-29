/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/memory_manager/os_agnostic_memory_manager.h"
#include "shared/test/common/mocks/mock_sip.h"

#include "common/StateSaveAreaHeader.h"

#include <cassert>

namespace NEO {

namespace MockSipData {
std::unique_ptr<MockSipKernel> mockSipKernel;
SipKernelType calledType = SipKernelType::COUNT;
bool called = false;
bool returned = true;
bool useMockSip = false;

void clearUseFlags() {
    calledType = SipKernelType::COUNT;
    called = false;
}

std::vector<char> createStateSaveAreaHeader(uint32_t version) {
    SIP::StateSaveAreaHeader stateSaveAreaHeader = {
        {
            // versionHeader
            "tssarea", // magic
            0,         // reserved1
            {          // version
             1,        // major
             0,        // minor
             0},       // patch
            40,        // size
            {0, 0, 0}, // reserved2
        },
        {
            // regHeader
            1,                   // num_slices
            6,                   // num_subslices_per_slice
            16,                  // num_eus_per_subslice
            7,                   // num_threads_per_eu
            0,                   // state_area_offset
            6144,                // state_save_size
            0,                   // slm_area_offset
            0,                   // slm_bank_size
            0,                   // slm_bank_valid
            4740,                // sr_magic_offset
            {0, 128, 256, 32},   // grf
            {4096, 1, 256, 32},  // addr
            {4128, 2, 32, 4},    // flag
            {4156, 1, 32, 4},    // emask
            {4160, 2, 128, 16},  // sr
            {4192, 1, 128, 16},  // cr
            {4256, 1, 96, 12},   // notification
            {4288, 1, 128, 16},  // tdr
            {4320, 10, 256, 32}, // acc
            {0, 0, 0, 0},        // mme
            {4672, 1, 32, 4},    // ce
            {4704, 1, 128, 16},  // sp
            {0, 0, 0, 0},        // cmd
            {4640, 1, 128, 16},  // tm
            {0, 0, 0, 0},        // fc
            {4736, 1, 32, 4},    // dbg
        },
    };

    SIP::StateSaveAreaHeader stateSaveAreaHeader2 = {
        {
            // versionHeader
            "tssarea", // magic
            0,         // reserved1
            {          // version
             2,        // major
             0,        // minor
             0},       // patch
            40,        // size
            {0, 0, 0}, // reserved2
        },
        {
            // regHeader
            1,                       // num_slices
            1,                       // num_subslices_per_slice
            8,                       // num_eus_per_subslice
            7,                       // num_threads_per_eu
            0,                       // state_area_offset
            6144,                    // state_save_size
            0,                       // slm_area_offset
            0,                       // slm_bank_size
            0,                       // slm_bank_valid
            4740,                    // sr_magic_offset
            {0, 128, 256, 32},       // grf
            {4096, 1, 256, 32},      // addr
            {4128, 2, 32, 4},        // flag
            {4156, 1, 32, 4},        // emask
            {4160, 2, 128, 16},      // sr
            {4192, 1, 128, 16},      // cr
            {4256, 1, 96, 12},       // notification
            {4288, 1, 128, 16},      // tdr
            {4320, 10, 256, 32},     // acc
            {0, 0, 0, 0},            // mme
            {4672, 1, 32, 4},        // ce
            {4704, 1, 128, 16},      // sp
            {4768, 1, 128 * 8, 128}, // cmd
            {4640, 1, 128, 16},      // tm
            {0, 0, 0, 0},            // fc
            {4736, 1, 32, 4},        // dbg
        },
    };

    char *begin = nullptr;

    if (version == 1) {
        begin = reinterpret_cast<char *>(&stateSaveAreaHeader);
    } else if (version == 2) {
        begin = reinterpret_cast<char *>(&stateSaveAreaHeader2);
    }
    return std::vector<char>(begin, begin + sizeof(stateSaveAreaHeader));
}
} // namespace MockSipData

bool SipKernel::initSipKernel(SipKernelType type, Device &device) {
    if (MockSipData::useMockSip) {
        auto &hwHelper = HwHelper::get(device.getRootDeviceEnvironment().getHardwareInfo()->platform.eRenderCoreFamily);
        if (hwHelper.isSipKernelAsHexadecimalArrayPreferred()) {
            SipKernel::classType = SipClassType::HexadecimalHeaderFile;
        } else {
            SipKernel::classType = SipClassType::Builtins;
        }
        MockSipData::calledType = type;
        MockSipData::called = true;

        MockSipData::mockSipKernel->mockSipMemoryAllocation->clearUsageInfo();
        return MockSipData::returned;
    } else {
        return SipKernel::initSipKernelImpl(type, device);
    }
}

const SipKernel &SipKernel::getSipKernel(Device &device) {
    if (MockSipData::useMockSip) {
        return *MockSipData::mockSipKernel.get();
    } else {
        auto sipType = SipKernel::getSipKernelType(device);
        SipKernel::initSipKernel(sipType, device);
        return SipKernel::getSipKernelImpl(device);
    }
}

} // namespace NEO
