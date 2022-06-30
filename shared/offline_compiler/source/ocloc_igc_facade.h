/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "cif/common/cif_main.h"
#include "cif/import/library_api.h"
#include "ocl_igc_interface/code_type.h"
#include "ocl_igc_interface/igc_ocl_device_ctx.h"

#include <memory>
#include <string>

class OclocArgHelper;

namespace NEO {

class CompilerHwInfoConfig;
class OsLibrary;

struct HardwareInfo;

class OclocIgcFacade {
  public:
    OclocIgcFacade(OclocArgHelper *argHelper);
    MOCKABLE_VIRTUAL ~OclocIgcFacade();

    OclocIgcFacade(OclocIgcFacade &) = delete;
    OclocIgcFacade(const OclocIgcFacade &&) = delete;
    OclocIgcFacade &operator=(const OclocIgcFacade &) = delete;
    OclocIgcFacade &operator=(OclocIgcFacade &&) = delete;

    int initialize(const HardwareInfo &hwInfo);
    bool isInitialized() const;
    CIF::RAII::UPtr_t<CIF::Builtins::BufferLatest> createConstBuffer(const void *data, size_t size);
    CIF::RAII::UPtr_t<IGC::IgcOclTranslationCtxTagOCL> createTranslationContext(IGC::CodeType::CodeType_t inType, IGC::CodeType::CodeType_t outType);

  protected:
    MOCKABLE_VIRTUAL std::unique_ptr<OsLibrary> loadIgcLibrary() const;
    MOCKABLE_VIRTUAL CIF::CreateCIFMainFunc_t loadCreateIgcMainFunction() const;
    MOCKABLE_VIRTUAL CIF::RAII::UPtr_t<CIF::CIFMain> createIgcMain(CIF::CreateCIFMainFunc_t createMainFunction) const;
    MOCKABLE_VIRTUAL bool isIgcInterfaceCompatible(const std::vector<CIF::InterfaceId_t> &interfacesToIgnore) const;
    MOCKABLE_VIRTUAL std::string getIncompatibleInterface(const std::vector<CIF::InterfaceId_t> &interfacesToIgnore) const;
    MOCKABLE_VIRTUAL bool isPatchtokenInterfaceSupported() const;
    MOCKABLE_VIRTUAL CIF::RAII::UPtr_t<IGC::IgcOclDeviceCtxTagOCL> createIgcDeviceContext() const;
    MOCKABLE_VIRTUAL CIF::RAII::UPtr_t<IGC::PlatformTagOCL> getIgcPlatformHandle() const;
    MOCKABLE_VIRTUAL CIF::RAII::UPtr_t<IGC::GTSystemInfoTagOCL> getGTSystemInfoHandle() const;
    MOCKABLE_VIRTUAL CIF::RAII::UPtr_t<IGC::IgcFeaturesAndWorkaroundsTagOCL> getIgcFeaturesAndWorkaroundsHandle() const;
    void populateWithFeatures(IGC::IgcFeaturesAndWorkaroundsTagOCL *handle, const HardwareInfo &hwInfo, const CompilerHwInfoConfig *compilerHwInfoConfig) const;

    OclocArgHelper *argHelper{};
    std::unique_ptr<OsLibrary> igcLib{};
    CIF::RAII::UPtr_t<CIF::CIFMain> igcMain{};
    CIF::RAII::UPtr_t<IGC::IgcOclDeviceCtxTagOCL> igcDeviceCtx{};
    bool initialized{false};
};

} // namespace NEO