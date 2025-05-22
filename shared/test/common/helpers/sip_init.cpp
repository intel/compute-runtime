/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debugger/debugger_l0.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/memory_manager/memory_allocation.h"
#include "shared/test/common/mocks/mock_sip.h"

#include "common/StateSaveAreaHeader.h"

#include <cassert>

namespace NEO {

namespace MockSipData {
std::unique_ptr<MockSipKernel> mockSipKernel;
SipKernelType calledType = SipKernelType::count;
bool called = false;
bool returned = true;
bool useMockSip = false;
bool uninitializedSipRequested = false;
uint64_t totalWmtpDataSize = 32 * MemoryConstants::megaByte;

void clearUseFlags() {
    calledType = SipKernelType::count;
    called = false;
}

std::vector<char> createStateSaveAreaHeader(uint32_t version) {
    return createStateSaveAreaHeader(version, 128);
}

std::vector<char> createStateSaveAreaHeader(uint32_t version, uint16_t grfNum) {
    return createStateSaveAreaHeader(version, grfNum, 1);
}

std::vector<char> createStateSaveAreaHeader(uint32_t version, uint16_t grfNum, uint16_t mmeNum) {
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
            1,                    // num_slices
            2,                    // num_subslices_per_slice
            16,                   // num_eus_per_subslice
            7,                    // num_threads_per_eu
            0,                    // state_area_offset
            6144,                 // state_save_size
            0,                    // slm_area_offset
            0,                    // slm_bank_size
            0,                    // slm_bank_valid
            4740,                 // sr_magic_offset
            {0, grfNum, 256, 32}, // grf
            {4096, 1, 256, 32},   // addr
            {4128, 2, 32, 4},     // flag
            {4156, 1, 32, 4},     // emask
            {4160, 2, 128, 16},   // sr
            {4192, 1, 128, 16},   // cr
            {4256, 1, 96, 12},    // notification
            {4288, 1, 128, 16},   // tdr
            {4320, 10, 256, 32},  // acc
            {4320, 10, 256, 32},  // mme
            {4672, 1, 32, 4},     // ce
            {4704, 1, 128, 16},   // sp
            {0, 0, 0, 0},         // cmd
            {4640, 1, 128, 16},   // tm
            {0, 1, 32, 4},        // fc
            {4736, 1, 32, 4},     // dbg
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
            44,        // size
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
            {0, grfNum, 256, 32},    // grf
            {4096, 1, 256, 32},      // addr
            {4128, 2, 32, 4},        // flag
            {4156, 1, 32, 4},        // emask
            {4160, 2, 128, 16},      // sr
            {4192, 1, 128, 16},      // cr
            {4256, 1, 96, 12},       // notification
            {4288, 1, 128, 16},      // tdr
            {4320, 10, 256, 32},     // acc
            {4320, mmeNum, 256, 32}, // mme
            {4672, 1, 32, 4},        // ce
            {4704, 1, 128, 16},      // sp
            {4768, 1, 128 * 8, 128}, // cmd
            {4640, 1, 128, 16},      // tm
            {0, 1, 32, 4},           // fc
            {4736, 1, 32, 4},        // dbg
            {4744, 1, 64, 8},        // ctx
            {4752, 1, 64, 8},        // dbg_reg
        },
    };

    SIP::StateSaveArea versionHeader = {
        // versionHeader
        "tssarea", // magic
        0,         // reserved1
        {          // version
         3,        // major
         0,        // minor
         0},       // patch
        53,        // size
        {0, 0, 0}, // reserved2
    };
    SIP::intelgt_state_save_area_V3 regHeaderV3 = {
        // regHeader
        1,                              // num_slices
        1,                              // num_subslices_per_slice
        8,                              // num_eus_per_subslice
        7,                              // num_threads_per_eu
        0,                              // state_area_offset
        6144,                           // state_save_size
        0,                              // slm_area_offset
        0,                              // slm_bank_size
        0,                              // reserved0
        4740,                           // sr_magic_offset
        0,                              // fifo_offset;
        100,                            // fifo_size;
        0,                              // fifo_head;
        0,                              // fifo_tail;
        0,                              // fifo_version;
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // reserved1[10];
        SIP::SIP_FLAG_HEAPLESS,         // sip_flags;
        {0, grfNum, 512, 64},           // grf
        {4096, 1, 256, 32},             // addr
        {4128, 2, 32, 4},               // flag
        {4156, 1, 32, 4},               // emask
        {4160, 2, 160, 20},             // sr
        {4192, 1, 128, 16},             // cr
        {4256, 1, 96, 12},              // notification
        {4288, 1, 128, 16},             // tdr
        {4320, 10, 256, 32},            // acc
        {4320, mmeNum, 256, 32},        // mme
        {4672, 1, 32, 4},               // ce
        {4704, 1, 128, 16},             // sp
        {4768, 1, 128 * 8, 128},        // cmd
        {4640, 1, 128, 16},             // tm
        {0, 1, 32, 4},                  // fc
        {4736, 1, 32, 4},               // dbg
        {4744, 1, 64, 8},               // ctx
        {4752, 1, 64, 8},               // dbg_reg
        {4760, 1, 64, 8},               // scalar
        {4768, 1, 64, 8},               // msg
    };
    NEO::StateSaveAreaHeader stateSaveAreaHeader3 = {};
    stateSaveAreaHeader3.versionHeader = versionHeader;
    stateSaveAreaHeader3.regHeaderV3 = regHeaderV3;

