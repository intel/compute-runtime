/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/compiler_interface/compiler_interface.h"
#include "shared/source/compiler_interface/linker.h"
#include "shared/source/device_binary_format/device_binary_formats.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/program/program_info.h"

#include "opencl/source/cl_device/cl_device_vector.h"
#include "opencl/source/helpers/base_object.h"

#include <functional>

namespace NEO {
namespace Zebin::Debug {
struct Segments;
}
namespace PatchTokenBinary {
struct ProgramFromPatchtokens;
}

enum class BuildPhase;
class BuiltinDispatchInfoBuilder;
class ClDevice;
class Context;
class CompilerInterface;
class Device;
class ExecutionEnvironment;
class Program;
struct MetadataGeneration;
struct KernelInfo;
enum class DecodeError : uint8_t;
struct ExternalFunctionInfo;
class SharedPoolAlloction;

template <>
struct OpenCLObjectMapper<_cl_program> {
    typedef class Program DerivedType;
};

namespace ProgramFunctions {
using CreateFromILFunc = std::function<Program *(Context *ctx,
                                                 const void *il,
                                                 size_t length,
                                                 int32_t &errcodeRet)>;
extern CreateFromILFunc createFromIL;
} // namespace ProgramFunctions

constexpr cl_int asClError(TranslationOutput::ErrorCode err) {
    switch (err) {
    default:
        return CL_OUT_OF_HOST_MEMORY;
    case TranslationOutput::ErrorCode::success:
        return CL_SUCCESS;
    case TranslationOutput::ErrorCode::compilerNotAvailable:
        return CL_COMPILER_NOT_AVAILABLE;
    case TranslationOutput::ErrorCode::compilationFailure:
        return CL_COMPILE_PROGRAM_FAILURE;
    case TranslationOutput::ErrorCode::buildFailure:
        return CL_BUILD_PROGRAM_FAILURE;
    case TranslationOutput::ErrorCode::linkFailure:
        return CL_LINK_PROGRAM_FAILURE;
    }
}

class Program : public BaseObject<_cl_program> {
  public:
    static const cl_ulong objectMagic = 0x5651C89100AAACFELL;

    enum class BuildPhase {
        init,
        sourceCodeNotification,
        binaryCreation,
        binaryProcessing,
        debugDataNotification
    };

    enum class CreatedFrom {
        source,
        il,
        binary,
        unknown
    };

    // Create program from binary
    template <typename T = Program>
    static T *create(
        Context *pContext,
        const ClDeviceVector &deviceVector,
        const size_t *lengths,
        const unsigned char **binaries,
        cl_int *binaryStatus,
        cl_int &errcodeRet);

    // Create program from source
    template <typename T = Program>
    static T *create(
        Context *pContext,
        cl_uint count,
        const char **strings,
        const size_t *lengths,
        cl_int &errcodeRet);

    template <typename T = Program>
    static T *createBuiltInFromSource(
        const char *nullTerminatedString,
        Context *context,
        const ClDeviceVector &deviceVector,
        cl_int *errcodeRet);

    template <typename T = Program>
    static T *createBuiltInFromGenBinary(
        Context *context,
        const ClDeviceVector &deviceVector,
        const void *binary,
        size_t size,
        cl_int *errcodeRet);

    template <typename T = Program>
    static T *createFromIL(Context *context,
                           const void *il,
                           size_t length,
                           cl_int &errcodeRet);

    Program(Context *context, bool isBuiltIn, const ClDeviceVector &clDevicesIn);
    ~Program() override;

    cl_int build(const ClDeviceVector &deviceVector, const char *buildOptions);

    cl_int build(const ClDeviceVector &deviceVector, const char *buildOptions,
                 std::unordered_map<std::string, BuiltinDispatchInfoBuilder *> &builtinsMap);

    cl_int processGenBinaries(const ClDeviceVector &clDevices, std::unordered_map<uint32_t, BuildPhase> &phaseReached);
    MOCKABLE_VIRTUAL cl_int processGenBinary(const ClDevice &clDevice);
    MOCKABLE_VIRTUAL cl_int processProgramInfo(ProgramInfo &dst, const ClDevice &clDevice);

