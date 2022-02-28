/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/compiler_interface/compiler_interface.h"
#include "shared/source/compiler_interface/linker.h"
#include "shared/source/device_binary_format/debug_zebin.h"
#include "shared/source/device_binary_format/elf/elf_encoder.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/program/program_info.h"
#include "shared/source/utilities/const_stringref.h"

#include "opencl/source/api/cl_types.h"
#include "opencl/source/cl_device/cl_device_vector.h"
#include "opencl/source/helpers/base_object.h"

#include "cif/builtins/memory/buffer/buffer.h"
#include "patch_list.h"

#include <list>
#include <string>
#include <vector>

namespace NEO {
namespace PatchTokenBinary {
struct ProgramFromPatchtokens;
}

class BuiltinDispatchInfoBuilder;
class ClDevice;
class Context;
class CompilerInterface;
class Device;
class ExecutionEnvironment;
class Program;
struct KernelInfo;
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
    case TranslationOutput::ErrorCode::Success:
        return CL_SUCCESS;
    case TranslationOutput::ErrorCode::CompilerNotAvailable:
        return CL_COMPILER_NOT_AVAILABLE;
    case TranslationOutput::ErrorCode::CompilationFailure:
        return CL_COMPILE_PROGRAM_FAILURE;
    case TranslationOutput::ErrorCode::BuildFailure:
        return CL_BUILD_PROGRAM_FAILURE;
    case TranslationOutput::ErrorCode::LinkFailure:
        return CL_LINK_PROGRAM_FAILURE;
    }
}

class Program : public BaseObject<_cl_program> {
  public:
    static const cl_ulong objectMagic = 0x5651C89100AAACFELL;

    enum class CreatedFrom {
        SOURCE,
        IL,
        BINARY,
        UNKNOWN
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

    Program(const Program &) = delete;
    Program &operator=(const Program &) = delete;

    cl_int build(const ClDeviceVector &deviceVector, const char *buildOptions,
                 bool enableCaching);

    cl_int build(const ClDeviceVector &deviceVector, const char *buildOptions, bool enableCaching,
                 std::unordered_map<std::string, BuiltinDispatchInfoBuilder *> &builtinsMap);

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

    Context &getContext() const {
        return *context;
    }

    Context *getContextPtr() const {
        return context;
    }

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

    GraphicsAllocation *getConstantSurface(uint32_t rootDeviceIndex) const {
        return buildInfos[rootDeviceIndex].constantSurface;
    }

    GraphicsAllocation *getGlobalSurface(uint32_t rootDeviceIndex) const {
        return buildInfos[rootDeviceIndex].globalSurface;
    }

    GraphicsAllocation *getExportedFunctionsSurface(uint32_t rootDeviceIndex) const {
        return buildInfos[rootDeviceIndex].exportedFunctionsSurface;
    }

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

    void enableKernelDebug() {
        kernelDebugEnabled = true;
    }

    bool isKernelDebugEnabled() {
        return kernelDebugEnabled;
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

    const ExecutionEnvironment &getExecutionEnvironment() const { return executionEnvironment; }

    void setContext(Context *pContext) {
        this->context = pContext;
    }

    void notifyDebuggerWithDebugData(ClDevice *clDevice);
    MOCKABLE_VIRTUAL void createDebugZebin(uint32_t rootDeviceIndex);
    Debug::Segments getZebinSegments(uint32_t rootDeviceIndex);

  protected:
    MOCKABLE_VIRTUAL cl_int createProgramFromBinary(const void *pBinary, size_t binarySize, ClDevice &clDevice);

    cl_int packDeviceBinary(ClDevice &clDevice);

    MOCKABLE_VIRTUAL cl_int linkBinary(Device *pDevice, const void *constantsInitData, const void *variablesInitData,
                                       const ProgramInfo::GlobalSurfaceInfo &stringInfo, std::vector<NEO::ExternalFunctionInfo> &extFuncInfos);

    void updateNonUniformFlag();
    void updateNonUniformFlag(const Program **inputProgram, size_t numInputPrograms);

    void extractInternalOptions(const std::string &options, std::string &internalOptions);
    MOCKABLE_VIRTUAL bool isFlagOption(ConstStringRef option);
    MOCKABLE_VIRTUAL bool isOptionValueValid(ConstStringRef option, ConstStringRef value);
    MOCKABLE_VIRTUAL void applyAdditionalOptions(std::string &internalOptions);

    MOCKABLE_VIRTUAL bool appendKernelDebugOptions(ClDevice &clDevice, std::string &internalOptions);
    void notifyDebuggerWithSourceCode(ClDevice &clDevice, std::string &filename);
    void prependFilePathToOptions(const std::string &filename);

    void setBuildStatus(cl_build_status status);
    void setBuildStatusSuccess(const ClDeviceVector &deviceVector, cl_program_binary_type binaryType);

    bool isSpirV = false;

    std::unique_ptr<char[]> irBinary;
    size_t irBinarySize = 0U;

    CreatedFrom createdFrom = CreatedFrom::UNKNOWN;

    struct DeviceBuildInfo {
        StackVec<ClDevice *, 2> associatedSubDevices;
        cl_build_status buildStatus = CL_BUILD_NONE;
        cl_program_binary_type programBinaryType = CL_PROGRAM_BINARY_TYPE_NONE;
    };

    std::unordered_map<ClDevice *, DeviceBuildInfo> deviceBuildInfos;
    bool isCreatedFromBinary = false;
    bool shouldWarnAboutRebuild = false;

    std::string sourceCode;
    std::string options;
    static const std::vector<ConstStringRef> internalOptionsToExtract;

    uint32_t programOptionVersion = 12U;
    bool allowNonUniform = false;

    struct BuildInfo : public NonCopyableClass {
        std::vector<KernelInfo *> kernelInfoArray;
        GraphicsAllocation *constantSurface = nullptr;
        GraphicsAllocation *globalSurface = nullptr;
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
    };

    std::vector<BuildInfo> buildInfos;

    bool areSpecializationConstantsInitialized = false;
    CIF::RAII::UPtr_t<CIF::Builtins::BufferSimple> specConstantsIds;
    CIF::RAII::UPtr_t<CIF::Builtins::BufferSimple> specConstantsSizes;
    specConstValuesMap specConstantsValues;

    ExecutionEnvironment &executionEnvironment;
    Context *context = nullptr;
    ClDeviceVector clDevices;
    ClDeviceVector clDevicesInProgram;

    bool isBuiltIn = false;
    bool kernelDebugEnabled = false;
    uint32_t maxRootDeviceIndex = std::numeric_limits<uint32_t>::max();
    std::mutex lockMutex;
    uint32_t exposedKernels = 0;
};

} // namespace NEO
