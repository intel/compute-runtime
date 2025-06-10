/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "offline_compiler.h"

#include "shared/offline_compiler/source/ocloc_api.h"
#include "shared/offline_compiler/source/ocloc_arg_helper.h"
#include "shared/offline_compiler/source/ocloc_fatbinary.h"
#include "shared/offline_compiler/source/ocloc_fcl_facade.h"
#include "shared/offline_compiler/source/ocloc_igc_facade.h"
#include "shared/offline_compiler/source/ocloc_supported_devices_helper.h"
#include "shared/offline_compiler/source/queries.h"
#include "shared/offline_compiler/source/utilities/get_git_version_info.h"
#include "shared/source/compiler_interface/compiler_options.h"
#include "shared/source/compiler_interface/compiler_options_extra.h"
#include "shared/source/compiler_interface/default_cache_config.h"
#include "shared/source/compiler_interface/intermediate_representations.h"
#include "shared/source/compiler_interface/oclc_extensions.h"
#include "shared/source/compiler_interface/tokenized_string.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device_binary_format/device_binary_formats.h"
#include "shared/source/device_binary_format/elf/elf_encoder.h"
#include "shared/source/device_binary_format/elf/ocl_elf.h"
#include "shared/source/helpers/compiler_options_parser.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/string.h"
#include "shared/source/helpers/validators.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/source/utilities/io_functions.h"

#include "neo_aot_platforms.h"
#include "offline_compiler_ext.h"

#include <iomanip>
#include <iterator>
#include <list>
#include <map>
#include <set>

#ifdef _WIN32
#include <direct.h>
#define GetCurrentWorkingDirectory _getcwd
#else
#include <sys/stat.h>
#define GetCurrentWorkingDirectory getcwd
#endif

