/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/engine_node_helper.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"

namespace NEO::EngineHelpers {

std::string engineUsageToString(EngineUsage usage) {
    switch (usage) {
    case EngineUsage::regular:
        return "Regular";
    case EngineUsage::lowPriority:
        return "LowPriority";
    case EngineUsage::highPriority:
        return "HighPriority";
    case EngineUsage::internal:
        return "Internal";
    case EngineUsage::cooperative:
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

bool isComputeEngine(aub_stream::EngineType engineType) {
    return isCcs(engineType) || engineType == aub_stream::ENGINE_RCS || engineType == aub_stream::ENGINE_CCCS;
}

bool isBcs(aub_stream::EngineType engineType) {
    return engineType == aub_stream::ENGINE_BCS || (engineType >= aub_stream::ENGINE_BCS1 && engineType <= aub_stream::ENGINE_BCS8);
}

bool isBcsVirtualEngineEnabled(aub_stream::EngineType engineType) {
    bool useVirtualEnginesForBcs = engineType == aub_stream::EngineType::ENGINE_BCS ||
                                   engineType == aub_stream::EngineType::ENGINE_BCS1;

    if (debugManager.flags.UseDrmVirtualEnginesForBcs.get() != -1) {
        useVirtualEnginesForBcs = !!debugManager.flags.UseDrmVirtualEnginesForBcs.get();
    }

    return useVirtualEnginesForBcs;
}

aub_stream::EngineType getBcsEngineType(const RootDeviceEnvironment &rootDeviceEnvironment, const DeviceBitfield &deviceBitfield, SelectorCopyEngine &selectorCopyEngine, bool internalUsage) {
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
    auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<GfxCoreHelper>();
    if (debugManager.flags.ForceBcsEngineIndex.get() != -1) {
        auto index = debugManager.flags.ForceBcsEngineIndex.get();
        UNRECOVERABLE_IF(index > 8);

        return (index == 0) ? aub_stream::EngineType::ENGINE_BCS
                            : static_cast<aub_stream::EngineType>(aub_stream::EngineType::ENGINE_BCS1 + index - 1);
    }

    if (!linkCopyEnginesSupported(rootDeviceEnvironment, deviceBitfield)) {
        return aub_stream::ENGINE_BCS;
    }

    if (internalUsage) {
        return EngineHelpers::mapBcsIndexToEngineType(gfxCoreHelper.getInternalCopyEngineIndex(hwInfo), true);
    }

    auto enableSelector = productHelper.isCopyEngineSelectorEnabled(hwInfo);

    if (debugManager.flags.EnableCopyEngineSelector.get() != -1) {
        enableSelector = debugManager.flags.EnableCopyEngineSelector.get();
    }

    const bool isBCS0Default = (aub_stream::ENGINE_BCS == productHelper.getDefaultCopyEngine());

    if (enableSelector) {
        const bool isMainCopyEngineAlreadyUsed = selectorCopyEngine.isMainUsed.exchange(true);
        if (false == isMainCopyEngineAlreadyUsed && false == isBCS0Default) {
            return productHelper.getDefaultCopyEngine();
        } else if (isMainCopyEngineAlreadyUsed) {
            return selectLinkCopyEngine(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine.selector);
        }
    }

    return aub_stream::ENGINE_BCS;
}

void releaseBcsEngineType(aub_stream::EngineType engineType, SelectorCopyEngine &selectorCopyEngine, const RootDeviceEnvironment &rootDeviceEnvironment) {
    if (auto &productHelper = rootDeviceEnvironment.getProductHelper(); engineType == productHelper.getDefaultCopyEngine()) {
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

uint32_t getCcsIndex(aub_stream::EngineType engineType) {
    UNRECOVERABLE_IF(!isCcs(engineType));
    if (engineType == aub_stream::ENGINE_CCS) {
        return 0;
    } else {
        return engineType - aub_stream::ENGINE_CCS;
    }
}

uint32_t getBcsIndex(aub_stream::EngineType engineType) {
    UNRECOVERABLE_IF(!isBcs(engineType));
    if (engineType == aub_stream::ENGINE_BCS) {
        return 0;
    } else {
        return 1 + engineType - aub_stream::ENGINE_BCS1;
    }
}

aub_stream::EngineType getBcsEngineAtIdx(uint32_t idx) {
    if (idx == 0) {
        return aub_stream::ENGINE_BCS;
    }
    return static_cast<aub_stream::EngineType>((idx - 1) + aub_stream::ENGINE_BCS1);
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
    const aub_stream::EngineType engine1 = gfxCoreHelper.isSubDeviceEngineSupported(rootDeviceEnvironment, deviceBitfield, aub_stream::ENGINE_BCS1)
                                               ? aub_stream::ENGINE_BCS1
                                               : aub_stream::ENGINE_BCS4;
    const aub_stream::EngineType engine2 = aub_stream::ENGINE_BCS2;

    return isBcsEnabled(hwInfo, engine1) || isBcsEnabled(hwInfo, engine2);
}

aub_stream::EngineType selectLinkCopyEngine(const RootDeviceEnvironment &rootDeviceEnvironment, const DeviceBitfield &deviceBitfield, std::atomic<uint32_t> &selectorCopyEngine) {
    auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<GfxCoreHelper>();
    auto &productHelper = rootDeviceEnvironment.getProductHelper();
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();

    const aub_stream::EngineType engine1 = gfxCoreHelper.isSubDeviceEngineSupported(rootDeviceEnvironment, deviceBitfield, aub_stream::ENGINE_BCS1) && aub_stream::ENGINE_BCS1 != productHelper.getDefaultCopyEngine()
                                               ? aub_stream::ENGINE_BCS1
                                               : aub_stream::ENGINE_BCS4;
    const aub_stream::EngineType engine2 = aub_stream::ENGINE_BCS2;

    auto hpEngine = gfxCoreHelper.getDefaultHpCopyEngine(hwInfo);

    if (isBcsEnabled(hwInfo, engine1) && engine1 != hpEngine &&
        isBcsEnabled(hwInfo, engine2) && engine2 != hpEngine) {
        // both BCS enabled, round robin
        return selectorCopyEngine.fetch_xor(1u) ? engine1 : engine2;
    } else {
        // one BCS enabled
        if (isBcsEnabled(hwInfo, engine1) && (engine1 != hpEngine)) {
            return engine1;
        } else if (isBcsEnabled(hwInfo, engine2) && (engine2 != hpEngine)) {
            return engine2;
        } else {
            return productHelper.getDefaultCopyEngine();
        }
    }
}
aub_stream::EngineType mapCcsIndexToEngineType(uint32_t index) {
    return static_cast<aub_stream::EngineType>(index + static_cast<uint32_t>(aub_stream::ENGINE_CCS));
}

EngineGroupType engineTypeToEngineGroupType(aub_stream::EngineType engineType) {
    if (isCcs(engineType)) {
        return EngineGroupType::compute;
    } else if (isComputeEngine(engineType)) {
        return EngineGroupType::renderCompute;
    } else if (engineType == aub_stream::ENGINE_BCS) {
        return EngineGroupType::copy;
    }
    return EngineGroupType::linkedCopy;
}

} // namespace NEO::EngineHelpers
