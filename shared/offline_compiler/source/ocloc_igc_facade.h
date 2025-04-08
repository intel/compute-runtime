/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/offline_compiler/source/ocloc_fcl_facade.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "cif/common/cif_main.h"
#include "cif/import/library_api.h"
#include "ocl_igc_interface/code_type.h"
#include "ocl_igc_interface/igc_ocl_device_ctx.h"

#include <memory>
#include <string>

class OclocArgHelper;

namespace NEO {

class CompilerProductHelper;
class OsLibrary;

struct HardwareInfo;

class OclocIgcFacade : NEO::NonCopyableAndNonMovableClass {
  public:
    OclocIgcFacade(OclocArgHelper *argHelper);
    MOCKABLE_VIRTUAL ~OclocIgcFacade();

    int initialize(const HardwareInfo &hwInfo);
    bool isInitialized() const;
    const char *getIgcRevision();
    size_t getIgcLibSize();
    time_t getIgcLibMTime();
    CIF::RAII::UPtr_t<CIF::Builtins::BufferLatest> createConstBuffer(const void *data, size_t size);
    CIF::RAII::UPtr_t<IGC::IgcOclTranslationCtxTagOCL> createTranslationContext(IGC::CodeType::CodeType_t inType, IGC::CodeType::CodeType_t outType);

  protected:
    MOCKABLE_VIRTUAL std::unique_ptr<OsLibrary> loadIgcLibrary(const char *libName) const;
    MOCKABLE_VIRTUAL CIF::CreateCIFMainFunc_t loadCreateIgcMainFunction() const;
    MOCKABLE_VIRTUAL CIF::RAII::UPtr_t<CIF::CIFMain> createIgcMain(CIF::CreateCIFMainFunc_t createMainFunction) const;
    MOCKABLE_VIRTUAL bool isIgcInterfaceCompatible(const std::vector<CIF::InterfaceId_t> &interfacesToIgnore) const;
    MOCKABLE_VIRTUAL std::string getIncompatibleInterface(const std::vector<CIF::InterfaceId_t> &interfacesToIgnore) const;
    MOCKABLE_VIRTUAL bool isPatchtokenInterfaceSupported() const;
    MOCKABLE_VIRTUAL CIF::RAII::UPtr_t<IGC::IgcOclDeviceCtx<3>> createIgcDeviceContext3() const;
    MOCKABLE_VIRTUAL CIF::RAII::UPtr_t<IGC::IgcOclDeviceCtxTagOCL> createIgcDeviceContext() const;
    MOCKABLE_VIRTUAL CIF::RAII::UPtr_t<IGC::PlatformTagOCL> getIgcPlatformHandle() const;
    MOCKABLE_VIRTUAL CIF::RAII::UPtr_t<IGC::GTSystemInfoTagOCL> getGTSystemInfoHandle() const;
    MOCKABLE_VIRTUAL CIF::RAII::UPtr_t<IGC::IgcFeaturesAndWorkaroundsTagOCL> getIgcFeaturesAndWorkaroundsHandle() const;
    void populateWithFeatures(IGC::IgcFeaturesAndWorkaroundsTagOCL *handle, const HardwareInfo &hwInfo, const CompilerProductHelper *compilerProductHelper) const;

    OclocArgHelper *argHelper{};
    std::unique_ptr<OsLibrary> igcLib;
    std::vector<char> igcRevision;
    size_t igcLibSize{0};
    time_t igcLibMTime{0};
    CIF::RAII::UPtr_t<CIF::CIFMain> igcMain;
    CIF::RAII::UPtr_t<IGC::IgcOclDeviceCtxTagOCL> igcDeviceCtx;
    bool initialized{false};
};

static_assert(NEO::NonCopyableAndNonMovable<OclocIgcFacade>);

class OclocIgcAsFcl : public OclocFclFacadeBase {
  public:
    OclocIgcAsFcl(OclocArgHelper *argHelper);
    ~OclocIgcAsFcl() override;

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
    IGC::CodeType::CodeType_t preferredIntermediateRepresentation = IGC::CodeType::undefined;
    std::unique_ptr<OclocIgcFacade> igc;
};

static_assert(NEO::NonCopyableAndNonMovable<OclocIgcAsFcl>);

} // namespace NEO
