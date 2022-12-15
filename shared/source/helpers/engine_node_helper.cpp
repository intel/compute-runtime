/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/engine_node_helper.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_helper.h"

namespace NEO::EngineHelpers {

std::string engineUsageToString(EngineUsage usage) {
    switch (usage) {
    case EngineUsage::Regular:
        return "Regular";
    case EngineUsage::LowPriority:
        return "LowPriority";
    case EngineUsage::Internal:
        return "Internal";
    case EngineUsage::Cooperative:
        return "Cooperative";
    default:
        return "Unknown";
    }
}

std::string engineTypeToString(aub_stream::EngineType engineType) {
    switch (engineType) {
    case aub_stream::EngineType::ENGINE_RCS:
        return "RCS";
    case aub_stream::EngineType::ENGINE_BCS:
        return "BCS";
    case aub_stream::EngineType::ENGINE_VCS:
        return "VCS";
    case aub_stream::EngineType::ENGINE_VECS:
        return "VECS";
    case aub_stream::EngineType::ENGINE_CCS:
        return "CCS";
    case aub_stream::EngineType::ENGINE_CCS1:
        return "CCS1";
    case aub_stream::EngineType::ENGINE_CCS2:
        return "CCS2";
    case aub_stream::EngineType::ENGINE_CCS3:
        return "CCS3";
    case aub_stream::EngineType::ENGINE_CCCS:
        return "CCCS";
    case aub_stream::EngineType::ENGINE_BCS1:
        return "BCS1";
    case aub_stream::EngineType::ENGINE_BCS2:
        return "BCS2";
    case aub_stream::EngineType::ENGINE_BCS3:
        return "BCS3";
    case aub_stream::EngineType::ENGINE_BCS4:
        return "BCS4";
    case aub_stream::EngineType::ENGINE_BCS5:
        return "BCS5";
    case aub_stream::EngineType::ENGINE_BCS6:
        return "BCS6";
    case aub_stream::EngineType::ENGINE_BCS7:
        return "BCS7";
    case aub_stream::EngineType::ENGINE_BCS8:
        return "BCS8";
    default:
        return "Unknown";
    }
}

bool isCcs(aub_stream::EngineType engineType) {
    return engineType >= aub_stream::ENGINE_CCS && engineType <= aub_stream::ENGINE_CCS3;
}

bool isBcs(aub_stream::EngineType engineType) {
    return engineType == aub_stream::ENGINE_BCS || (engineType >= aub_stream::ENGINE_BCS1 && engineType <= aub_stream::ENGINE_BCS8);
}

bool isBcsVirtualEngineEnabled(aub_stream::EngineType engineType) {
    bool useVirtualEnginesForBcs = engineType == aub_stream::EngineType::ENGINE_BCS ||
                                   engineType == aub_stream::EngineType::ENGINE_BCS1;

    if (DebugManager.flags.UseDrmVirtualEnginesForBcs.get() != -1) {
        useVirtualEnginesForBcs = !!DebugManager.flags.UseDrmVirtualEnginesForBcs.get();
    }

    return useVirtualEnginesForBcs;
}

aub_stream::EngineType getBcsEngineType(const RootDeviceEnvironment &rootDeviceEnvironment, const DeviceBitfield &deviceBitfield, SelectorCopyEngine &selectorCopyEngine, bool internalUsage) {
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
    if (DebugManager.flags.ForceBcsEngineIndex.get() != -1) {
        auto index = DebugManager.flags.ForceBcsEngineIndex.get();
        UNRECOVERABLE_IF(index > 8);

        return (index == 0) ? aub_stream::EngineType::ENGINE_BCS
                            : static_cast<aub_stream::EngineType>(aub_stream::EngineType::ENGINE_BCS1 + index - 1);
    }

    if (!linkCopyEnginesSupported(rootDeviceEnvironment, deviceBitfield)) {
        return aub_stream::ENGINE_BCS;
    }

    if (internalUsage) {
        if (DebugManager.flags.ForceBCSForInternalCopyEngine.get() != -1) {
            return DebugManager.flags.ForceBCSForInternalCopyEngine.get() == 0 ? aub_stream::EngineType::ENGINE_BCS
                                                                               : static_cast<aub_stream::EngineType>(aub_stream::EngineType::ENGINE_BCS1 + DebugManager.flags.ForceBCSForInternalCopyEngine.get() - 1);
        }
        return aub_stream::ENGINE_BCS3;
    }

    auto enableSelector = productHelper.isCopyEngineSelectorEnabled(hwInfo);

    if (DebugManager.flags.EnableCopyEngineSelector.get() != -1) {
        enableSelector = DebugManager.flags.EnableCopyEngineSelector.get();
    }

    if (enableSelector) {
        const bool isMainCopyEngineAlreadyUsed = selectorCopyEngine.isMainUsed.exchange(true);
        if (isMainCopyEngineAlreadyUsed) {
            return selectLinkCopyEngine(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine.selector);
        }
    }

    return aub_stream::ENGINE_BCS;
}

void releaseBcsEngineType(aub_stream::EngineType engineType, SelectorCopyEngine &selectorCopyEngine) {
    if (engineType == aub_stream::EngineType::ENGINE_BCS) {
        selectorCopyEngine.isMainUsed.store(false);
    }
}

aub_stream::EngineType remapEngineTypeToHwSpecific(aub_stream::EngineType inputType, const RootDeviceEnvironment &rootDeviceEnvironment) {
    auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<GfxCoreHelper>();
    bool isExpectedProduct = gfxCoreHelper.isEngineTypeRemappingToHwSpecificRequired();

    if (isExpectedProduct && inputType == aub_stream::EngineType::ENGINE_RCS) {
        return aub_stream::EngineType::ENGINE_CCCS;
    }

    return inputType;
}

uint32_t getBcsIndex(aub_stream::EngineType engineType) {
    UNRECOVERABLE_IF(!isBcs(engineType));
    if (engineType == aub_stream::ENGINE_BCS) {
        return 0;
    } else {
        return 1 + engineType - aub_stream::ENGINE_BCS1;
    }
}

aub_stream::EngineType mapBcsIndexToEngineType(uint32_t index, bool includeMainCopyEngine) {
    if (index == 0 && includeMainCopyEngine) {
        return aub_stream::ENGINE_BCS;
    } else {
        auto offset = index;
        if (includeMainCopyEngine) {
            offset -= 1;
        }

        return static_cast<aub_stream::EngineType>(offset + static_cast<uint32_t>(aub_stream::ENGINE_BCS1));
    }
}

bool isBcsEnabled(const HardwareInfo &hwInfo, aub_stream::EngineType engineType) {
    return hwInfo.featureTable.ftrBcsInfo.test(getBcsIndex(engineType));
}

bool linkCopyEnginesSupported(const RootDeviceEnvironment &rootDeviceEnvironment, const DeviceBitfield &deviceBitfield) {
    auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<GfxCoreHelper>();
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    const aub_stream::EngineType engine1 = gfxCoreHelper.isSubDeviceEngineSupported(hwInfo, deviceBitfield, aub_stream::ENGINE_BCS1)
                                               ? aub_stream::ENGINE_BCS1
                                               : aub_stream::ENGINE_BCS4;
    const aub_stream::EngineType engine2 = aub_stream::ENGINE_BCS2;

    return isBcsEnabled(hwInfo, engine1) || isBcsEnabled(hwInfo, engine2);
}

aub_stream::EngineType selectLinkCopyEngine(const RootDeviceEnvironment &rootDeviceEnvironment, const DeviceBitfield &deviceBitfield, std::atomic<uint32_t> &selectorCopyEngine) {
    auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<GfxCoreHelper>();
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    auto enableCmdQRoundRobindBcsEngineAssign = false;

    if (DebugManager.flags.EnableCmdQRoundRobindBcsEngineAssign.get() != -1) {
        enableCmdQRoundRobindBcsEngineAssign = DebugManager.flags.EnableCmdQRoundRobindBcsEngineAssign.get();
    }

    if (enableCmdQRoundRobindBcsEngineAssign) {
        aub_stream::EngineType engineType;

        auto bcsRoundRobinLimit = EngineHelpers::numLinkedCopyEngines;
        auto engineOffset = 0u;
        auto mainCE = false;

        if (DebugManager.flags.EnableCmdQRoundRobindBcsEngineAssignStartingValue.get() != -1) {
            engineOffset = DebugManager.flags.EnableCmdQRoundRobindBcsEngineAssignStartingValue.get();
            mainCE = engineOffset == 0;
        }

        if (mainCE) {
            bcsRoundRobinLimit++;
        }

        if (DebugManager.flags.EnableCmdQRoundRobindBcsEngineAssignLimit.get() != -1) {
            bcsRoundRobinLimit = DebugManager.flags.EnableCmdQRoundRobindBcsEngineAssignLimit.get();
        }

        do {
            auto selectEngineValue = (selectorCopyEngine.fetch_add(1u) % bcsRoundRobinLimit) + engineOffset;

            if (mainCE) {
                if (selectEngineValue == 0u) {
                    engineType = aub_stream::EngineType::ENGINE_BCS;
                } else {
                    engineType = static_cast<aub_stream::EngineType>(aub_stream::EngineType::ENGINE_BCS1 + selectEngineValue - 1);
                }
            } else {
                engineType = static_cast<aub_stream::EngineType>(aub_stream::EngineType::ENGINE_BCS1 + selectEngineValue);
            }

        } while (!gfxCoreHelper.isSubDeviceEngineSupported(hwInfo, deviceBitfield, engineType) || !hwInfo.featureTable.ftrBcsInfo.test(engineType == aub_stream::EngineType::ENGINE_BCS
                                                                                                                                           ? 0
                                                                                                                                           : engineType - aub_stream::EngineType::ENGINE_BCS1 + 1));

        return engineType;
    }

    const aub_stream::EngineType engine1 = gfxCoreHelper.isSubDeviceEngineSupported(hwInfo, deviceBitfield, aub_stream::ENGINE_BCS1)
                                               ? aub_stream::ENGINE_BCS1
                                               : aub_stream::ENGINE_BCS4;
    const aub_stream::EngineType engine2 = aub_stream::ENGINE_BCS2;

    if (isBcsEnabled(hwInfo, engine1) && isBcsEnabled(hwInfo, engine2)) {
        // both BCS enabled, round robin
        return selectorCopyEngine.fetch_xor(1u) ? engine1 : engine2;
    } else {
        // one BCS enabled
        return isBcsEnabled(hwInfo, engine1) ? engine1 : engine2;
    }
}
aub_stream::EngineType mapCcsIndexToEngineType(uint32_t index) {
    return static_cast<aub_stream::EngineType>(index + static_cast<uint32_t>(aub_stream::ENGINE_CCS));
}

} // namespace NEO::EngineHelpers