    NEO::StateSaveAreaHeader stateSaveAreaHeader4 = {
        {
            // versionHeader
            "tssarea", // magic
            0,         // reserved1
            {          // version
             4,        // major
             0,        // minor
             0},       // patch
            8,         // size
            {0, 0, 0}, // reserved2
        }};
    stateSaveAreaHeader4.totalWmtpDataSize = totalWmtpDataSize;

    char *begin = nullptr;
    unsigned long sizeOfHeader = 0u;
    if (version == 1) {
        begin = reinterpret_cast<char *>(&stateSaveAreaHeader);
        sizeOfHeader = offsetof(SIP::StateSaveAreaHeader, regHeader.dbg) + sizeof(SIP::StateSaveAreaHeader::regHeader.dbg);
    } else if (version == 2) {
        begin = reinterpret_cast<char *>(&stateSaveAreaHeader2);
        sizeOfHeader = offsetof(SIP::StateSaveAreaHeader, regHeader.dbg_reg) + sizeof(SIP::StateSaveAreaHeader::regHeader.dbg_reg);
    } else if (version == 3) {
        stateSaveAreaHeader3.versionHeader.size = sizeof(stateSaveAreaHeader3) / 8;
        begin = reinterpret_cast<char *>(&stateSaveAreaHeader3);
        stateSaveAreaHeader3.regHeaderV3.fifo_size = 56;

        stateSaveAreaHeader3.regHeaderV3.fifo_offset = (stateSaveAreaHeader3.regHeaderV3.num_slices *
                                                        stateSaveAreaHeader3.regHeaderV3.num_subslices_per_slice *
                                                        stateSaveAreaHeader3.regHeaderV3.num_eus_per_subslice *
                                                        stateSaveAreaHeader3.regHeaderV3.num_threads_per_eu *
                                                        stateSaveAreaHeader3.regHeaderV3.state_save_size);
        sizeOfHeader = offsetof(NEO::StateSaveAreaHeader, regHeaderV3.msg) + sizeof(NEO::StateSaveAreaHeader::regHeaderV3.msg);
    } else if (version == 4) {
        begin = reinterpret_cast<char *>(&stateSaveAreaHeader4);
        sizeOfHeader = offsetof(NEO::StateSaveAreaHeader, totalWmtpDataSize) + sizeof(stateSaveAreaHeader4.totalWmtpDataSize);
    }

    return std::vector<char>(begin, begin + sizeOfHeader);
}
} // namespace MockSipData

bool SipKernel::initSipKernel(SipKernelType type, Device &device) {
    if (MockSipData::useMockSip) {
        auto &gfxCoreHelper = device.getGfxCoreHelper();
        if (gfxCoreHelper.isSipKernelAsHexadecimalArrayPreferred()) {
            SipKernel::classType = SipClassType::hexadecimalHeaderFile;
        } else {
            SipKernel::classType = SipClassType::builtins;
        }
        MockSipData::calledType = type;
        MockSipData::called = true;

        MockSipData::mockSipKernel->mockSipMemoryAllocation->clearUsageInfo();
        MockSipData::mockSipKernel->createTempSipAllocation(MemoryManager::maxOsContextCount);

        return MockSipData::returned;
    } else {
        return SipKernel::initSipKernelImpl(type, device, nullptr);
    }
}

void SipKernel::freeSipKernels(RootDeviceEnvironment *rootDeviceEnvironment, MemoryManager *memoryManager) {

    if (MockSipData::mockSipKernel) {
        MockSipData::mockSipKernel->tempSipMemoryAllocation.reset(nullptr);
    }

    for (auto &sipKernel : rootDeviceEnvironment->sipKernels) {
        if (sipKernel.get()) {
            memoryManager->freeGraphicsMemory(sipKernel->getSipAllocation());
            sipKernel.reset();
        }
    }
}

const SipKernel &SipKernel::getSipKernel(Device &device, OsContext *context) {
    if (MockSipData::useMockSip) {
        if (!MockSipData::called) {
            MockSipData::uninitializedSipRequested = true;
        }
        return *MockSipData::mockSipKernel.get();
    } else {
        if (context && device.getExecutionEnvironment()->getDebuggingMode() == NEO::DebuggingMode::offline) {
            return SipKernel::getDebugSipKernel(device, context);
        }
        auto sipType = SipKernel::getSipKernelType(device);
        SipKernel::initSipKernel(sipType, device);
        return SipKernel::getSipKernelImpl(device);
    }
}

} // namespace NEO
