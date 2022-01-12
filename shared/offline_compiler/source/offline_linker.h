/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/hw_info.h"
#include "shared/source/utilities/arrayref.h"

#include "cif/common/cif_main.h"
#include "cif/import/library_api.h"
#include "ocl_igc_interface/code_type.h"
#include "ocl_igc_interface/igc_ocl_device_ctx.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

class OclocArgHelper;

namespace NEO {

class OsLibrary;

class OfflineLinker {
  protected:
    enum class OperationMode {
        SKIP_EXECUTION = 0,
        SHOW_HELP = 1,
        LINK_FILES = 2,
    };

    struct InputFileContent {
        InputFileContent(std::unique_ptr<char[]> bytes, size_t size, IGC::CodeType::CodeType_t codeType)
            : bytes{std::move(bytes)}, size{size}, codeType{codeType} {}

        std::unique_ptr<char[]> bytes{};
        size_t size{};
        IGC::CodeType::CodeType_t codeType{};
    };

  public:
    static std::unique_ptr<OfflineLinker> create(size_t argsCount, const std::vector<std::string> &args, int &errorCode, OclocArgHelper *argHelper);
    MOCKABLE_VIRTUAL ~OfflineLinker();

    int execute();
    std::string getBuildLog() const;

  protected:
    explicit OfflineLinker(OclocArgHelper *argHelper);
    int initialize(size_t argsCount, const std::vector<std::string> &args);
    int parseCommand(size_t argsCount, const std::vector<std::string> &args);
    IGC::CodeType::CodeType_t parseOutputFormat(const std::string &outputFormatName);
    int verifyLinkerCommand();
    int loadInputFilesContent();
    IGC::CodeType::CodeType_t detectCodeType(char *bytes, size_t size) const;
    int initHardwareInfo();
    int prepareIgc();
    int link();
    int showHelp();
    std::vector<uint8_t> createSingleInputFile() const;
    std::pair<int, std::vector<uint8_t>> translateToOutputFormat(const std::vector<uint8_t> &elfInput);
    void tryToStoreBuildLog(const char *buildLogRaw, size_t size);

    MOCKABLE_VIRTUAL ArrayRef<const HardwareInfo *> getHardwareInfoTable() const;
    MOCKABLE_VIRTUAL std::unique_ptr<OsLibrary> loadIgcLibrary() const;
    MOCKABLE_VIRTUAL CIF::CreateCIFMainFunc_t loadCreateIgcMainFunction() const;
    MOCKABLE_VIRTUAL CIF::RAII::UPtr_t<CIF::CIFMain> createIgcMain(CIF::CreateCIFMainFunc_t createMainFunction) const;
    MOCKABLE_VIRTUAL CIF::RAII::UPtr_t<IGC::IgcOclDeviceCtxTagOCL> createIgcDeviceContext() const;
    MOCKABLE_VIRTUAL CIF::RAII::UPtr_t<IGC::PlatformTagOCL> getIgcPlatformHandle() const;
    MOCKABLE_VIRTUAL CIF::RAII::UPtr_t<IGC::GTSystemInfoTagOCL> getGTSystemInfoHandle() const;

    OclocArgHelper *argHelper{};
    OperationMode operationMode{};

    std::vector<std::string> inputFilenames{};
    std::vector<InputFileContent> inputFilesContent{};
    std::string outputFilename{};
    IGC::CodeType::CodeType_t outputFormat{};
    std::string options{};
    std::string internalOptions{};

    std::unique_ptr<OsLibrary> igcLib{};
    CIF::RAII::UPtr_t<CIF::CIFMain> igcMain{};
    CIF::RAII::UPtr_t<IGC::IgcOclDeviceCtxTagOCL> igcDeviceCtx{};
    HardwareInfo hwInfo{};
    std::string buildLog{};
};

} // namespace NEO