namespace NEO {

std::string convertToPascalCase(const std::string &inString) {
    std::string outString;
    bool capitalize = true;

    for (unsigned int i = 0; i < inString.length(); i++) {
        if (isalpha(inString[i]) && capitalize == true) {
            outString += toupper(inString[i]);
            capitalize = false;
        } else if (inString[i] == '_') {
            capitalize = true;
        } else {
            outString += inString[i];
        }
    }
    return outString;
}

std::string getDeprecatedDevices(OclocArgHelper *helper) {
    auto acronyms = helper->productConfigHelper->getDeprecatedAcronyms();
    return helper->createStringForArgs(acronyms);
}

std::string getSupportedDevices(OclocArgHelper *helper) {
    auto devices = helper->productConfigHelper->getAllProductAcronyms();
    auto families = helper->productConfigHelper->getFamiliesAcronyms();
    auto releases = helper->productConfigHelper->getReleasesAcronyms();
    auto familiesAndReleases = helper->getArgsWithoutDuplicate(families, releases);

    return helper->createStringForArgs(devices, familiesAndReleases);
}

OfflineCompiler::OfflineCompiler() = default;
OfflineCompiler::~OfflineCompiler() {
    pBuildInfo.reset();
    delete[] irBinary;
    delete[] genBinary;
    delete[] debugDataBinary;
}

OfflineCompiler *OfflineCompiler::create(size_t numArgs, const std::vector<std::string> &allArgs, bool dumpFiles, int &retVal, OclocArgHelper *helper) {
    retVal = OCLOC_SUCCESS;
    auto pOffCompiler = new OfflineCompiler();

    if (pOffCompiler) {
        pOffCompiler->argHelper = helper;
        pOffCompiler->fclFacade = std::make_unique<OclocFclFacade>(helper);
        pOffCompiler->igcFacade = std::make_unique<OclocIgcFacade>(helper);
        retVal = pOffCompiler->initialize(numArgs, allArgs, dumpFiles);
        if (pOffCompiler->useIgcAsFcl()) {
            pOffCompiler->fclFacade = std::make_unique<OclocIgcAsFcl>(helper);
        }
    }

    if (retVal != OCLOC_SUCCESS) {
        delete pOffCompiler;
        pOffCompiler = nullptr;
    }

    return pOffCompiler;
}

void printQueryHelp(OclocArgHelper *helper) {
    helper->printf(OfflineCompiler::queryHelp.data());
}

NameVersionPair::NameVersionPair(ConstStringRef name, unsigned int version) {
    this->version = version;
    this->name[OCLOC_NAME_VERSION_MAX_NAME_SIZE - 1] = '\0';
    strncpy_s(this->name, sizeof(this->name), name.data(), name.size());
};

std::vector<NameVersionPair> OfflineCompiler::getExtensions(ConstStringRef product, bool needVersions, OclocArgHelper *helper) {
    std::vector<NameVersionPair> ret;
    std::vector<std::string> args;
    args.push_back("ocloc");
    args.push_back("-device");
    args.push_back(product.str());
    int retVal = 0;
    std::unique_ptr<OfflineCompiler> compiler{OfflineCompiler::create(args.size(), args, true, retVal, helper)};
    if (nullptr == compiler) {
        return {};
    }
    auto extensionsStr = compiler->compilerProductHelper->getDeviceExtensions(compiler->hwInfo, compiler->releaseHelper.get());
    auto extensions = NEO::CompilerOptions::tokenize(extensionsStr, ' ');
    ret.reserve(extensions.size());
    for (const auto &ext : extensions) {
        unsigned int version = 0;
        if (needVersions) {
            version = NEO::getOclCExtensionVersion(ext.str(), CL_MAKE_VERSION(1u, 0, 0));
        }
        ret.emplace_back(ext, version);
    }
    return ret;
}

std::vector<NameVersionPair> OfflineCompiler::getOpenCLCVersions(ConstStringRef product, OclocArgHelper *helper) {
    std::vector<std::string> args;
    args.push_back("ocloc");
    args.push_back("-device");
    args.push_back(product.str());
    int retVal = 0;
    std::unique_ptr<OfflineCompiler> compiler{OfflineCompiler::create(args.size(), args, true, retVal, helper)};
    if (nullptr == compiler) {
        return {};
    }

    auto deviceOpenCLCVersions = compiler->compilerProductHelper->getDeviceOpenCLCVersions(compiler->getHardwareInfo(), {});
    NameVersionPair openClCVersion{"OpenCL C", 0};

    std::vector<NameVersionPair> allSupportedVersions;
    for (auto &ver : deviceOpenCLCVersions) {
        openClCVersion.version = CL_MAKE_VERSION(ver.major, ver.minor, 0);
        allSupportedVersions.push_back(openClCVersion);
    }
    return allSupportedVersions;
}

std::vector<NameVersionPair> OfflineCompiler::getOpenCLCFeatures(ConstStringRef product, OclocArgHelper *helper) {
    std::vector<std::string> args;
    args.push_back("ocloc");
    args.push_back("-device");
    args.push_back(product.str());
    int retVal = 0;
    std::unique_ptr<OfflineCompiler> compiler{OfflineCompiler::create(args.size(), args, true, retVal, helper)};
    if (nullptr == compiler) {
        return {};
    }

    OpenClCFeaturesContainer availableFeatures;
    NEO::getOpenclCFeaturesList(compiler->getHardwareInfo(), availableFeatures, *compiler->compilerProductHelper, compiler->releaseHelper.get());

    std::vector<NameVersionPair> allSupportedFeatures;
    for (auto &feature : availableFeatures) {
        allSupportedFeatures.push_back({feature.name, feature.version});
    }
    return allSupportedFeatures;
}

std::vector<NameVersionPair> getCommonNameVersion(const std::vector<std::vector<NameVersionPair>> &perTarget) {
    std::vector<NameVersionPair> commonExtensionsVec;
    if (perTarget.size() == 1) {
        commonExtensionsVec = perTarget[0];
    } else {
        std::map<std::string, std::map<unsigned int, int>> allExtensions;
        for (const auto &targetExt : perTarget) {
            for (const auto &ext : targetExt) {
                ++allExtensions[ext.name][ext.version];
            }
        }
        int numTargets = static_cast<int>(perTarget.size());
        for (const auto &extScore : allExtensions) {
            for (const auto &version : extScore.second) {
                if ((version.second == numTargets)) {
                    commonExtensionsVec.emplace_back(ConstStringRef(extScore.first), version.first);
                }
            }
        }
    }
    return commonExtensionsVec;
}

std::vector<NameVersionPair> getCommonExtensions(std::vector<ConstStringRef> products, bool needVersions, OclocArgHelper *helper) {
    std::vector<std::vector<NameVersionPair>> perTarget;
    for (const auto &targetProduct : products) {
        auto extensions = OfflineCompiler::getExtensions(targetProduct, needVersions, helper);
        perTarget.push_back(extensions);
    }

    auto commonExtensions = getCommonNameVersion(perTarget);
    return commonExtensions;
}

std::vector<NameVersionPair> getCommonOpenCLCVersions(std::vector<ConstStringRef> products, OclocArgHelper *helper) {
    std::vector<std::vector<NameVersionPair>> perTarget;
    for (const auto &targetProduct : products) {
        auto versions = OfflineCompiler::getOpenCLCVersions(targetProduct, helper);
        perTarget.push_back(versions);
    }

    return getCommonNameVersion(perTarget);
}

std::vector<NameVersionPair> getCommonOpenCLCFeatures(std::vector<ConstStringRef> products, OclocArgHelper *helper) {
    std::vector<std::vector<NameVersionPair>> perTarget;
    for (const auto &targetProduct : products) {
        auto features = OfflineCompiler::getOpenCLCFeatures(targetProduct, helper);
        perTarget.push_back(features);
    }

    return getCommonNameVersion(perTarget);
}

std::string formatNameVersionString(std::vector<NameVersionPair> extensions, bool needVersions) {
    std::vector<std::string> formatedExtensions;
    formatedExtensions.reserve(extensions.size());
    for (const auto &ext : extensions) {
        formatedExtensions.push_back({});
        auto it = formatedExtensions.rbegin();
        bool needsQuoutes = (nullptr != strstr(ext.name, " "));
        it->reserve(strnlen_s(ext.name, sizeof(ext.name)) + (needsQuoutes ? 2 : 0) + (needVersions ? 16 : 0));
        if (needsQuoutes) {
            it->append("\"");
        }
        it->append(ext.name);
        if (needsQuoutes) {
            it->append("\"");
        }
        if (needVersions) {
            it->append(":");
            it->append(std::to_string(CL_VERSION_MAJOR(ext.version)));
            it->append(".");
            it->append(std::to_string(CL_VERSION_MINOR(ext.version)));
            it->append(".");
            it->append(std::to_string(CL_VERSION_PATCH(ext.version)));
        }
    }
    return ConstStringRef(" ").join(formatedExtensions);
}

int OfflineCompiler::query(size_t numArgs, const std::vector<std::string> &allArgs, OclocArgHelper *helper) {
    if (allArgs.size() < 3) {
        helper->printf("Error: Invalid command line. Expected ocloc query <argument>. See ocloc query --help\n");
        return OCLOC_INVALID_COMMAND_LINE;
    }

    Ocloc::SupportedDevicesMode supportedDevicesMode = Ocloc::SupportedDevicesMode::concat;

    std::vector<ConstStringRef> targetProducts;
    std::vector<Queries::QueryType> queries;
    auto argIt = allArgs.begin() + 2;
    std::string deviceArg = "*";
    while (argIt != allArgs.end()) {
        if ("-device" == *argIt) {
            if (argIt + 1 == allArgs.end()) {
                helper->printf("Error: Invalid command line : -device must be followed by device name\n");
            }
            ++argIt;
            deviceArg = *argIt;
        } else if (Queries::queryNeoRevision == *argIt) {
            queries.push_back(Queries::QueryType::neoRevision);
        } else if (Queries::queryOCLDriverVersion == *argIt) {
            queries.push_back(Queries::QueryType::oclDriverVersion);
        } else if (Queries::queryIgcRevision == *argIt) {
            queries.push_back(Queries::QueryType::igcRevision);
        } else if (Queries::queryOCLDeviceExtensions == *argIt) {
            queries.push_back(Queries::QueryType::oclDeviceExtensions);
        } else if (Queries::queryOCLDeviceExtensionsWithVersion == *argIt) {
            queries.push_back(Queries::QueryType::oclDeviceExtensionsWithVersion);
        } else if (Queries::queryOCLDeviceProfile == *argIt) {
            queries.push_back(Queries::QueryType::oclDeviceProfile);
        } else if (Queries::queryOCLDeviceOpenCLCAllVersions == *argIt) {
            queries.push_back(Queries::QueryType::oclDeviceOpenCLCAllVersions);
        } else if (Queries::queryOCLDeviceOpenCLCFeatures == *argIt) {
            queries.push_back(Queries::QueryType::oclDeviceOpenCLCFeatures);
        } else if (Queries::querySupportedDevices == *argIt) {
            queries.push_back(Queries::QueryType::supportedDevices);
            if (argIt + 1 != allArgs.end()) {
                auto newMode = Ocloc::parseSupportedDevicesMode(*(argIt + 1));
                if (newMode != Ocloc::SupportedDevicesMode::unknown) {
                    supportedDevicesMode = newMode;
                    ++argIt;
                }
            }
        } else if ("--help" == *argIt) {
            printQueryHelp(helper);
            return 0;
        } else {
            helper->printf("Error: Invalid command line.\nUnknown argument %s\n", argIt->c_str());
            return OCLOC_INVALID_COMMAND_LINE;
        }

        ++argIt;
    }

    if (NEO::requestedFatBinary(deviceArg, helper)) {
        targetProducts = NEO::getTargetProductsForFatbinary(deviceArg, helper);
    } else {
        targetProducts.push_back(deviceArg);
    }

    for (const auto &queryType : queries) {
        switch (queryType) {
        default:
            helper->printf("Error: Invalid command line. See ocloc query --help\n");
            break;
        case Queries::QueryType::neoRevision: {
            auto revision = NEO::getRevision();
            helper->saveOutput(Queries::queryNeoRevision.str(), revision.c_str(), revision.size() + 1);
            helper->printf("%s\n", revision.c_str());
        } break;
        case Queries::QueryType::oclDriverVersion: {
            auto driverVersion = NEO::getOclDriverVersion();
            helper->saveOutput(Queries::queryOCLDriverVersion.str(), driverVersion.c_str(), driverVersion.size() + 1);
            helper->printf("%s\n", driverVersion.c_str());
        } break;
        case Queries::QueryType::igcRevision: {
            auto igcFacade = std::make_unique<OclocIgcFacade>(helper);
            NEO::HardwareInfo hwInfo{};
            auto initResult = igcFacade->initialize(hwInfo);
            if (initResult == OCLOC_SUCCESS) {
                auto igcRev = igcFacade->getIgcRevision();
                helper->saveOutput(Queries::queryIgcRevision.str(), igcRev, strlen(igcRev) + 1);
                helper->printf("%s\n", igcRev);
            }
        } break;
        case Queries::QueryType::oclDeviceExtensions: {
            auto commonExtensionsVec = getCommonExtensions(targetProducts, false, helper);
            auto commonExtString = formatNameVersionString(commonExtensionsVec, false);
            helper->saveOutput(Queries::queryOCLDeviceExtensions.str(), commonExtString.c_str(), commonExtString.size());
            helper->printf("%s\n", commonExtString.c_str());
        } break;
        case Queries::QueryType::oclDeviceExtensionsWithVersion: {
            auto commonExtensionsVec = getCommonExtensions(targetProducts, true, helper);
            auto commonExtString = formatNameVersionString(commonExtensionsVec, true);
            helper->saveOutput(Queries::queryOCLDeviceExtensionsWithVersion.str(), commonExtensionsVec.data(), commonExtensionsVec.size() * sizeof(NameVersionPair));
            helper->printf("%s\n", commonExtString.c_str());
        } break;
        case Queries::QueryType::oclDeviceProfile: {
            ConstStringRef profile{"FULL_PROFILE\n"};
            helper->saveOutput(Queries::queryOCLDeviceProfile.str(), profile.data(), profile.size());
            helper->printf("%s\n", profile.data());
        } break;
        case Queries::QueryType::oclDeviceOpenCLCAllVersions: {
            auto commonVersionsVec = getCommonOpenCLCVersions(targetProducts, helper);
            auto commonVersionsString = formatNameVersionString(commonVersionsVec, true);
            helper->saveOutput(Queries::queryOCLDeviceOpenCLCAllVersions.str(), commonVersionsVec.data(), commonVersionsVec.size() * sizeof(NameVersionPair));
            helper->printf("%s\n", commonVersionsString.c_str());
        } break;
        case Queries::QueryType::oclDeviceOpenCLCFeatures: {
            auto commonFeaturesVec = getCommonOpenCLCFeatures(targetProducts, helper);
            auto commonFeaturesString = formatNameVersionString(commonFeaturesVec, true);
            helper->saveOutput(Queries::queryOCLDeviceOpenCLCFeatures.str(), commonFeaturesVec.data(), commonFeaturesVec.size() * sizeof(NameVersionPair));
            helper->printf("%s\n", commonFeaturesString.c_str());
        } break;
        case Queries::QueryType::supportedDevices: {
            querySupportedDevices(supportedDevicesMode, helper);
        } break;
        }
    }

    return OCLOC_SUCCESS;
}

void printAcronymIdsHelp(OclocArgHelper *helper) {
    helper->printf(OfflineCompiler::idsHelp.data(), getSupportedDevices(helper).c_str());
}

int OfflineCompiler::queryAcronymIds(size_t numArgs, const std::vector<std::string> &allArgs, OclocArgHelper *helper) {
    const size_t numArgRequested = 3u;
    auto retVal = OCLOC_SUCCESS;

    if (numArgs != numArgRequested) {
        helper->printf("Error: Invalid command line. Expected ocloc ids <acronym>.\n");
        retVal = OCLOC_INVALID_COMMAND_LINE;
        return retVal;
    } else if ((ConstStringRef("-h") == allArgs[2] || ConstStringRef("--help") == allArgs[2])) {
        printAcronymIdsHelp(helper);
        return retVal;
    }

    std::string queryAcronym = allArgs[2];
    ProductConfigHelper::adjustDeviceName(queryAcronym);

    auto &enabledDevices = helper->productConfigHelper->getDeviceAotInfo();
    std::vector<std::string> matchedVersions{};

    auto family = helper->productConfigHelper->getFamilyFromDeviceName(queryAcronym);
    auto release = helper->productConfigHelper->getReleaseFromDeviceName(queryAcronym);
    auto product = helper->productConfigHelper->getProductConfigFromDeviceName(queryAcronym);

    if (family != AOT::UNKNOWN_FAMILY) {
        for (const auto &device : enabledDevices) {
            if (device.family == family) {
                matchedVersions.push_back(ProductConfigHelper::parseMajorMinorRevisionValue(device.aotConfig));
            }
        }
    } else if (release != AOT::UNKNOWN_RELEASE) {
        for (const auto &device : enabledDevices) {
            if (device.release == release) {
                matchedVersions.push_back(ProductConfigHelper::parseMajorMinorRevisionValue(device.aotConfig));
            }
        }
    } else if (product != AOT::UNKNOWN_ISA) {
        for (const auto &device : enabledDevices) {
            if (device.aotConfig.value == product) {
                matchedVersions.push_back(ProductConfigHelper::parseMajorMinorRevisionValue(device.aotConfig));
            }
        }
    } else {
        bool isMatched{false};
        if (auto hwInfoDepAcr = getHwInfoForDeprecatedAcronym(queryAcronym); hwInfoDepAcr) {
            if (auto compilerProductHelper = NEO::CompilerProductHelper::create(hwInfoDepAcr->platform.eProductFamily); compilerProductHelper) {
                matchedVersions.push_back(ProductConfigHelper::parseMajorMinorRevisionValue(compilerProductHelper->getDefaultHwIpVersion()));
                isMatched = true;
            }
        }
        if (!isMatched) {
            helper->printf("Error: Invalid command line. Unknown acronym %s.\n", allArgs[2].c_str());
            retVal = OCLOC_INVALID_COMMAND_LINE;
            return retVal;
        }
    }

    std::ostringstream os;
    for (const auto &prefix : matchedVersions) {
        if (os.tellp())
            os << "\n";
        os << prefix;
    }
    helper->printf("Matched ids:\n%s\n", os.str().c_str());

    return retVal;
}

int OfflineCompiler::querySupportedDevices(Ocloc::SupportedDevicesMode mode, OclocArgHelper *helper) {
    auto enabledDevices = helper->productConfigHelper->getDeviceAotInfo();

    Ocloc::SupportedDevicesHelper supportedDevicesHelper(mode, helper->productConfigHelper.get());
    auto supportedDevicesData = supportedDevicesHelper.collectSupportedDevicesData(enabledDevices);

    std::string output;

    if (mode == Ocloc::SupportedDevicesMode::merge) {
        output = supportedDevicesHelper.mergeAndSerializeWithFormerData(supportedDevicesData);
    } else {
        output = supportedDevicesHelper.concatAndSerializeWithFormerData(supportedDevicesData);
    }

    helper->saveOutput(supportedDevicesHelper.getCurrentOclocOutputFilename(), output.data(), output.size());

    return 0;
}

struct OfflineCompiler::BuildInfo {
    std::unique_ptr<CIF::Builtins::BufferLatest, CIF::RAII::ReleaseHelper<CIF::Builtins::BufferLatest>> fclOptions;
    std::unique_ptr<CIF::Builtins::BufferLatest, CIF::RAII::ReleaseHelper<CIF::Builtins::BufferLatest>> fclInternalOptions;
    std::unique_ptr<IGC::OclTranslationOutputTagOCL, CIF::RAII::ReleaseHelper<IGC::OclTranslationOutputTagOCL>> fclOutput;
    IGC::CodeType::CodeType_t intermediateRepresentation;
};

int OfflineCompiler::buildToIrBinary() {
    int retVal = OCLOC_SUCCESS;

    if (allowCaching) {
        const std::string igcRevision = igcFacade->getIgcRevision();
        const auto igcLibSize = igcFacade->getIgcLibSize();
        const auto igcLibMTime = igcFacade->getIgcLibMTime();
        irHash = cache->getCachedFileName(getHardwareInfo(),
                                          sourceCode,
                                          options,
                                          internalOptions, ArrayRef<const char>(), ArrayRef<const char>(), igcRevision, igcLibSize, igcLibMTime);
        irBinary = cache->loadCachedBinary(irHash, irBinarySize).release();
        if (irBinary) {
            return retVal;
        }
    }

    UNRECOVERABLE_IF(!fclFacade->isInitialized());

    // sourceCode.size() returns the number of characters without null terminated char
    CIF::RAII::UPtr_t<CIF::Builtins::BufferLatest> fclSrc = nullptr;
    pBuildInfo->fclOptions = fclFacade->createConstBuffer(options.c_str(), options.size());
    pBuildInfo->fclInternalOptions = fclFacade->createConstBuffer(internalOptions.c_str(), internalOptions.size());
    auto err = fclFacade->createConstBuffer(nullptr, 0);

    auto srcType = IGC::CodeType::undefined;
    std::vector<uint8_t> tempSrcStorage;
    if (this->argHelper->hasHeaders()) {
        srcType = IGC::CodeType::elf;

        NEO::Elf::ElfEncoder<> elfEncoder(true, true, 1U);
        elfEncoder.getElfFileHeader().type = NEO::Elf::ET_OPENCL_SOURCE;
        elfEncoder.appendSection(NEO::Elf::SHT_OPENCL_SOURCE, "CLMain", sourceCode);

        for (const auto &header : this->argHelper->getHeaders()) {
            ArrayRef<const uint8_t> headerData(header.data, header.length);
            ConstStringRef headerName = header.name;

            elfEncoder.appendSection(NEO::Elf::SHT_OPENCL_HEADER, headerName, headerData);
        }
        tempSrcStorage = elfEncoder.encode();
        fclSrc = fclFacade->createConstBuffer(tempSrcStorage.data(), tempSrcStorage.size());
    } else {
        srcType = IGC::CodeType::oclC;
        fclSrc = fclFacade->createConstBuffer(sourceCode.c_str(), sourceCode.size() + 1);
    }

    if (false == NEO::areNotNullptr(fclSrc.get(), pBuildInfo->fclOptions.get(), pBuildInfo->fclInternalOptions.get())) {
        retVal = OCLOC_OUT_OF_HOST_MEMORY;
        return retVal;
    }

    pBuildInfo->fclOutput = fclFacade->translate(srcType, intermediateRepresentation, err.get(),
                                                 fclSrc.get(), pBuildInfo->fclOptions.get(),
                                                 pBuildInfo->fclInternalOptions.get(), nullptr, 0);

    if (true == NEO::areNotNullptr(err->GetMemory<char>())) {
        updateBuildLog(err->GetMemory<char>(), err->GetSizeRaw());
        retVal = OCLOC_BUILD_PROGRAM_FAILURE;
        return retVal;
    }

    if (pBuildInfo->fclOutput == nullptr) {
        retVal = OCLOC_OUT_OF_HOST_MEMORY;
        return retVal;
    }

    UNRECOVERABLE_IF(pBuildInfo->fclOutput->GetBuildLog() == nullptr);
    UNRECOVERABLE_IF(pBuildInfo->fclOutput->GetOutput() == nullptr);

    if (pBuildInfo->fclOutput->Successful() == false) {
        updateBuildLog(pBuildInfo->fclOutput->GetBuildLog()->GetMemory<char>(), pBuildInfo->fclOutput->GetBuildLog()->GetSizeRaw());
        retVal = OCLOC_BUILD_PROGRAM_FAILURE;
        return retVal;
    }

    pBuildInfo->intermediateRepresentation = intermediateRepresentation;
    storeBinary(irBinary, irBinarySize, pBuildInfo->fclOutput->GetOutput()->GetMemory<char>(), pBuildInfo->fclOutput->GetOutput()->GetSizeRaw());

    updateBuildLog(pBuildInfo->fclOutput->GetBuildLog()->GetMemory<char>(), pBuildInfo->fclOutput->GetBuildLog()->GetSizeRaw());

    if (allowCaching) {
        cache->cacheBinary(irHash, irBinary, static_cast<uint32_t>(irBinarySize));
    }

    return retVal;
}

std::string OfflineCompiler::validateInputType(const std::string &input, bool isLlvm, bool isSpirv) {
    auto asBitcode = ArrayRef<const uint8_t>::fromAny(input.data(), input.size());
    if (isSpirv) {
        if (NEO::isSpirVBitcode(asBitcode)) {
            return "";
        }
        return "Warning : file does not look like spirv bitcode (wrong magic numbers)";
    }

    if (isLlvm) {
        if (NEO::isLlvmBitcode(asBitcode)) {
            return "";
        }
        return "Warning : file does not look like llvm bitcode (wrong magic numbers)";
    }

    if (NEO::isSpirVBitcode(asBitcode)) {
        return "Warning : file looks like spirv bitcode (based on magic numbers) - please make sure proper CLI flags are present";
    }

    if (NEO::isLlvmBitcode(asBitcode)) {
        return "Warning : file looks like llvm bitcode (based on magic numbers) - please make sure proper CLI flags are present";
    }

    return "";
}

int OfflineCompiler::buildSourceCode() {
    int retVal = OCLOC_SUCCESS;

    if (sourceCode.empty()) {
        return OCLOC_INVALID_PROGRAM;
    }
    auto inputTypeWarnings = validateInputType(sourceCode, inputFileLlvm(), inputFileSpirV());
    this->argHelper->printf(inputTypeWarnings.c_str());
    if (isIntermediateRepresentation(this->inputCodeType)) {
        storeBinary(irBinary, irBinarySize, sourceCode.c_str(), sourceCode.size());
        pBuildInfo->intermediateRepresentation = this->inputCodeType;
    } else {
        pBuildInfo->intermediateRepresentation = (this->intermediateRepresentation != IGC::CodeType::undefined) ? this->intermediateRepresentation : this->preferredIntermediateRepresentation;
        retVal = buildToIrBinary();
        if (retVal != OCLOC_SUCCESS)
            return retVal;
    }

    const std::string igcRevision = igcFacade->getIgcRevision();
    const auto igcLibSize = igcFacade->getIgcLibSize();
    const auto igcLibMTime = igcFacade->getIgcLibMTime();
    const bool generateDebugInfo = CompilerOptions::contains(options, CompilerOptions::generateDebugInfo);

    if (allowCaching) {
        genHash = cache->getCachedFileName(getHardwareInfo(), ArrayRef<const char>(irBinary, irBinarySize), options, internalOptions, ArrayRef<const char>(), ArrayRef<const char>(), igcRevision, igcLibSize, igcLibMTime);
        if (generateDebugInfo) {
            dbgHash = cache->getCachedFileName(getHardwareInfo(), irHash, options, internalOptions, ArrayRef<const char>(), ArrayRef<const char>(), igcRevision, igcLibSize, igcLibMTime);
        }

        genBinary = cache->loadCachedBinary(genHash, genBinarySize).release();
        if (genBinary) {
            bool isZebin = isDeviceBinaryFormat<DeviceBinaryFormat::zebin>(ArrayRef<uint8_t>(reinterpret_cast<uint8_t *>(genBinary), genBinarySize));
            if (!generateDebugInfo || isZebin) {
                return retVal;
            }
            debugDataBinary = cache->loadCachedBinary(dbgHash, debugDataBinarySize).release();
            if (debugDataBinary) {
                return retVal;
            }
        }

        delete[] genBinary;
        genBinary = nullptr;
        genBinarySize = 0;
    }

    UNRECOVERABLE_IF(!igcFacade->isInitialized());

    auto igcTranslationCtx = igcFacade->createTranslationContext(pBuildInfo->intermediateRepresentation, IGC::CodeType::oclGenBin);

    auto igcSrc = igcFacade->createConstBuffer(irBinary, irBinarySize);
    auto igcOptions = igcFacade->createConstBuffer(options.c_str(), options.size());
    auto igcInternalOptions = igcFacade->createConstBuffer(internalOptions.c_str(), internalOptions.size());
    auto igcOutput = igcTranslationCtx->Translate(igcSrc.get(), igcOptions.get(), igcInternalOptions.get(), nullptr, 0);

    if (igcOutput == nullptr) {
        return OCLOC_OUT_OF_HOST_MEMORY;
    }

    UNRECOVERABLE_IF(igcOutput->GetBuildLog() == nullptr);
    UNRECOVERABLE_IF(igcOutput->GetOutput() == nullptr);
    updateBuildLog(igcOutput->GetBuildLog()->GetMemory<char>(), igcOutput->GetBuildLog()->GetSizeRaw());

    if (igcOutput->GetOutput()->GetSizeRaw() != 0) {
        storeBinary(genBinary, genBinarySize, igcOutput->GetOutput()->GetMemory<char>(), igcOutput->GetOutput()->GetSizeRaw());
    }
    if (igcOutput->GetDebugData()->GetSizeRaw() != 0) {
        storeBinary(debugDataBinary, debugDataBinarySize, igcOutput->GetDebugData()->GetMemory<char>(), igcOutput->GetDebugData()->GetSizeRaw());
    }
    if (allowCaching) {
        cache->cacheBinary(genHash, genBinary, static_cast<uint32_t>(genBinarySize));
        cache->cacheBinary(dbgHash, debugDataBinary, static_cast<uint32_t>(debugDataBinarySize));
    }

    retVal = igcOutput->Successful() ? OCLOC_SUCCESS : OCLOC_BUILD_PROGRAM_FAILURE;

    return retVal;
}

int OfflineCompiler::build() {
    if (inputFile.empty()) {
        argHelper->printf("Error: Input file name missing.\n");
        return OCLOC_INVALID_COMMAND_LINE;
    } else if (!argHelper->fileExists(inputFile)) {
        argHelper->printf("Error: Input file %s missing.\n", inputFile.c_str());
        return OCLOC_INVALID_FILE;
    }

    const char *source = nullptr;
    std::unique_ptr<char[]> sourceFromFile;
    size_t sourceFromFileSize = 0;
    sourceFromFile = argHelper->loadDataFromFile(inputFile, sourceFromFileSize);
    if (sourceFromFileSize == 0) {
        return OCLOC_INVALID_FILE;
    }
    if (this->inputCodeType == IGC::CodeType::oclC) {
        // for text input, we also accept files used as runtime builtins
        source = strstr((const char *)sourceFromFile.get(), "R\"===(");
        sourceCode = (source != nullptr) ? getStringWithinDelimiters(sourceFromFile.get()) : sourceFromFile.get();
    } else {
        // use the binary input "as is"
        sourceCode.assign(sourceFromFile.get(), sourceFromFileSize);
    }

    if (this->inputCodeType == IGC::CodeType::oclC) {
        const auto fclInitializationResult = fclFacade->initialize(hwInfo);
        if (fclInitializationResult != OCLOC_SUCCESS) {
            argHelper->printf("Error! FCL initialization failure. Error code = %d\n", fclInitializationResult);
            return fclInitializationResult;
        }

        preferredIntermediateRepresentation = fclFacade->getPreferredIntermediateRepresentation();
    } else {
        if (!isQuiet()) {
            argHelper->printf("Compilation from IR - skipping loading of FCL\n");
        }
        preferredIntermediateRepresentation = IGC::CodeType::spirV;
    }

    if (intermediateRepresentation == IGC::CodeType::undefined) {
        intermediateRepresentation = preferredIntermediateRepresentation;
    }

    const auto igcInitializationResult = igcFacade->initialize(hwInfo);
    if (igcInitializationResult != OCLOC_SUCCESS) {
        argHelper->printf("Error! IGC initialization failure. Error code = %d\n", igcInitializationResult);
        return igcInitializationResult;
    }

    int retVal = OCLOC_SUCCESS;
    if (isOnlySpirV()) {
        retVal = buildToIrBinary();
    } else {
        retVal = buildSourceCode();
    }
    generateElfBinary();
    if (dumpFiles) {
        writeOutAllFiles();
    }

    return retVal;
}

void OfflineCompiler::updateBuildLog(const char *pErrorString, const size_t errorStringSize) {
    if (pErrorString != nullptr) {
        std::string log(pErrorString, pErrorString + errorStringSize);
        ConstStringRef errorString(log);
        const bool warningFound = errorString.containsCaseInsensitive("warning");
        if (!isQuiet() || !warningFound) {
            if (buildLog.empty()) {
                buildLog.assign(errorString.data());
            } else {
                buildLog.append("\n");
                buildLog.append(errorString.data());
            }
        }
    }
}

std::string &OfflineCompiler::getBuildLog() {
    return buildLog;
}

const HardwareInfo *getHwInfoForDeprecatedAcronym(const std::string &deviceName) {
    std::vector<PRODUCT_FAMILY> allSupportedProduct{ALL_SUPPORTED_PRODUCT_FAMILIES};
    auto deviceNameLowered = deviceName;
    std::transform(deviceNameLowered.begin(), deviceNameLowered.end(), deviceNameLowered.begin(), ::tolower);
    for (const auto &product : allSupportedProduct) {
        if (0 == strcmp(deviceNameLowered.c_str(), hardwarePrefix[product])) {
            return hardwareInfoTable[product];
        }
    }
    return nullptr;
}

int OfflineCompiler::initHardwareInfoForDeprecatedAcronyms(const std::string &deviceName, std::unique_ptr<NEO::CompilerProductHelper> &compilerProductHelper, std::unique_ptr<NEO::ReleaseHelper> &releaseHelper) {
    auto foundHwInfo = getHwInfoForDeprecatedAcronym(deviceName);
    if (nullptr == foundHwInfo) {
        return OCLOC_INVALID_DEVICE;
    }
    hwInfo = *foundHwInfo;
    if (revisionId != -1) {
        hwInfo.platform.usRevId = revisionId;
    }
    compilerProductHelper = NEO::CompilerProductHelper::create(hwInfo.platform.eProductFamily);
    auto defaultIpVersion = compilerProductHelper->getDefaultHwIpVersion();
    auto productConfig = compilerProductHelper->matchRevisionIdWithProductConfig(defaultIpVersion, revisionId);
    hwInfo.ipVersion = argHelper->productConfigHelper->isSupportedProductConfig(productConfig) ? productConfig : defaultIpVersion;

    uint64_t config = hwInfoConfig ? hwInfoConfig : compilerProductHelper->getHwInfoConfig(hwInfo);
    setHwInfoValuesFromConfig(config, hwInfo);
    releaseHelper = NEO::ReleaseHelper::create(hwInfo.ipVersion);
    hardwareInfoBaseSetup[hwInfo.platform.eProductFamily](&hwInfo, true, releaseHelper.get());
    UNRECOVERABLE_IF(compilerProductHelper == nullptr);
    productFamilyName = hardwarePrefix[hwInfo.platform.eProductFamily];

    return OCLOC_SUCCESS;
}

bool OfflineCompiler::isArgumentDeviceId(const std::string &argument) const {
    const char hexPrefixLength = 2;
    return argument.substr(0, hexPrefixLength) == "0x" && std::all_of(argument.begin() + hexPrefixLength, argument.end(), (::isxdigit));
}

int OfflineCompiler::initHardwareInfoForProductConfig(std::string deviceName) {
    uint32_t productConfig = AOT::UNKNOWN_ISA;
    ProductConfigHelper::adjustDeviceName(deviceName);

    if (isArgumentDeviceId(deviceName)) {
        auto deviceID = static_cast<unsigned short>(std::stoi(deviceName, 0, 16));
        productConfig = argHelper->getProductConfigAndSetHwInfoBasedOnDeviceAndRevId(hwInfo, deviceID, revisionId, compilerProductHelper, releaseHelper);
        if (productConfig == AOT::UNKNOWN_ISA) {
            return OCLOC_INVALID_DEVICE;
        }
        auto product = argHelper->productConfigHelper->getAcronymForProductConfig(productConfig);
        argHelper->printf("Auto-detected target based on %s device id: %s\n", deviceName.c_str(), product.c_str());
    } else if (revisionId == -1) {
        productConfig = argHelper->productConfigHelper->getProductConfigFromDeviceName(deviceName);
        if (!argHelper->setHwInfoForProductConfig(productConfig, hwInfo, compilerProductHelper, releaseHelper)) {
            return OCLOC_INVALID_DEVICE;
        }
    } else {
        return OCLOC_INVALID_DEVICE;
    }
    argHelper->setHwInfoForHwInfoConfig(hwInfo, hwInfoConfig, compilerProductHelper, releaseHelper);
    deviceConfig = hwInfo.ipVersion.value;
    productFamilyName = hardwarePrefix[hwInfo.platform.eProductFamily];
    return OCLOC_SUCCESS;
}

int OfflineCompiler::initHardwareInfo(std::string deviceName) {
    int retVal = OCLOC_INVALID_DEVICE;
    if (deviceName.empty()) {
        return retVal;
    }

    retVal = initHardwareInfoForProductConfig(deviceName);
    if (retVal == OCLOC_SUCCESS) {
        return retVal;
    }

    retVal = initHardwareInfoForDeprecatedAcronyms(deviceName, compilerProductHelper, releaseHelper);
    if (retVal != OCLOC_SUCCESS) {
        argHelper->printf("Could not determine device target: %s.\n", deviceName.c_str());
    }
    return retVal;
}

std::string OfflineCompiler::getStringWithinDelimiters(const std::string &src) {
    size_t start = src.find("R\"===(");
    size_t stop = src.find(")===\"");

    DEBUG_BREAK_IF(std::string::npos == start);
    DEBUG_BREAK_IF(std::string::npos == stop);

    start += strlen("R\"===(");
    size_t size = stop - start;

    std::string dst(src, start, size + 1);
    dst[size] = '\0'; // put null char at the end

    return dst;
}

int OfflineCompiler::initialize(size_t numArgs, const std::vector<std::string> &allArgs, bool dumpFiles) {
    this->dumpFiles = dumpFiles;
    int retVal = OCLOC_SUCCESS;
    this->pBuildInfo = std::make_unique<BuildInfo>();
    retVal = parseCommandLine(numArgs, allArgs);
    if (showHelp) {
        printUsage();
        return retVal;
    } else if (retVal != OCLOC_SUCCESS) {
        return retVal;
    }

    if (options.empty()) {
        // try to read options from file if not provided by commandline
        size_t extStart = inputFile.find_last_of(".");
        if (extStart != std::string::npos) {
            std::string oclocOptionsFileName = inputFile.substr(0, extStart);
            oclocOptionsFileName.append("_ocloc_options.txt");

            std::string oclocOptionsFromFile;
            bool oclocOptionsRead = readOptionsFromFile(oclocOptionsFromFile, oclocOptionsFileName, argHelper);
            if (oclocOptionsRead) {
                if (!isQuiet()) {
                    argHelper->printf("Building with ocloc options:\n%s\n", oclocOptionsFromFile.c_str());
                }
                std::istringstream iss(allArgs[0] + " " + oclocOptionsFromFile);
                std::vector<std::string> tokens{
                    std::istream_iterator<std::string>{iss}, std::istream_iterator<std::string>{}};

                retVal = parseCommandLine(tokens.size(), tokens);
                if (retVal != OCLOC_SUCCESS) {
                    argHelper->printf("Failed with ocloc options from file:\n%s\n", oclocOptionsFromFile.c_str());
                    return retVal;
                }
            }

            std::string optionsFileName = inputFile.substr(0, extStart);
            optionsFileName.append("_options.txt");

            bool optionsRead = readOptionsFromFile(options, optionsFileName, argHelper);
            if (optionsRead) {
                optionsReadFromFile = std::string(options);
            }
            if (optionsRead && !isQuiet()) {
                argHelper->printf("Building with options:\n%s\n", options.c_str());
            }

            std::string internalOptionsFileName = inputFile.substr(0, extStart);
            internalOptionsFileName.append("_internal_options.txt");

            std::string internalOptionsFromFile;
            bool internalOptionsRead = readOptionsFromFile(internalOptionsFromFile, internalOptionsFileName, argHelper);
            if (internalOptionsRead) {
                internalOptionsReadFromFile = std::string(internalOptionsFromFile);
            }
            if (internalOptionsRead && !isQuiet()) {
                argHelper->printf("Building with internal options:\n%s\n", internalOptionsFromFile.c_str());
            }
            CompilerOptions::concatenateAppend(internalOptions, internalOptionsFromFile);
        }
    }

    retVal = deviceName.empty() ? OCLOC_SUCCESS : initHardwareInfo(deviceName.c_str());
    if (retVal != OCLOC_SUCCESS) {
        argHelper->printf("Error: Cannot get HW Info for device %s.\n", deviceName.c_str());
        return retVal;
    }

    if (formatToEnforce.empty() &&
        compilerProductHelper &&
        compilerProductHelper->oclocEnforceZebinFormat()) {
        formatToEnforce = "zebin";
    }

    if (!formatToEnforce.empty()) {
        enforceFormat(formatToEnforce);
    }
    if (CompilerOptions::contains(options, CompilerOptions::generateDebugInfo.str())) {
        if (false == inputFileSpirV() && false == CompilerOptions::contains(options, CompilerOptions::generateSourcePath) && false == CompilerOptions::contains(options, CompilerOptions::useCMCompiler)) {
            auto sourcePathStringOption = CompilerOptions::generateSourcePath.str();
            sourcePathStringOption.append(" ");
            sourcePathStringOption.append(CompilerOptions::wrapInQuotes(inputFile));
            options = CompilerOptions::concatenate(options, sourcePathStringOption);
        }
    }
    if (deviceName.empty()) {
        std::string emptyDeviceOptions = "-ocl-version=300 -cl-ext=-all,+cl_khr_3d_image_writes,"
                                         "+__opencl_c_3d_image_writes,+__opencl_c_images";
        internalOptions = CompilerOptions::concatenate(emptyDeviceOptions, internalOptions);
        CompilerOptions::concatenateAppend(internalOptions, CompilerOptions::enableImageSupport);
    } else {
        appendExtensionsToInternalOptions(hwInfo, options, internalOptions);
        appendExtraInternalOptions(internalOptions);
    }
    parseDebugSettings();

    if (allowCaching) {
        auto cacheConfig = NEO::getDefaultCompilerCacheConfig();
        if (cacheConfig.cacheDir.empty() && !cacheDir.empty()) {
            cacheConfig.cacheDir = cacheDir;
        }
        cache = std::make_unique<CompilerCache>(cacheConfig);
        retVal = createDir(cacheConfig.cacheDir);
        if (retVal != OCLOC_SUCCESS) {
            argHelper->printf("Error: Failed to create directory '%s'.\n", cacheConfig.cacheDir.c_str());
            return retVal;
        }
    }

    return retVal;
}

int OfflineCompiler::parseCommandLine(size_t numArgs, const std::vector<std::string> &argv) {
    int retVal = OCLOC_SUCCESS;
    bool compile32 = false;
    bool compile64 = false;
    std::set<std::string> deviceAcronymsFromDeviceOptions;

    for (uint32_t argIndex = 1; argIndex < argv.size(); argIndex++) {
        const auto &currArg = argv[argIndex];
        const bool hasMoreArgs = (argIndex + 1 < numArgs);
        const bool hasAtLeast2MoreArgs = (argIndex + 2 < numArgs);
        if ("compile" == currArg) {
            // skip it
        } else if (("-file" == currArg) && hasMoreArgs) {
            inputFile = argv[argIndex + 1];
            argIndex++;
        } else if (("-output" == currArg) && hasMoreArgs) {
            outputFile = argv[argIndex + 1];
            argIndex++;
        } else if (("-o" == currArg) && hasMoreArgs) {
            binaryOutputFile = argv[argIndex + 1];
            argIndex++;
        } else if ((CompilerOptions::arch32bit == currArg) || ("-32" == currArg)) {
            compile32 = true;
            CompilerOptions::concatenateAppend(internalOptions, CompilerOptions::arch32bit);
        } else if ((CompilerOptions::arch64bit == currArg) || ("-64" == currArg)) {
            compile64 = true;
            CompilerOptions::concatenateAppend(internalOptions, CompilerOptions::arch64bit);
        } else if (CompilerOptions::greaterThan4gbBuffersRequired == currArg) {
            CompilerOptions::concatenateAppend(internalOptions, CompilerOptions::greaterThan4gbBuffersRequired);
        } else if (("-device" == currArg) && hasMoreArgs) {
            deviceName = argv[argIndex + 1];
            argIndex++;
        } else if ("-llvm_text" == currArg) {
            if (this->inputCodeType == IGC::CodeType::llvmBc) {
                this->inputCodeType = IGC::CodeType::llvmLl;
            }
            this->intermediateRepresentation = IGC::CodeType::llvmLl;
        } else if ("-llvm_bc" == currArg) {
            if (this->inputCodeType == IGC::CodeType::llvmLl) {
                this->inputCodeType = IGC::CodeType::llvmBc;
            }
            this->intermediateRepresentation = IGC::CodeType::llvmBc;
        } else if ("-llvm_input" == currArg) {
            this->inputCodeType = (this->intermediateRepresentation == IGC::CodeType::llvmLl) ? IGC::CodeType::llvmLl : IGC::CodeType::llvmBc;
            this->intermediateRepresentation = this->inputCodeType;
        } else if ("-spirv_input" == currArg) {
            this->inputCodeType = IGC::CodeType::spirV;
            this->intermediateRepresentation = IGC::CodeType::spirV;
        } else if ("-cpp_file" == currArg) {
            useCppFile = true;
        } else if ("-gen_file" == currArg) {
            useGenFile = true;
        } else if (("-options" == currArg) && hasMoreArgs) {
            CompilerOptions::concatenateAppend(options, argv[argIndex + 1]);
            argIndex++;
        } else if (("-device_options" == currArg) && hasAtLeast2MoreArgs) {
            const auto deviceAcronyms = CompilerOptions::tokenize(argv[argIndex + 1], ',');
            const auto &options = argv[argIndex + 2];
            for (const auto &deviceAcronym : deviceAcronyms) {
                deviceAcronymsFromDeviceOptions.insert(deviceAcronym.str());
                CompilerOptions::concatenateAppend(perDeviceOptions[deviceAcronym.str()], options);
            }
            argIndex += 2;
        } else if (("-internal_options" == currArg) && hasMoreArgs) {
            CompilerOptions::concatenateAppend(internalOptions, argv[argIndex + 1]);
            argIndex++;
        } else if ("-options_name" == currArg) {
            useOptionsSuffix = true;
        } else if ("-force_stos_opt" == currArg) {
            forceStatelessToStatefulOptimization = true;
        } else if (("-out_dir" == currArg) && hasMoreArgs) {
            outputDirectory = argv[argIndex + 1];
            argIndex++;
        } else if (("-cache_dir" == currArg) && hasMoreArgs) {
            cacheDir = argv[argIndex + 1];
            argIndex++;
        } else if ("-q" == currArg) {
            quiet = true;
        } else if ("-qq" == currArg) {
            argHelper->getPrinterRef().setSuppressMessages(true);
            quiet = true;
        } else if ("-v" == currArg) {
            argHelper->setVerbose(true);
        } else if ("-spv_only" == currArg) {
            onlySpirV = true;
        } else if ("-output_no_suffix" == currArg) {
            outputNoSuffix = true;
        } else if ("--help" == currArg) {
            showHelp = true;
            return OCLOC_SUCCESS;
        } else if (("-revision_id" == currArg) && hasMoreArgs) {
            revisionId = std::stoi(argv[argIndex + 1], nullptr, 0);
            argIndex++;
        } else if ("-exclude_ir" == currArg) {
            excludeIr = true;
        } else if ("--format" == currArg) {
            formatToEnforce = argv[argIndex + 1];
            argIndex++;
        } else if ("-stateful_address_mode" == currArg) {
            addressingMode = argv[argIndex + 1];
            argIndex++;
        } else if (("-heapless_mode" == currArg) && hasMoreArgs) {
            if (argv[argIndex + 1] == "enable") {
                heaplessMode = CompilerOptions::HeaplessMode::enabled;
            } else if (argv[argIndex + 1] == "disable") {
                heaplessMode = CompilerOptions::HeaplessMode::disabled;
            } else if (argv[argIndex + 1] == "default") {
                heaplessMode = CompilerOptions::HeaplessMode::defaultMode;
            } else {
                argHelper->printf("Error: Invalid heapless mode.\n");
                retVal = OCLOC_INVALID_COMMAND_LINE;
                break;
            }
            argIndex++;
        } else if (("-config" == currArg) && hasMoreArgs) {
            parseHwInfoConfigString(argv[argIndex + 1], hwInfoConfig);
            if (!hwInfoConfig) {
                argHelper->printf("Error: Invalid config.\n");
                retVal = OCLOC_INVALID_COMMAND_LINE;
                break;
            }
            argIndex++;
        } else if ("-allow_caching" == currArg) {
            allowCaching = true;
        } else {
            retVal = parseCommandLineExt(numArgs, argv, argIndex);
            if (OCLOC_INVALID_COMMAND_LINE == retVal) {
                argHelper->printf("Invalid option (arg %u): %s\n", argIndex, argv[argIndex].c_str());
                break;
            }
        }
    }

    if (!deviceName.empty()) {
        auto deviceNameCopy = deviceName;
        ProductConfigHelper::adjustDeviceName(deviceNameCopy);
        if (isArgumentDeviceId(deviceNameCopy)) {
            auto deviceID = static_cast<unsigned short>(std::stoi(deviceNameCopy.c_str(), 0, 16));
            uint32_t productConfig = argHelper->getProductConfigAndSetHwInfoBasedOnDeviceAndRevId(hwInfo, deviceID, revisionId, compilerProductHelper, releaseHelper);
            if (productConfig != AOT::UNKNOWN_ISA) {
                auto productAcronym = argHelper->productConfigHelper->getAcronymForProductConfig(productConfig);
                if (perDeviceOptions.find(productAcronym.c_str()) != perDeviceOptions.end())
                    CompilerOptions::concatenateAppend(options, perDeviceOptions.at(productAcronym.c_str()));
            }
        }
    }

    if (perDeviceOptions.find(deviceName) != perDeviceOptions.end()) {
        CompilerOptions::concatenateAppend(options, perDeviceOptions.at(deviceName));
    }

    if (!binaryOutputFile.empty() && (useGenFile || useCppFile || outputNoSuffix || !outputFile.empty())) {
        argHelper->printf("Error: options: -gen_file/-cpp_file/-output_no_suffix/-output cannot be used with -o\n");
        retVal = OCLOC_INVALID_COMMAND_LINE;
        return retVal;
    }
    if (!addressingMode.empty()) {

        if (addressingMode != "bindless" && addressingMode != "bindful" && addressingMode != "default") {
            argHelper->printf("Error: -stateful_address_mode value: %s is invalid\n", addressingMode.c_str());
            retVal = OCLOC_INVALID_COMMAND_LINE;
        }
        if (nullptr != strstr(internalOptions.c_str(), "-cl-intel-use-bindless") && addressingMode == "bindful") {
            argHelper->printf("Error: option -stateful_address_mode cannot be used with internal_options containing \"-cl-intel-use-bindless\"\n");
            retVal = OCLOC_INVALID_COMMAND_LINE;
        }
    }
    unifyExcludeIrFlags();

    if (debugManager.flags.OverrideRevision.get() != -1) {
        revisionId = static_cast<unsigned short>(debugManager.flags.OverrideRevision.get());
    }

    if (retVal == OCLOC_SUCCESS) {
        if (compile32 && compile64) {
            argHelper->printf("Error: Cannot compile for 32-bit and 64-bit, please choose one.\n");
            retVal |= OCLOC_INVALID_COMMAND_LINE;
        }

        if (deviceName.empty() && (false == onlySpirV)) {
            argHelper->printf("Error: Device name missing.\n");
            retVal = OCLOC_INVALID_COMMAND_LINE;
        }

        for (const auto &device : deviceAcronymsFromDeviceOptions) {
            auto productConfig = argHelper->productConfigHelper->getProductConfigFromDeviceName(device);
            if (productConfig == AOT::UNKNOWN_ISA) {
                auto productName = device;
                std::transform(productName.begin(), productName.end(), productName.begin(), ::tolower);
                std::vector<PRODUCT_FAMILY> allSupportedProduct{ALL_SUPPORTED_PRODUCT_FAMILIES};
                auto isDeprecatedName = false;
                for (const auto &product : allSupportedProduct) {
                    if (0 == strcmp(productName.c_str(), hardwarePrefix[product])) {
                        isDeprecatedName = true;
                        break;
                    }
                }
                if (!isDeprecatedName) {
                    argHelper->printf("Error: Invalid device acronym passed to -device_options: %s\n", device.c_str());
                    retVal = OCLOC_INVALID_COMMAND_LINE;
                }
            }
        }
    }

    return retVal;
}

void OfflineCompiler::unifyExcludeIrFlags() {
    const auto excludeIrFromZebin{internalOptions.find(CompilerOptions::excludeIrFromZebin.data()) != std::string::npos};
    if (!excludeIr && excludeIrFromZebin) {
        excludeIr = true;
    } else if (excludeIr && !excludeIrFromZebin) {
        const std::string prefix{"-ze"};
        CompilerOptions::concatenateAppend(internalOptions, prefix + CompilerOptions::excludeIrFromZebin.data());
    }
}

void OfflineCompiler::setStatelessToStatefulBufferOffsetFlag() {
    bool isStatelessToStatefulBufferOffsetSupported = true;
    if (!deviceName.empty()) {
        isStatelessToStatefulBufferOffsetSupported = compilerProductHelper->isStatelessToStatefulBufferOffsetSupported();
    }
    if (debugManager.flags.EnableStatelessToStatefulBufferOffsetOpt.get() != -1) {
        isStatelessToStatefulBufferOffsetSupported = debugManager.flags.EnableStatelessToStatefulBufferOffsetOpt.get() != 0;
    }
    if (isStatelessToStatefulBufferOffsetSupported) {
        CompilerOptions::concatenateAppend(internalOptions, CompilerOptions::hasBufferOffsetArg);
    }
}

void OfflineCompiler::appendExtraInternalOptions(std::string &internalOptions) {
    if (compilerProductHelper->isForceToStatelessRequired() && !forceStatelessToStatefulOptimization) {
        CompilerOptions::concatenateAppend(internalOptions, CompilerOptions::greaterThan4gbBuffersRequired);
    }
    if (compilerProductHelper->isForceEmuInt32DivRemSPRequired()) {
        CompilerOptions::concatenateAppend(internalOptions, CompilerOptions::forceEmuInt32DivRemSP);
    }
    if ((!compilerProductHelper->isBindlessAddressingDisabled(releaseHelper.get()) && addressingMode != "bindful") ||
        addressingMode == "bindless") {
        CompilerOptions::concatenateAppend(internalOptions, CompilerOptions::bindlessMode);
    }

    CompilerOptions::concatenateAppend(internalOptions, compilerProductHelper->getCachingPolicyOptions(false));
    CompilerOptions::applyExtraInternalOptions(internalOptions, hwInfo, *compilerProductHelper, this->heaplessMode);
}

void OfflineCompiler::parseDebugSettings() {
    setStatelessToStatefulBufferOffsetFlag();
}

std::string OfflineCompiler::parseBinAsCharArray(uint8_t *binary, size_t size, std::string &fileName) {
    std::string builtinName = convertToPascalCase(fileName);
    std::ostringstream out;

    // Convert binary to cpp
    out << "#include <cstddef>\n";
    out << "#include <cstdint>\n\n";
    out << "size_t " << builtinName << "BinarySize_" << productFamilyName << " = " << size << ";\n";
    out << "uint32_t " << builtinName << "Binary_" << productFamilyName << "[" << (size + 3) / 4 << "] = {"
        << std::endl
        << "    ";

    uint32_t *binaryUint = (uint32_t *)binary;
    for (size_t i = 0; i < (size + 3) / 4; i++) {
        if (i != 0) {
            out << ", ";
            if (i % 8 == 0) {
                out << std::endl
                    << "    ";
            }
        }
        if (i < size / 4) {
            out << "0x" << std::hex << std::setw(8) << std::setfill('0') << binaryUint[i];
        } else {
            uint32_t lastBytes = size & 0x3;
            uint32_t lastUint = 0;
            uint8_t *pLastUint = (uint8_t *)&lastUint;
            for (uint32_t j = 0; j < lastBytes; j++) {
                pLastUint[sizeof(uint32_t) - 1 - j] = binary[i * 4 + j];
            }
            out << "0x" << std::hex << std::setw(8) << std::setfill('0') << lastUint;
        }
    }
    out << "};" << std::endl;

    return out.str();
}

std::string OfflineCompiler::getFileNameTrunk(std::string &filePath) {
    size_t slashPos = filePath.find_last_of("\\/", filePath.size()) + 1;
    size_t extPos = filePath.find_last_of(".", filePath.size());
    if (extPos == std::string::npos) {
        extPos = filePath.size();
    }

    std::string fileTrunk = filePath.substr(slashPos, (extPos - slashPos));

    return fileTrunk;
}

void OfflineCompiler::printUsage() {
    argHelper->printf(R"OCLOC_HELP(Compiles input file to Intel Compute GPU device binary (*.bin).
Additionally, outputs intermediate representation (e.g. spirV).
Different input and intermediate file formats are available.

Usage: ocloc [compile] -file <filename> -device <device_type> [-output <filename>] [-out_dir <output_dir>] [-options <options>] [-device_options <device_type> <options>] [-32|-64] [-internal_options <options>] [-llvm_text|-llvm_input|-spirv_input] [-options_name] [-q] [-cpp_file] [-output_no_suffix] [--help]

  -file <filename>                          The input file to be compiled
                                            (by default input source format is
                                            OpenCL C kernel language).

  -device <device_type>                     Target device.
                                            <device_type> can be: %s, ip version  or hexadecimal value with 0x prefix
                                            - can be single or multiple target devices.
                                            The ip version can be a representation of the
                                            <major>.<minor>.<revision> or a decimal value that
                                            can be queried using the L0 ZE_extension_device_ip_version.
                                            The hexadecimal value represents device ID.
                                            If such value is provided, ocloc will try to
                                            match it with corresponding device type.
                                            For example, 0x9A49 device ID will be translated
                                            to tgllp.
                                            If multiple target devices are provided, ocloc
                                            will compile for each of these targets and will
                                            create a fatbinary archive that contains all of
                                            device binaries produced this way.
                                            Supported -device patterns examples:
                                            -device 0x4905        ; will compile 1 target (dg1)
                                            -device 12.10.0       ; will compile 1 target (dg1)
                                            -device 50495488      ; will compile 1 target (dg1)
                                            -device dg1           ; will compile 1 target
                                            -device dg1,acm-g10   ; will compile 2 targets
                                            -device dg1:acm-g10   ; will compile all targets
                                                                    in range (inclusive)
                                            -device dg1:          ; will compile all targets
                                                                    newer/same as provided
                                            -device :dg1          ; will compile all targets
                                                                    older/same as provided
                                            -device xe-hpg        ; will compile all targets
                                                                    matching the same release
                                            -device xe            ; will compile all targets
                                                                    matching the same family
                                            -device xe-hpg:xe-hpc ; will compile all targets
                                                                    in range (inclusive)
                                            -device xe-hpg:       ; will compile all targets
                                                                    newer/same as provided
                                            -device :xe-hpg       ; will compile all targets
                                                                    older/same as provided
                                                                    known to ocloc

                                            Deprecated notation that is still supported:
                                            <device_type> can be: %s
                                            - can be single target device.

  -o <filename>                             Optional output file name.
                                            Must not be used with:
                                            -gen_file | -cpp_file | -output_no_suffix | -output

  -output <filename>                        Optional output file base name.
                                            Default is input file's base name.
                                            This base name will be used for all output files.
                                            For single target device proper suffixes (describing file formats)
                                            will be added automatically.

  -out_dir <output_dir>                     Optional output directory.
                                            Default is current working directory.

  -allow_caching                            Allows caching binaries from compilation (like spirv,
                                            gen or debug data) and loading them by ocloc
                                            when the same program is compiled again.

  -cache_dir <output_dir>                   Optional caching directory.
                                            Default directory is "ocloc_cache".

  -options <options>                        Optional OpenCL C compilation options
                                            as defined by OpenCL specification.
                                            Special options for Vector Compute:
                                            -vc-codegen <vc options> compile from SPIRV
                                            -cmc <cm-options> compile from CM sources

  -device_options <device_type> <options>   Optional OpenCL C compilation options
                                            as defined by OpenCL specification - specific to a single target device.
                                            Multiple product acronyms may be provided - separated by commas.
                                            <device_type> can be product acronym or version passed in -device i.e. dg1 or 12.10.0

  -32                                       Forces target architecture to 32-bit pointers.
                                            Default pointer size is inherited from
                                            ocloc's pointer size.
                                            This option is exclusive with -64.

  -64                                       Forces target architecture to 64-bit pointers.
                                            Default pointer size is inherited from
                                            ocloc's pointer size.
                                            This option is exclusive with -32.

  -internal_options <options>               Optional compiler internal options
                                            as defined by compilers used underneath.
                                            Check intel-graphics-compiler (IGC) project
                                            for details on available internal options.
                                            You also may provide explicit --help to inquire
                                            information about option, mentioned in -options

  -llvm_text                                Forces intermediate representation (IR) format
                                            to human-readable LLVM IR (.ll).
                                            This option affects only output files
                                            and should not be used in combination with
                                            '-llvm_input' option.
                                            Default IR is spirV.
                                            This option is exclusive with -spirv_input.
                                            This option is exclusive with -llvm_input.

  -llvm_input                               Indicates that input file is an llvm binary.
                                            Default is OpenCL C kernel language.
                                            This option is exclusive with -spirv_input.
                                            This option is exclusive with -llvm_text.

  -spirv_input                              Indicates that input file is a spirV binary.
                                            Default is OpenCL C kernel language format.
                                            This option is exclusive with -llvm_input.
                                            This option is exclusive with -llvm_text.

  -options_name                             Will add suffix to output files.
                                            This suffix will be generated based on input
                                            options (useful when rebuilding with different
                                            set of options so that results won't get
                                            overwritten).
                                            This suffix is added always as the last part
                                            of the filename (even after file's extension).
                                            It does not affect '--output' parameter and can
                                            be used along with it ('--output' parameter
                                            defines the base name - i.e. prefix).

  -force_stos_opt                           Will forcibly enable stateless to stateful optimization,
                                            i.e. skip "-cl-intel-greater-than-4GB-buffer-required".

  -q                                        Will silence output messages (except errors).

  -qq                                       Will silence most of output messages.

  -v                                        Verbose mode.

  -spv_only                                 Will generate only spirV file.

  -cpp_file                                 Will generate c++ file with C-array
                                            containing Intel Compute device binary.

  -gen_file                                 Will generate gen file.

  -output_no_suffix                         Prevents ocloc from adding family name suffix.

  --help                                    Print this usage message.

  -revision_id <revision_id>                Target stepping. Can be decimal or hexadecimal value.

  -exclude_ir                               Excludes IR from the output binary file.

  --format                                  Enforce given binary format. The possible values are:
                                            --format zebin - Enforce generating zebin binary
                                            --format patchtokens - Enforce generating patchtokens (legacy) binary.

  -stateful_address_mode                    Enforce given addressing mode. The possible values are:
                                            -stateful_address_mode bindful - enforce generating in bindful mode
                                            -stateful_address_mode bindless - enforce generating in bindless mode
                                            -stateful_address_mode default - default mode
                                            not defined: default mode.

  -heapless_mode                            Enforce heapless mode for the binary. The possible values are:
                                            -heapless_mode enable  - enforce generating in heapless mode
                                            -heapless_mode disable - enforce generating in heapful mode
                                            -heapless_mode default - default mode for current platform

  -config                                   Target hardware info config for a single device,
                                            e.g 1x4x8.
%s
Examples :
  Compile file to Intel Compute GPU device binary (out = source_file_Gen9core.bin)
    ocloc -file source_file.cl -device skl
)OCLOC_HELP",
                      getSupportedDevices(argHelper).c_str(),
                      getDeprecatedDevices(argHelper).c_str(),
                      getOfflineCompilerOptionsExt().c_str());
}

void OfflineCompiler::storeBinary(
    char *&pDst,
    size_t &dstSize,
    const void *pSrc,
    const size_t srcSize) {
    dstSize = 0;

    DEBUG_BREAK_IF(!(pSrc && srcSize > 0));

    delete[] pDst;
    pDst = new char[srcSize];

    dstSize = static_cast<uint32_t>(srcSize);
    memcpy_s(pDst, dstSize, pSrc, srcSize);
}

bool OfflineCompiler::generateElfBinary() {
    if (!genBinary || !genBinarySize) {
        return false;
    }

    // return "as is" if zebin format
    if (isDeviceBinaryFormat<DeviceBinaryFormat::zebin>(ArrayRef<uint8_t>(reinterpret_cast<uint8_t *>(genBinary), genBinarySize))) {
        this->elfBinary = std::vector<uint8_t>(genBinary, genBinary + genBinarySize);
        return true;
    }

    if (allowCaching) {
        const std::string igcRevision = igcFacade->getIgcRevision();
        const auto igcLibSize = igcFacade->getIgcLibSize();
        const auto igcLibMTime = igcFacade->getIgcLibMTime();
        elfHash = cache->getCachedFileName(getHardwareInfo(),
                                           genHash,
                                           options,
                                           internalOptions, ArrayRef<const char>(), ArrayRef<const char>(), igcRevision, igcLibSize, igcLibMTime);
        auto loadedData = cache->loadCachedBinary(elfHash, elfBinarySize);
        elfBinary.assign(loadedData.get(), loadedData.get() + elfBinarySize);
        if (!elfBinary.empty()) {
            return true;
        }
    }

    SingleDeviceBinary binary = {};
    binary.buildOptions = this->options;
    binary.intermediateRepresentation = ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(this->irBinary), this->irBinarySize);
    binary.deviceBinary = ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(this->genBinary), this->genBinarySize);
    binary.debugData = ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(this->debugDataBinary), this->debugDataBinarySize);
    std::string packErrors;
    std::string packWarnings;

    using namespace NEO::Elf;
    ElfEncoder<EI_CLASS_64> elfEncoder;
    elfEncoder.getElfFileHeader().type = ET_OPENCL_EXECUTABLE;
    if (binary.buildOptions.empty() == false) {
        elfEncoder.appendSection(SHT_OPENCL_OPTIONS, SectionNamesOpenCl::buildOptions,
                                 ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(binary.buildOptions.data()), binary.buildOptions.size()));
    }

    if (!binary.intermediateRepresentation.empty() && !excludeIr) {
        if (this->intermediateRepresentation == IGC::CodeType::spirV) {
            elfEncoder.appendSection(SHT_OPENCL_SPIRV, SectionNamesOpenCl::spirvObject, binary.intermediateRepresentation);
        } else {
            elfEncoder.appendSection(SHT_OPENCL_LLVM_BINARY, SectionNamesOpenCl::llvmObject, binary.intermediateRepresentation);
        }
    }

    if (binary.debugData.empty() == false) {
        elfEncoder.appendSection(SHT_OPENCL_DEV_DEBUG, SectionNamesOpenCl::deviceDebug, binary.debugData);
    }

    if (binary.deviceBinary.empty() == false) {
        elfEncoder.appendSection(SHT_OPENCL_DEV_BINARY, SectionNamesOpenCl::deviceBinary, binary.deviceBinary);
    }

    this->elfBinary = elfEncoder.encode();
    if (allowCaching) {
        cache->cacheBinary(elfHash, reinterpret_cast<char *>(elfBinary.data()), static_cast<uint32_t>(this->elfBinary.size()));
    }
    return true;
}