    cl_int compile(const ClDeviceVector &deviceVector, const char *buildOptions,
                   cl_uint numInputHeaders, const cl_program *inputHeaders, const char **headerIncludeNames);

    cl_int link(const ClDeviceVector &deviceVector, const char *buildOptions,
                cl_uint numInputPrograms, const cl_program *inputPrograms);

    cl_int setProgramSpecializationConstant(cl_uint specId, size_t specSize, const void *specValue);
    MOCKABLE_VIRTUAL cl_int updateSpecializationConstant(cl_uint specId, size_t specSize, const void *specValue);

    size_t getNumKernels() const;
    const KernelInfo *getKernelInfo(const char *kernelName, uint32_t rootDeviceIndex) const;
    const KernelInfo *getKernelInfo(size_t ordinal, uint32_t rootDeviceIndex) const;

    cl_int getInfo(cl_program_info paramName, size_t paramValueSize,
                   void *paramValue, size_t *paramValueSizeRet);

    cl_int getBuildInfo(cl_device_id device, cl_program_build_info paramName,
                        size_t paramValueSize, void *paramValue, size_t *paramValueSizeRet) const;

    bool isBuilt() const {
        return std::any_of(this->deviceBuildInfos.begin(), this->deviceBuildInfos.end(), [](auto deviceBuildInfo) { return deviceBuildInfo.second.buildStatus == CL_SUCCESS && deviceBuildInfo.second.programBinaryType == CL_PROGRAM_BINARY_TYPE_EXECUTABLE; });
    }

    Context &getContext() const;
    Context *getContextPtr() const;

    ExecutionEnvironment &peekExecutionEnvironment() const {
        return executionEnvironment;
    }

    cl_int processSpirBinary(const void *pBinary, size_t binarySize, bool isSpirV);

    cl_int getSource(std::string &binary) const;

    MOCKABLE_VIRTUAL void processDebugData(uint32_t rootDeviceIndex);

    void updateBuildLog(uint32_t rootDeviceIndex, const char *pErrorString, const size_t errorStringSize);

    const char *getBuildLog(uint32_t rootDeviceIndex) const;

    cl_uint getProgramBinaryType(ClDevice *clDevice) const {
        return deviceBuildInfos.at(clDevice).programBinaryType;
    }

    bool getIsSpirV() const {
        return isSpirV;
    }

    void freeGlobalBufferAllocation(const std::unique_ptr<NEO::SharedPoolAllocation> &buffer);

    NEO::SharedPoolAllocation *getConstantSurface(uint32_t rootDeviceIndex) const;
    NEO::GraphicsAllocation *getConstantSurfaceGA(uint32_t rootDeviceIndex) const;
    NEO::SharedPoolAllocation *getGlobalSurface(uint32_t rootDeviceIndex) const;
    NEO::GraphicsAllocation *getGlobalSurfaceGA(uint32_t rootDeviceIndex) const;
    NEO::GraphicsAllocation *getExportedFunctionsSurface(uint32_t rootDeviceIndex) const;

    void cleanCurrentKernelInfo(uint32_t rootDeviceIndex);

    const std::string &getOptions() const { return options; }

    bool getAllowNonUniform() const {
        return allowNonUniform;
    }
    bool getIsBuiltIn() const {
        return isBuiltIn;
    }
    uint32_t getProgramOptionVersion() const {
        return programOptionVersion;
    }

    char *getDebugData(uint32_t rootDeviceIndex) {
        return buildInfos[rootDeviceIndex].debugData.get();
    }

    size_t getDebugDataSize(uint32_t rootDeviceIndex) {
        return buildInfos[rootDeviceIndex].debugDataSize;
    }

    const Linker::RelocatedSymbolsMap &getSymbols(uint32_t rootDeviceIndex) const {
        return buildInfos[rootDeviceIndex].symbols;
    }

    void setSymbols(uint32_t rootDeviceIndex, Linker::RelocatedSymbolsMap &&symbols) {
        buildInfos[rootDeviceIndex].symbols = std::move(symbols);
    }

