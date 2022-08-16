/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/unit_tests/sources/debug/linux/debug_session_fixtures_linux.h"

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

    if (version.major < 2) {
        // grfs for 0/3/7/3 - somewhere in the middle
        fillRegsetForThread(&pStateSaveAreaHeader->regHeader.grf, 0, 3, 7, 3, 'a');

        // grfs for 0/5/15/6 - very last eu thread
        fillRegsetForThread(&pStateSaveAreaHeader->regHeader.grf, 0, 5, 15, 6, 'a');
    }
}

void DebugApiLinuxFixture::setUp(NEO::HardwareInfo *hwInfo) {
    if (hwInfo != nullptr) {
        auto executionEnvironment = MockDevice::prepareExecutionEnvironment(hwInfo, 0u);
        DeviceFixture::setupWithExecutionEnvironment(*executionEnvironment);
    } else {
        DeviceFixture::setUp();
    }

    mockDrm = new DrmQueryMock(*neoDevice->executionEnvironment->rootDeviceEnvironments[0]);
    mockDrm->allowDebugAttach = true;
    mockDrm->queryEngineInfo();

    // set config from HwInfo to have correct topology requested by tests
    if (hwInfo) {
        mockDrm->storedSVal = hwInfo->gtSystemInfo.SliceCount;
        mockDrm->storedSSVal = hwInfo->gtSystemInfo.SubSliceCount;
        mockDrm->storedEUVal = hwInfo->gtSystemInfo.EUCount;
    }
    NEO::Drm::QueryTopologyData topologyData = {};
    mockDrm->queryTopology(neoDevice->getHardwareInfo(), topologyData);

    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface);
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mockDrm));
}

void DebugApiLinuxMultiDeviceFixture::setUp() {
    MultipleDevicesWithCustomHwInfo::setUp();
    neoDevice = driverHandle->devices[0]->getNEODevice();

    L0::Device *device = driverHandle->devices[0];
    deviceImp = static_cast<DeviceImp *>(device);

    mockDrm = new DrmQueryMock(*neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0], neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->getHardwareInfo());
    mockDrm->allowDebugAttach = true;

    // set config from HwInfo to have correct topology requested by tests
    mockDrm->storedSVal = hwInfo.gtSystemInfo.SliceCount;
    mockDrm->storedSSVal = hwInfo.gtSystemInfo.SubSliceCount;
    mockDrm->storedEUVal = hwInfo.gtSystemInfo.EUCount;

    mockDrm->queryEngineInfo();
    auto engineInfo = mockDrm->getEngineInfo();
    ASSERT_NE(nullptr, engineInfo->getEngineInstance(1, hwInfo.capabilityTable.defaultEngineType));

    NEO::Drm::QueryTopologyData topologyData = {};
    mockDrm->queryTopology(neoDevice->getHardwareInfo(), topologyData);

    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface);
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mockDrm));
}

TileDebugSessionLinux *MockDebugSessionLinux::createTileSession(const zet_debug_config_t &config, L0::Device *device, L0::DebugSessionImp *rootDebugSession) {
    return new MockTileDebugSessionLinux(config, device, rootDebugSession);
}
} // namespace ult
} // namespace L0