void OfflineCompiler::writeOutAllFiles() {
    std::string fileBase;
    std::string fileTrunk = getFileNameTrunk(inputFile);
    if (outputNoSuffix) {
        if (outputFile.empty()) {
            fileBase = fileTrunk;
        } else {
            fileBase = outputFile;
        }
    } else {
        if (outputFile.empty()) {
            fileBase = fileTrunk + "_" + productFamilyName;
        } else {
            fileBase = outputFile + "_" + productFamilyName;
        }
    }

    if (outputDirectory != "") {
        std::list<std::string> dirList;
        std::string tmp = outputDirectory;
        size_t pos = outputDirectory.size() + 1;

        do {
            dirList.push_back(tmp);
            pos = tmp.find_last_of("/\\", pos);
            tmp = tmp.substr(0, pos);
        } while (pos != std::string::npos && !tmp.empty());

        while (!dirList.empty()) {
            int retVal = createDir(dirList.back());
            if (retVal != OCLOC_SUCCESS) {
                argHelper->printf("Error: Failed to create directory '%s'.\n", dirList.back().c_str());
            }
            dirList.pop_back();
        }
    }

    if (!binaryOutputFile.empty()) {
        std::string outputFile = generateFilePath(outputDirectory, binaryOutputFile, "");
        if (isOnlySpirV()) {
            argHelper->saveOutput(outputFile, irBinary, irBinarySize);
        } else if (!elfBinary.empty()) {
            argHelper->saveOutput(outputFile, elfBinary.data(), elfBinary.size());
        }
        return;
    }

    if (irBinary && (this->inputCodeType != IGC::CodeType::spirV)) {
        std::string irOutputFileName = generateFilePathForIr(fileBase) + generateOptsSuffix();

        argHelper->saveOutput(irOutputFileName, irBinary, irBinarySize);
    }

    if (genBinary) {
        if (useGenFile) {
            std::string genOutputFile = generateFilePath(outputDirectory, fileBase, ".gen") + generateOptsSuffix();
            argHelper->saveOutput(genOutputFile, genBinary, genBinarySize);
        }

        if (useCppFile) {
            std::string cppOutputFile = generateFilePath(outputDirectory, fileBase, ".cpp");
            std::string cpp = parseBinAsCharArray((uint8_t *)genBinary, genBinarySize, fileTrunk);
            argHelper->saveOutput(cppOutputFile, cpp.c_str(), cpp.size());
        }
    }

    if (!elfBinary.empty()) {
        std::string elfOutputFile;
        if (outputNoSuffix) {
            // temporary fix for backwards compatibility with oneAPI 2023.1 and older
            // after adding ".bin" extension to name of binary output file
            // if "-output filename" is passed with ".out" or ".exe" extension - do not append ".bin"
            if (outputFile.empty()) {
                elfOutputFile = generateFilePath(outputDirectory, fileBase, ".bin");
            } else {
                size_t extPos = fileBase.find_last_of(".", fileBase.size());
                std::string fileExt = ".bin";
                if (extPos != std::string::npos) {
                    auto existingExt = fileBase.substr(extPos, fileBase.size());
                    if (existingExt == ".out" || existingExt == ".exe") {
                        fileExt = "";
                    }
                }
                elfOutputFile = generateFilePath(outputDirectory, fileBase, fileExt.c_str());
            }
        } else {
            elfOutputFile = generateFilePath(outputDirectory, fileBase, ".bin") + generateOptsSuffix();
        }
        argHelper->saveOutput(
            elfOutputFile,
            elfBinary.data(),
            elfBinary.size());
    }

    if (debugDataBinary) {
        std::string debugOutputFile = generateFilePath(outputDirectory, fileBase, ".dbg") + generateOptsSuffix();

        argHelper->saveOutput(
            debugOutputFile,
            debugDataBinary,
            debugDataBinarySize);
    }
}