    LinkerInput *getLinkerInput(uint32_t rootDeviceIndex) const {
        return buildInfos[rootDeviceIndex].linkerInput.get();
    }
    void setLinkerInput(uint32_t rootDeviceIndex, std::unique_ptr<LinkerInput> &&linkerInput) {
        buildInfos[rootDeviceIndex].linkerInput = std::move(linkerInput);
    }

    MOCKABLE_VIRTUAL void replaceDeviceBinary(std::unique_ptr<char[]> &&newBinary, size_t newBinarySize, uint32_t rootDeviceIndex);

    static bool isValidCallback(void(CL_CALLBACK *funcNotify)(cl_program program, void *userData), void *userData);
    void invokeCallback(void(CL_CALLBACK *funcNotify)(cl_program program, void *userData), void *userData);

    const ClDeviceVector &getDevices() const { return clDevices; }
    const ClDeviceVector &getDevicesInProgram() const;
    bool isDeviceAssociated(const ClDevice &clDevice) const;

    static cl_int processInputDevices(ClDeviceVector *&deviceVectorPtr, cl_uint numDevices, const cl_device_id *deviceList, const ClDeviceVector &allAvailableDevices);
    MOCKABLE_VIRTUAL std::string getInternalOptions() const;
    uint32_t getMaxRootDeviceIndex() const { return maxRootDeviceIndex; }
    uint32_t getIndirectDetectionVersion() const { return indirectDetectionVersion; }
    uint32_t getIndirectAccessBufferVersion() const { return indirectAccessBufferMajorVersion; }
    void retainForKernel() {
        std::unique_lock<std::mutex> lock{lockMutex};
        exposedKernels++;
    }
    void releaseForKernel() {
        std::unique_lock<std::mutex> lock{lockMutex};
        UNRECOVERABLE_IF(exposedKernels == 0);
        exposedKernels--;
    }
    bool isLocked() {
        std::unique_lock<std::mutex> lock{lockMutex};
        return 0 != exposedKernels;
    }
    bool getCreatedFromBinary() const {
        return isCreatedFromBinary;
    }

    const ExecutionEnvironment &getExecutionEnvironment() const { return executionEnvironment; }

    void setContext(Context *pContext) {
        this->context = pContext;
    }

    MOCKABLE_VIRTUAL void debugNotify(const ClDeviceVector &deviceVector, std::unordered_map<uint32_t, BuildPhase> &phasesReached);
    void createDebugData(ClDevice *clDevice);
    MOCKABLE_VIRTUAL void createDebugZebin(uint32_t rootDeviceIndex);
    Zebin::Debug::Segments getZebinSegments(uint32_t rootDeviceIndex);
    MOCKABLE_VIRTUAL void callPopulateZebinExtendedArgsMetadataOnce(uint32_t rootDeviceIndex);
    MOCKABLE_VIRTUAL void callGenerateDefaultExtendedArgsMetadataOnce(uint32_t rootDeviceIndex);
    MOCKABLE_VIRTUAL cl_int createFromILExt(Context *context, const void *il, size_t length);

  protected:
    MOCKABLE_VIRTUAL cl_int createProgramFromBinary(const void *pBinary, size_t binarySize, ClDevice &clDevice);

    cl_int packDeviceBinary(ClDevice &clDevice);

    MOCKABLE_VIRTUAL cl_int linkBinary(Device *pDevice, const void *constantsInitData, size_t constantsInitDataSize, const void *variablesInitData,
                                       size_t variablesInitDataSize, const ProgramInfo::GlobalSurfaceInfo &stringInfo,
                                       std::vector<NEO::ExternalFunctionInfo> &extFuncInfos);

    void updateNonUniformFlag();
    void updateNonUniformFlag(const Program **inputProgram, size_t numInputPrograms);

    void extractInternalOptions(const std::string &options, std::string &internalOptions);
    MOCKABLE_VIRTUAL bool isFlagOption(ConstStringRef option);
    MOCKABLE_VIRTUAL bool isOptionValueValid(ConstStringRef option, ConstStringRef value);

