/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/compiler_interface/compiler_interface.h"
#include "core/compiler_interface/linker.h"
#include "core/elf/writer.h"
#include "core/program/program_info.h"
#include "core/utilities/const_stringref.h"
#include "runtime/api/cl_types.h"
#include "runtime/helpers/base_object.h"

#include "cif/builtins/memory/buffer/buffer.h"
#include "patch_list.h"

#include <map>
#include <string>
#include <vector>

namespace NEO {
namespace PatchTokenBinary {
struct ProgramFromPatchtokens;
}

class BlockKernelManager;
class BuiltinDispatchInfoBuilder;
class ClDevice;
class Context;
class CompilerInterface;
class Device;
class ExecutionEnvironment;
struct KernelInfo;
template <>
struct OpenCLObjectMapper<_cl_program> {
    typedef class Program DerivedType;
};

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
        cl_context context,
        cl_uint numDevices,
        const cl_device_id *deviceList,
        const size_t *lengths,
        const unsigned char **binaries,
        cl_int *binaryStatus,
        cl_int &errcodeRet);

    // Create program from source
    template <typename T = Program>
    static T *create(
        cl_context context,
        cl_uint count,
        const char **strings,
        const size_t *lengths,
        cl_int &errcodeRet);

    template <typename T = Program>
    static T *create(
        const char *nullTerminatedString,
        Context *context,
        ClDevice &device,
        bool isBuiltIn,
        cl_int *errcodeRet);

    template <typename T = Program>
    static T *create(
        const char *nullTerminatedString,
        Context *context,
        Device &device,
        bool isBuiltIn,
        cl_int *errcodeRet);

    template <typename T = Program>
    static T *createFromGenBinary(
        ExecutionEnvironment &executionEnvironment,
        Context *context,
        const void *binary,
        size_t size,
        bool isBuiltIn,
        cl_int *errcodeRet);

    template <typename T = Program>
    static T *createFromIL(Context *context,
                           const void *il,
                           size_t length,
                           cl_int &errcodeRet);

    Program(ExecutionEnvironment &executionEnvironment, Context *context, bool isBuiltIn);
    ~Program() override;

    Program(const Program &) = delete;
    Program &operator=(const Program &) = delete;

    cl_int build(cl_uint numDevices, const cl_device_id *deviceList, const char *buildOptions,
                 void(CL_CALLBACK *funcNotify)(cl_program program, void *userData),
                 void *userData, bool enableCaching);

    cl_int build(const Device *pDevice, const char *buildOptions, bool enableCaching,
                 std::unordered_map<std::string, BuiltinDispatchInfoBuilder *> &builtinsMap);

    MOCKABLE_VIRTUAL cl_int processGenBinary();
    MOCKABLE_VIRTUAL cl_int processPatchTokensBinary(ArrayRef<const uint8_t> src, ProgramInfo &dst);
    MOCKABLE_VIRTUAL cl_int processProgramInfo(ProgramInfo &dst);

    cl_int compile(cl_uint numDevices, const cl_device_id *deviceList, const char *buildOptions,
                   cl_uint numInputHeaders, const cl_program *inputHeaders, const char **headerIncludeNames,
                   void(CL_CALLBACK *funcNotify)(cl_program program, void *userData),
                   void *userData);

    cl_int link(cl_uint numDevices, const cl_device_id *deviceList, const char *buildOptions,
                cl_uint numInputPrograms, const cl_program *inputPrograms,
                void(CL_CALLBACK *funcNotify)(cl_program program, void *userData),
                void *userData);

    cl_int setProgramSpecializationConstant(cl_uint specId, size_t specSize, const void *specValue);
    MOCKABLE_VIRTUAL cl_int updateSpecializationConstant(cl_uint specId, size_t specSize, const void *specValue);

    size_t getNumKernels() const;
    const KernelInfo *getKernelInfo(const char *kernelName) const;
    const KernelInfo *getKernelInfo(size_t ordinal) const;

    cl_int getInfo(cl_program_info paramName, size_t paramValueSize,
                   void *paramValue, size_t *paramValueSizeRet);

    cl_int getBuildInfo(cl_device_id device, cl_program_build_info paramName,
                        size_t paramValueSize, void *paramValue, size_t *paramValueSizeRet) const;

    cl_build_status getBuildStatus() const {
        return buildStatus;
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

    const ClDevice &getDevice(cl_uint deviceOrdinal) const {
        UNRECOVERABLE_IF(pDevice == nullptr);
        return *pDevice;
    }

    void setDevice(Device *device);

    MOCKABLE_VIRTUAL cl_int processElfBinary(const void *pBinary, size_t binarySize, uint32_t &binaryVersion);
    cl_int processSpirBinary(const void *pBinary, size_t binarySize, bool isSpirV);

    cl_int getSource(std::string &binary) const;

    void processDebugData();

    void updateBuildLog(const ClDevice *pDevice, const char *pErrorString, const size_t errorStringSize);

    const char *getBuildLog(const ClDevice *pDevice) const;

    cl_uint getProgramBinaryType() const {
        return programBinaryType;
    }

    bool getIsSpirV() const {
        return isSpirV;
    }

    GraphicsAllocation *getConstantSurface() const {
        return constantSurface;
    }

    GraphicsAllocation *getGlobalSurface() const {
        return globalSurface;
    }

    GraphicsAllocation *getExportedFunctionsSurface() const {
        return exportedFunctionsSurface;
    }

    BlockKernelManager *getBlockKernelManager() const {
        return blockKernelManager;
    }

    void allocateBlockPrivateSurfaces(uint32_t rootDeviceIndex);
    void freeBlockResources();
    void cleanCurrentKernelInfo();

    const std::string &getOptions() const { return options; }

    const std::string &getInternalOptions() const { return internalOptions; }

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

    static bool isValidLlvmBinary(const void *pBinary, size_t binarySize);
    static bool isValidSpirvBinary(const void *pBinary, size_t binarySize);
    bool isKernelDebugEnabled() {
        return kernelDebugEnabled;
    }

    char *getDebugData() {
        return debugData.get();
    }

    size_t getDebugDataSize() {
        return debugDataSize;
    }

    const Linker::RelocatedSymbolsMap &getSymbols() const {
        return this->symbols;
    }

    LinkerInput *getLinkerInput() const {
        return this->linkerInput.get();
    }

    MOCKABLE_VIRTUAL bool isSafeToSkipUnhandledToken(unsigned int token) const;

  protected:
    Program(ExecutionEnvironment &executionEnvironment);

    MOCKABLE_VIRTUAL cl_int createProgramFromBinary(const void *pBinary, size_t binarySize);

    cl_int resolveProgramBinary();

    MOCKABLE_VIRTUAL cl_int linkBinary();

    MOCKABLE_VIRTUAL cl_int isHandled(const PatchTokenBinary::ProgramFromPatchtokens &decodedProgram) const;

    MOCKABLE_VIRTUAL cl_int rebuildProgramFromIr();

    bool validateGenBinaryDevice(GFXCORE_FAMILY device) const;
    bool validateGenBinaryHeader(const iOpenCL::SProgramBinaryHeader *pGenBinaryHeader) const;

    void separateBlockKernels();

    void updateNonUniformFlag();
    void updateNonUniformFlag(const Program **inputProgram, size_t numInputPrograms);

    void extractInternalOptions(const std::string &options);
    MOCKABLE_VIRTUAL bool isFlagOption(ConstStringRef option);
    MOCKABLE_VIRTUAL bool isOptionValueValid(ConstStringRef option, ConstStringRef value);
    MOCKABLE_VIRTUAL void applyAdditionalOptions();

    MOCKABLE_VIRTUAL bool appendKernelDebugOptions();
    void notifyDebuggerWithSourceCode(std::string &filename);

    static const std::string clOptNameClVer;

    cl_program_binary_type programBinaryType;
    bool isSpirV = false;
    CLElfLib::ElfBinaryStorage elfBinary;
    size_t elfBinarySize;

    std::unique_ptr<char[]> genBinary;
    size_t genBinarySize;

    std::unique_ptr<char[]> irBinary;
    size_t irBinarySize;

    std::unique_ptr<char[]> debugData;
    size_t debugDataSize;

    CreatedFrom createdFrom = CreatedFrom::UNKNOWN;

    std::vector<KernelInfo *> kernelInfoArray;
    std::vector<KernelInfo *> parentKernelInfoArray;
    std::vector<KernelInfo *> subgroupKernelInfoArray;

    GraphicsAllocation *constantSurface;
    GraphicsAllocation *globalSurface;
    GraphicsAllocation *exportedFunctionsSurface = nullptr;

    size_t globalVarTotalSize;

    cl_build_status buildStatus;
    bool isCreatedFromBinary;
    bool isProgramBinaryResolved;

    std::string sourceCode;
    std::string options;
    std::string internalOptions;
    static const std::vector<ConstStringRef> internalOptionsToExtract;

    uint32_t programOptionVersion;
    bool allowNonUniform;

    std::unique_ptr<LinkerInput> linkerInput;
    Linker::RelocatedSymbolsMap symbols;

    std::map<const ClDevice *, std::string> buildLog;

    bool areSpecializationConstantsInitialized = false;
    CIF::RAII::UPtr_t<CIF::Builtins::BufferSimple> specConstantsIds;
    CIF::RAII::UPtr_t<CIF::Builtins::BufferSimple> specConstantsSizes;
    CIF::RAII::UPtr_t<CIF::Builtins::BufferSimple> specConstantsValues;

    BlockKernelManager *blockKernelManager;
    ExecutionEnvironment &executionEnvironment;
    Context *context;
    ClDevice *pDevice;
    cl_uint numDevices;

    bool isBuiltIn;
    bool kernelDebugEnabled = false;
};

} // namespace NEO