int OfflineCompiler::createDir(const std::string &path) {
    auto result = IoFunctions::mkdirPtr(path.c_str());
    if (result != 0) {
        if (errno == EEXIST) {
            // Directory already exists, not an error
            return OCLOC_SUCCESS;
        } else {
            return OCLOC_INVALID_FILE;
        }
    }
    return OCLOC_SUCCESS;
}

bool OfflineCompiler::readOptionsFromFile(std::string &options, const std::string &file, OclocArgHelper *helper) {
    if (!helper->fileExists(file)) {
        return false;
    }
    size_t optionsSize = 0U;
    auto optionsFromFile = helper->loadDataFromFile(file, optionsSize);
    if (optionsSize > 0) {
        // Remove comment containing copyright header
        options = optionsFromFile.get();
        size_t commentBegin = options.find("/*");
        size_t commentEnd = options.rfind("*/");
        if (commentBegin != std::string::npos && commentEnd != std::string::npos) {
            auto sizeToReplace = commentEnd - commentBegin + 2;
            options = options.replace(commentBegin, sizeToReplace, "");
            size_t optionsBegin = options.find_first_not_of(" \t\n\r");
            if (optionsBegin != std::string::npos) {
                options = options.substr(optionsBegin, options.length());
            }
        }
        auto trimPos = options.find_last_not_of(" \n\r");
        options = options.substr(0, trimPos + 1);
    }
    return true;
}

