/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"

#include "shared/source/ail/ail_configuration.h"
#include "shared/source/assert_handler/assert_handler.h"
#include "shared/source/aub/aub_center.h"
#include "shared/source/built_ins/built_ins.h"
#include "shared/source/built_ins/sip.h"
#include "shared/source/compiler_interface/compiler_interface.h"
#include "shared/source/compiler_interface/default_cache_config.h"
#include "shared/source/debugger/debugger.h"
#include "shared/source/debugger/debugger_l0.h"
#include "shared/source/device/device.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/page_table_mngr.h"
#include "shared/source/helpers/api_gfx_core_helper.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/bindless_heaps_helper.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/memory_operations_handler.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/os_time.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/program/print_formatter.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/source/sip_external_lib/sip_external_lib.h"
#include "shared/source/utilities/software_tags_manager.h"
#include "shared/source/utilities/wait_util.h"

namespace NEO {

RootDeviceEnvironment::RootDeviceEnvironment(ExecutionEnvironment &executionEnvironment) : executionEnvironment(executionEnvironment) {
    hwInfo = std::make_unique<HardwareInfo>();

    if (debugManager.flags.EnableSWTags.get()) {
        tagsManager = std::make_unique<SWTagsManager>();
    }
}

RootDeviceEnvironment::~RootDeviceEnvironment() = default;

void RootDeviceEnvironment::initAubCenter(bool localMemoryEnabled, const std::string &aubFileName, CommandStreamReceiverType csrType) {
    if (!aubCenter) {
        UNRECOVERABLE_IF(!getGmmHelper());
        aubCenter.reset(new AubCenter(*this, localMemoryEnabled, aubFileName, csrType));
    }
    if (debugManager.flags.UseAubStream.get() && aubCenter->getAubManager() == nullptr) {
        printToStderr("ERROR: Simulation mode detected but Aubstream is not available.\n");
        UNRECOVERABLE_IF(true);
    }
}

void RootDeviceEnvironment::initDebuggerL0(Device *neoDevice) {

    DEBUG_BREAK_IF(this->debugger.get() != nullptr);

    this->getMutableHardwareInfo()->capabilityTable.fusedEuEnabled = false;
    this->getMutableHardwareInfo()->capabilityTable.ftrRenderCompressedBuffers = false;
    this->getMutableHardwareInfo()->capabilityTable.ftrRenderCompressedImages = false;

    this->debugger = DebuggerL0::create(neoDevice);
}

void RootDeviceEnvironment::initWaitUtils() {
    WaitUtils::init(WaitUtils::WaitpkgUse::tpause, *hwInfo);
}

const HardwareInfo *RootDeviceEnvironment::getHardwareInfo() const {
    return hwInfo.get();
}

HardwareInfo *RootDeviceEnvironment::getMutableHardwareInfo() const {
    return hwInfo.get();
}

void RootDeviceEnvironment::setHwInfoAndInitHelpers(const HardwareInfo *hwInfo) {
    setHwInfo(hwInfo);
    initHelpers();
}

void RootDeviceEnvironment::setHwInfo(const HardwareInfo *hwInfo) {
    *this->hwInfo = *hwInfo;
    if (debugManager.flags.DisableSupportForL0Debugger.get() == 1) {
        this->hwInfo->capabilityTable.l0DebuggerSupported = false;
    }
}

bool RootDeviceEnvironment::isFullRangeSvm() const {
    return hwInfo->capabilityTable.gpuAddressSpace >= maxNBitValue(47);
}

GmmHelper *RootDeviceEnvironment::getGmmHelper() const {
    return gmmHelper.get();
}
GmmClientContext *RootDeviceEnvironment::getGmmClientContext() const {
    return gmmHelper->getClientContext();
}

void RootDeviceEnvironment::prepareForCleanup() const {
    if (osInterface && osInterface->getDriverModel()) {
        osInterface->getDriverModel()->isDriverAvailable();
    }
}

bool RootDeviceEnvironment::initAilConfiguration() {
    if (ailConfiguration == nullptr) {
        return (false == debugManager.flags.EnableAIL.get());
    }

    auto result = ailConfiguration->initProcessExecutableName();
    if (result != true) {
        return false;
    }

    ailConfiguration->apply(*hwInfo);

    return true;
}

void RootDeviceEnvironment::initGmm() {
    if (!gmmHelper) {
        gmmHelper.reset(new GmmHelper(*this));
    }
}

void RootDeviceEnvironment::initOsTime() {
    if (!osTime) {
        osTime = OSTime::create(osInterface.get());
        osTime->setDeviceTimerResolution();
    }
}

BindlessHeapsHelper *RootDeviceEnvironment::getBindlessHeapsHelper() const {
    return bindlessHeapsHelper.get();
}

const ProductHelper &RootDeviceEnvironment::getProductHelper() const {
    return *productHelper;
}

void RootDeviceEnvironment::createBindlessHeapsHelper(Device *rootDevice, bool availableDevices) {
    bindlessHeapsHelper = std::make_unique<BindlessHeapsHelper>(rootDevice, availableDevices);
}

CompilerInterface *RootDeviceEnvironment::getCompilerInterface() {
    if (this->compilerInterface.get() == nullptr) {
        std::lock_guard<std::mutex> autolock(this->mtx);
        if (this->compilerInterface.get() == nullptr) {
            auto cache = std::make_unique<CompilerCache>(getDefaultCompilerCacheConfig());
            this->compilerInterface.reset(CompilerInterface::createInstance(std::move(cache), ApiSpecificConfig::getApiType() == ApiSpecificConfig::ApiType::OCL));
        }
    }
    return this->compilerInterface.get();
}

SipExternalLib *RootDeviceEnvironment::getSipExternalLibInterface() {
    if (sipExternalLib.get() == nullptr) {
        if (gfxCoreHelper->getSipBinaryFromExternalLib()) {
            std::lock_guard<std::mutex> autolock(this->mtx);
            sipExternalLib.reset(SipExternalLib::getSipExternalLibInstance());
        }
    }
    return sipExternalLib.get();
}

void RootDeviceEnvironment::initHelpers() {
    initProductHelper();
    initGfxCoreHelper();
    initializeGfxCoreHelperFromProductHelper();
    initializeGfxCoreHelperFromHwInfo();
    initApiGfxCoreHelper();
    initCompilerProductHelper();
    initReleaseHelper();
    initAilConfigurationHelper();
    initWaitUtils();
}

void RootDeviceEnvironment::initializeGfxCoreHelperFromHwInfo() {
    if (gfxCoreHelper != nullptr) {
        gfxCoreHelper->initializeDefaultHpCopyEngine(*this->getHardwareInfo());
    }
}

void RootDeviceEnvironment::initializeGfxCoreHelperFromProductHelper() {
    if (this->productHelper) {
        gfxCoreHelper->initializeFromProductHelper(*this->productHelper.get());
    }
}

void RootDeviceEnvironment::initGfxCoreHelper() {
    if (gfxCoreHelper == nullptr) {
        gfxCoreHelper = GfxCoreHelper::create(this->getHardwareInfo()->platform.eRenderCoreFamily);
    }
}

void RootDeviceEnvironment::initProductHelper() {
    if (productHelper == nullptr) {
        productHelper = ProductHelper::create(this->getHardwareInfo()->platform.eProductFamily);
    }
}
void RootDeviceEnvironment::initCompilerProductHelper() {
    if (compilerProductHelper == nullptr) {
        compilerProductHelper = CompilerProductHelper::create(this->getHardwareInfo()->platform.eProductFamily);
    }
}

void RootDeviceEnvironment::initReleaseHelper() {
    if (releaseHelper == nullptr) {
        releaseHelper = ReleaseHelper::create(this->getHardwareInfo()->ipVersion);
    }
}

void RootDeviceEnvironment::initAilConfigurationHelper() {
    if (ailConfiguration == nullptr && debugManager.flags.EnableAIL.get()) {
        ailConfiguration = AILConfiguration::create(this->getHardwareInfo()->platform.eProductFamily);
    }
}

ReleaseHelper *RootDeviceEnvironment::getReleaseHelper() const {
    return releaseHelper.get();
}

AILConfiguration *RootDeviceEnvironment::getAILConfigurationHelper() const {
    return ailConfiguration.get();
}

BuiltIns *RootDeviceEnvironment::getBuiltIns() {
    if (this->builtins.get() == nullptr) {
        std::lock_guard<std::mutex> autolock(this->mtx);
        if (this->builtins.get() == nullptr) {
            this->builtins = std::make_unique<BuiltIns>();
        }
    }
    return this->builtins.get();
}

void RootDeviceEnvironment::setNumberOfCcs(uint32_t numberOfCcs) {

    hwInfo->gtSystemInfo.CCSInfo.NumberOfCCSEnabled = std::min(hwInfo->gtSystemInfo.CCSInfo.NumberOfCCSEnabled, numberOfCcs);
    limitedNumberOfCcs = true;
}

bool RootDeviceEnvironment::isNumberOfCcsLimited() const {
    return limitedNumberOfCcs;
}

void RootDeviceEnvironment::setRcsExposure() {
    if (releaseHelper) {
        if (releaseHelper->isRcsExposureDisabled()) {
            hwInfo->featureTable.flags.ftrRcsNode = false;
            if ((debugManager.flags.NodeOrdinal.get() == static_cast<int32_t>(aub_stream::EngineType::ENGINE_RCS)) || (debugManager.flags.NodeOrdinal.get() == static_cast<int32_t>(aub_stream::EngineType::ENGINE_CCCS))) {
                hwInfo->featureTable.flags.ftrRcsNode = true;
            }
        }
    }
}

void RootDeviceEnvironment::initDummyAllocation() {
    std::call_once(isDummyAllocationInitialized, [this]() {
        auto customDeleter = [this](GraphicsAllocation *dummyAllocation) {
            this->executionEnvironment.memoryManager->freeGraphicsMemory(dummyAllocation);
        };
        auto dummyBlitAllocation = this->executionEnvironment.memoryManager->allocateGraphicsMemoryWithProperties(
            *this->dummyBlitProperties.get());
        this->dummyAllocation = GraphicsAllocationUniquePtrType(dummyBlitAllocation, customDeleter);
    });
}

void RootDeviceEnvironment::setDummyBlitProperties(uint32_t rootDeviceIndex) {
    size_t size = 32 * MemoryConstants::kiloByte;
    this->dummyBlitProperties = std::make_unique<AllocationProperties>(rootDeviceIndex, size, NEO::AllocationType::buffer, systemMemoryBitfield);
}

GraphicsAllocation *RootDeviceEnvironment::getDummyAllocation() const {
    return dummyAllocation.get();
}

void RootDeviceEnvironment::releaseDummyAllocation() {
    dummyAllocation.reset();
}

AssertHandler *RootDeviceEnvironment::getAssertHandler(Device *neoDevice) {
    if (this->assertHandler.get() == nullptr) {
        std::lock_guard<std::mutex> autolock(this->mtx);
        if (this->assertHandler.get() == nullptr) {
            this->assertHandler = std::make_unique<AssertHandler>(neoDevice);
        }
    }
    return this->assertHandler.get();
}

bool RootDeviceEnvironment::isWddmOnLinux() const {
    return isWddmOnLinuxEnable;
}

template <typename HelperType>
HelperType &RootDeviceEnvironment::getHelper() const {
    if constexpr (std::is_same_v<HelperType, CompilerProductHelper>) {
        UNRECOVERABLE_IF(compilerProductHelper == nullptr);
        return *compilerProductHelper;
    } else if constexpr (std::is_same_v<HelperType, ProductHelper>) {
        UNRECOVERABLE_IF(productHelper == nullptr);
        return *productHelper;
    } else {
        static_assert(std::is_same_v<HelperType, GfxCoreHelper>, "Only CompilerProductHelper, ProductHelper and GfxCoreHelper are supported");
        UNRECOVERABLE_IF(gfxCoreHelper == nullptr);
        return *gfxCoreHelper;
    }
}

template ProductHelper &RootDeviceEnvironment::getHelper() const;
template CompilerProductHelper &RootDeviceEnvironment::getHelper() const;
template GfxCoreHelper &RootDeviceEnvironment::getHelper() const;

} // namespace NEO