    void prependFilePathToOptions(const std::string &filename);

    void setBuildStatus(cl_build_status status);
    void setBuildStatusSuccess(const ClDeviceVector &deviceVector, cl_program_binary_type binaryType);

    void notifyModuleCreate();
    void notifyModuleDestroy();

    StackVec<NEO::GraphicsAllocation *, 32> getModuleAllocations(uint32_t rootIndex);
    bool isSpirV = false;

    std::unique_ptr<char[]> irBinary;
    size_t irBinarySize = 0U;

    CreatedFrom createdFrom = CreatedFrom::unknown;

    struct DeviceBuildInfo {
        StackVec<ClDevice *, 2> associatedSubDevices;
        cl_build_status buildStatus = CL_BUILD_NONE;
        cl_program_binary_type programBinaryType = CL_PROGRAM_BINARY_TYPE_NONE;
    };

    std::unordered_map<ClDevice *, DeviceBuildInfo> deviceBuildInfos;
    bool isCreatedFromBinary = false;
    bool requiresRebuild = false;

    std::string sourceCode;
    std::string options;
    static const std::vector<ConstStringRef> internalOptionsToExtract;

    uint32_t programOptionVersion = 12U;
    bool allowNonUniform = false;

    struct BuildInfo : public NonCopyableClass {
        std::vector<KernelInfo *> kernelInfoArray;
        std::unique_ptr<NEO::SharedPoolAllocation> constantSurface;
        std::unique_ptr<NEO::SharedPoolAllocation> globalSurface;
        GraphicsAllocation *exportedFunctionsSurface = nullptr;
        size_t globalVarTotalSize = 0U;
        std::unique_ptr<LinkerInput> linkerInput;
        Linker::RelocatedSymbolsMap symbols{};
        std::string buildLog{};

        std::unique_ptr<char[]> unpackedDeviceBinary;
        size_t unpackedDeviceBinarySize = 0U;

        std::unique_ptr<char[]> packedDeviceBinary;
        size_t packedDeviceBinarySize = 0U;
        ProgramInfo::GlobalSurfaceInfo constStringSectionData;

        std::unique_ptr<char[]> debugData;
        size_t debugDataSize = 0U;
        size_t kernelMiscInfoPos = std::string::npos;
    };

    std::vector<BuildInfo> buildInfos;

    struct DebuggerInfo : public NonCopyableClass {
        uint32_t debugModuleHandle;
        uint32_t debugElfHandle;
        uint64_t moduleLoadAddress;
    };

    std::vector<DebuggerInfo> debuggerInfos;

    bool areSpecializationConstantsInitialized = false;
    CIF::RAII::UPtr_t<CIF::Builtins::BufferSimple> specConstantsIds;
    CIF::RAII::UPtr_t<CIF::Builtins::BufferSimple> specConstantsSizes;
    specConstValuesMap specConstantsValues;

    ExecutionEnvironment &executionEnvironment;
    Context *context = nullptr;
    ClDeviceVector clDevices;
    ClDeviceVector clDevicesInProgram;

    uint32_t indirectDetectionVersion = 0u;
    uint32_t indirectAccessBufferMajorVersion = 0u;
    bool isBuiltIn = false;
    bool isGeneratedByIgc = true;

    uint32_t maxRootDeviceIndex = std::numeric_limits<uint32_t>::max();
    std::mutex lockMutex;
    uint32_t exposedKernels = 0;

    size_t exportedFunctionsKernelId = std::numeric_limits<size_t>::max();

    std::unique_ptr<MetadataGeneration> metadataGeneration;

    struct DecodedSingleDeviceBinary {
        bool isSet = false;
        ProgramInfo programInfo;
        DecodeError decodeError;
        std::string decodeErrors;
        std::string decodeWarnings;
    } decodedSingleDeviceBinary;
    IGC::CodeType::CodeType_t intermediateRepresentation = IGC::CodeType::invalid;
};

static_assert(NEO::NonCopyableAndNonMovable<Program>);

} // namespace NEO