void OfflineCompiler::enforceFormat(std::string &format) {
    std::transform(format.begin(), format.end(), format.begin(),
                   [](auto c) { return std::tolower(c); });
    if (format == "zebin") {
        // zebin is enabled by default
    } else if (format == "patchtokens") {
        CompilerOptions::concatenateAppend(internalOptions, CompilerOptions::disableZebin);
    } else {
        argHelper->printf("Invalid format passed: %s. Ignoring.\n", format.c_str());
    }
}

std::string generateFilePath(const std::string &directory, const std::string &fileNameBase, const char *extension) {
    UNRECOVERABLE_IF(extension == nullptr);

    if (directory.empty()) {
        return fileNameBase + extension;
    }

    bool hasTrailingSlash = (*directory.rbegin() == '/');
    std::string ret;
    ret.reserve(directory.size() + (hasTrailingSlash ? 0 : 1) + fileNameBase.size() + strlen(extension) + 1);
    ret.append(directory);
    if (false == hasTrailingSlash) {
        ret.append("/", 1);
    }
    ret.append(fileNameBase);
    ret.append(extension);

    return ret;
}

bool OfflineCompiler::useIgcAsFcl() {
    if (0 != debugManager.flags.UseIgcAsFcl.get()) {
        if (1 == debugManager.flags.UseIgcAsFcl.get()) {
            return true;
        } else if (2 == debugManager.flags.UseIgcAsFcl.get()) {
            return false;
        }
    }

    if (nullptr == compilerProductHelper) {
        return false;
    }
    return compilerProductHelper->useIgcAsFcl();
}

} // namespace NEO
