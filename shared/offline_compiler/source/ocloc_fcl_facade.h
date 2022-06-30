/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "cif/common/cif_main.h"
#include "cif/import/library_api.h"
#include "igfxfmid.h"
#include "ocl_igc_interface/code_type.h"
#include "ocl_igc_interface/fcl_ocl_device_ctx.h"

#include <memory>
#include <string>

class OclocArgHelper;

namespace NEO {

class CompilerHwInfoConfig;
class OsLibrary;

struct HardwareInfo;

class OclocFclFacade {
  public:
    OclocFclFacade(OclocArgHelper *argHelper);
    MOCKABLE_VIRTUAL ~OclocFclFacade();

    OclocFclFacade(OclocFclFacade &) = delete;
    OclocFclFacade(const OclocFclFacade &&) = delete;
    OclocFclFacade &operator=(const OclocFclFacade &) = delete;
    OclocFclFacade &operator=(OclocFclFacade &&) = delete;

    int initialize(const HardwareInfo &hwInfo);
    bool isInitialized() const;
    IGC::CodeType::CodeType_t getPreferredIntermediateRepresentation() const;
    CIF::RAII::UPtr_t<CIF::Builtins::BufferLatest> createConstBuffer(const void *data, size_t size);
    CIF::RAII::UPtr_t<IGC::FclOclTranslationCtxTagOCL> createTranslationContext(IGC::CodeType::CodeType_t inType, IGC::CodeType::CodeType_t outType, CIF::Builtins::BufferLatest *error);

  protected:
    MOCKABLE_VIRTUAL std::unique_ptr<OsLibrary> loadFclLibrary() const;
    MOCKABLE_VIRTUAL CIF::CreateCIFMainFunc_t loadCreateFclMainFunction() const;
    MOCKABLE_VIRTUAL CIF::RAII::UPtr_t<CIF::CIFMain> createFclMain(CIF::CreateCIFMainFunc_t createMainFunction) const;
    MOCKABLE_VIRTUAL bool isFclInterfaceCompatible() const;
    MOCKABLE_VIRTUAL std::string getIncompatibleInterface() const;
    MOCKABLE_VIRTUAL CIF::RAII::UPtr_t<IGC::FclOclDeviceCtxTagOCL> createFclDeviceContext() const;
    MOCKABLE_VIRTUAL bool shouldPopulateFclInterface() const;
    MOCKABLE_VIRTUAL CIF::RAII::UPtr_t<IGC::PlatformTagOCL> getPlatformHandle() const;
    MOCKABLE_VIRTUAL void populateFclInterface(IGC::PlatformTagOCL &handle, const PLATFORM &platform);

    OclocArgHelper *argHelper{};
    std::unique_ptr<OsLibrary> fclLib{};
    CIF::RAII::UPtr_t<CIF::CIFMain> fclMain{};
    CIF::RAII::UPtr_t<IGC::FclOclDeviceCtxTagOCL> fclDeviceCtx{};
    bool initialized{false};
};

} // namespace NEO