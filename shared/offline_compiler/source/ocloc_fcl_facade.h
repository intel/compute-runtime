/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "cif/common/cif_main.h"
#include "cif/import/library_api.h"
#include "neo_igfxfmid.h"
#include "ocl_igc_interface/code_type.h"
#include "ocl_igc_interface/fcl_ocl_device_ctx.h"

#include <memory>
#include <string>

class OclocArgHelper;

namespace NEO {

class CompilerProductHelper;
class OsLibrary;

struct HardwareInfo;

class OclocFclFacadeBase : NEO::NonCopyableAndNonMovableClass {
  protected:
    OclocFclFacadeBase() = default;

  public:
    virtual ~OclocFclFacadeBase() = default;
    virtual int initialize(const HardwareInfo &hwInfo) = 0;
    virtual bool isInitialized() const = 0;
    virtual IGC::CodeType::CodeType_t getPreferredIntermediateRepresentation() const = 0;
    virtual CIF::RAII::UPtr_t<CIF::Builtins::BufferLatest> createConstBuffer(const void *data, size_t size) = 0;

    virtual CIF::RAII::UPtr_t<IGC::OclTranslationOutputTagOCL> translate(IGC::CodeType::CodeType_t inType, IGC::CodeType::CodeType_t outType, CIF::Builtins::BufferLatest *error,
                                                                         CIF::Builtins::BufferSimple *src,
                                                                         CIF::Builtins::BufferSimple *options,
                                                                         CIF::Builtins::BufferSimple *internalOptions,
                                                                         CIF::Builtins::BufferSimple *tracingOptions,
                                                                         uint32_t tracingOptionsCount) = 0;
};
static_assert(NEO::NonCopyableAndNonMovable<OclocFclFacadeBase>);

class OclocFclFacade : public OclocFclFacadeBase {
  public:
    OclocFclFacade(OclocArgHelper *argHelper);
    ~OclocFclFacade() override;

    int initialize(const HardwareInfo &hwInfo) override;
    bool isInitialized() const override;
    IGC::CodeType::CodeType_t getPreferredIntermediateRepresentation() const override;
    CIF::RAII::UPtr_t<CIF::Builtins::BufferLatest> createConstBuffer(const void *data, size_t size) override;
    CIF::RAII::UPtr_t<IGC::OclTranslationOutputTagOCL> translate(IGC::CodeType::CodeType_t inType, IGC::CodeType::CodeType_t outType, CIF::Builtins::BufferLatest *error,
                                                                 CIF::Builtins::BufferSimple *src,
                                                                 CIF::Builtins::BufferSimple *options,
                                                                 CIF::Builtins::BufferSimple *internalOptions,
                                                                 CIF::Builtins::BufferSimple *tracingOptions,
                                                                 uint32_t tracingOptionsCount) override;

  protected:
    MOCKABLE_VIRTUAL CIF::RAII::UPtr_t<IGC::FclOclTranslationCtxTagOCL> createTranslationContext(IGC::CodeType::CodeType_t inType, IGC::CodeType::CodeType_t outType, CIF::Builtins::BufferLatest *error);
    MOCKABLE_VIRTUAL std::unique_ptr<OsLibrary> loadFclLibrary() const;
    MOCKABLE_VIRTUAL CIF::CreateCIFMainFunc_t loadCreateFclMainFunction() const;
    MOCKABLE_VIRTUAL CIF::RAII::UPtr_t<CIF::CIFMain> createFclMain(CIF::CreateCIFMainFunc_t createMainFunction) const;
    MOCKABLE_VIRTUAL bool isFclInterfaceCompatible() const;
    MOCKABLE_VIRTUAL std::string getIncompatibleInterface() const;
    MOCKABLE_VIRTUAL CIF::RAII::UPtr_t<IGC::FclOclDeviceCtxTagOCL> createFclDeviceContext() const;
    MOCKABLE_VIRTUAL bool shouldPopulateFclInterface() const;
    MOCKABLE_VIRTUAL CIF::RAII::UPtr_t<IGC::PlatformTagOCL> getPlatformHandle() const;
    MOCKABLE_VIRTUAL void populateFclInterface(IGC::PlatformTagOCL &handle, const HardwareInfo &hwInfo);

    OclocArgHelper *argHelper{};
    std::unique_ptr<OsLibrary> fclLib;
    CIF::RAII::UPtr_t<CIF::CIFMain> fclMain;
    CIF::RAII::UPtr_t<IGC::FclOclDeviceCtxTagOCL> fclDeviceCtx;
    bool initialized{false};
};

static_assert(NEO::NonCopyableAndNonMovable<OclocFclFacade>);

} // namespace NEO
