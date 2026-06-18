/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/api/opencl/source/api/cl_types.h"
#include "level_zero/api/opencl/source/context/context.h"
#include "level_zero/api/opencl/source/helpers/base_object.h"
#include "level_zero/core/source/module/module_imp.h"

#include <map>
#include <span>
#include <vector>

namespace NEO {
namespace LEO {

template <>
struct OpenCLObjectMapper<_cl_program> {
    typedef class Program DerivedType;
};

class Program : public BaseObject<_cl_program> {
    /**
     * OpenCL program compilation / linking / build paths:
     *
     *
     *   [OpenCL C] ---compile---> [SPIR-V] ---.
     *                                         |
     *   import [SPIR-V]  -- (as link input) --+--- N x [SPIR-V]  (any number,
     *   import [LLVM BC] -- (as link input) --+--- N x [LLVM BC]  any mix)
     *                                         |         |
     *                                         |         +--- link -----------> [Gen Binary]
     *                                         |         |
     *                                         |         +--- link (lib) -----> [LLVM BC Library]
     *                                         |                                      |
     *                                         '---------- (reusable as link input) --'
     *
     *   import [Gen Binary] ------------------------> [Gen Binary]
     *
     *   [OpenCL C] ---build---> ([SPIR-V])* --------> [Gen Binary]
     *                              ^ impl detail
     *
     *   * SPIR-V is an internal implementation detail of the build path;
     *     it is not exposed to the caller.
     */
  public:
    static const cl_ulong objectMagic = 0x5651C89100AAACFELL;
    enum class CreatedFrom {
        source,
        il,
        binary,
        unknown
    };

    Program(Context *context, cl_uint count, const char **strings, const size_t *lengths);
    Program(Context *context);
    Program() = delete;
    ~Program() override;

    cl_int getInfo(cl_program_info paramName, size_t paramValueSize,
                   void *paramValue, size_t *paramValueSizeRet);

    cl_int getBuildInfo(cl_device_id device, cl_program_build_info paramName,
                        size_t paramValueSize, void *paramValue, size_t *paramValueSizeRet);

    cl_int createFromBinaryOrIl(cl_device_id device, size_t length, const unsigned char *binary);
    cl_int buildFromSource(const char *options);
    cl_int compileFromSourceWithHeaders(const char *options, cl_uint numInputHeaders,
                                        const cl_program *inputHeaders, const char **headerIncludeNames);
    cl_int buildFromIL(const char *options);
    cl_int link(const char *options, cl_uint numInputPrograms, const cl_program *inputPrograms);
    cl_int captureIrForLibraryOutput();
    cl_int setProgramSpecializationConstant(cl_uint specId, size_t specSize, const void *specValue);
    void setProgramBinaryType(cl_program_binary_type programBinaryType) { this->programBinaryType = programBinaryType; }
    void setBuildStatus(cl_build_status buildStatus) { this->buildStatus = buildStatus; }
    void setBuildLogHandle(uint32_t rootDeviceIndex, ze_module_build_log_handle_t buildLogHandle) { this->buildLogHandles[rootDeviceIndex] = buildLogHandle; }
    void setModuleHandle(uint32_t rootDeviceIndex, ze_module_handle_t moduleHandle) { this->moduleHandles[rootDeviceIndex] = moduleHandle; }

    CreatedFrom getCreatedFrom() const;
    Context *getContext();
    std::string_view getSource() const;
    const char *getIrBinary() const;
    size_t getIrBinarySize() const;
    bool getIsSpirv() const;
    ze_module_handle_t getModuleHandle() const;
    ze_module_handle_t getModuleHandle(uint32_t rootDeviceIndex) const;
    const std::map<uint32_t, ze_module_handle_t> &getModuleHandles() const { return this->moduleHandles; }
    std::vector<const char *> getUserKernelNames() const;
    L0::ModuleImp *getL0Object() const { return static_cast<L0::ModuleImp *>(L0::Module::fromHandle(this->getModuleHandle())); }
    L0::ModuleImp *getL0Object(uint32_t rootDeviceIndex) const { return static_cast<L0::ModuleImp *>(L0::Module::fromHandle(this->getModuleHandle(rootDeviceIndex))); }
    static bool isValidCallback(void(CL_CALLBACK *funcNotify)(cl_program program, void *userData), void *userData);
    void invokeCallback(void(CL_CALLBACK *funcNotify)(cl_program program, void *userData), void *userData);

    // clBuildProgram / clCompileProgram: dispatch on createdFrom, drive the CL_BUILD_ERROR ->
    // CL_BUILD_SUCCESS transition, and invoke the notify callback once.
    cl_int build(const char *options,
                 void(CL_CALLBACK *funcNotify)(cl_program program, void *userData), void *userData);
    cl_int compile(const char *options, cl_uint numInputHeaders,
                   const cl_program *inputHeaders, const char **headerIncludeNames,
                   void(CL_CALLBACK *funcNotify)(cl_program program, void *userData), void *userData);

  protected:
    // buildFailureCode is the CL error this build action reports when L0 fails the module build/link
    // (clBuildProgram/clCompileProgram/clLinkProgram each have a distinct one); other L0 errors map
    // through the generic L0ToClResultMapper.
    cl_int buildModulesForContextDevices(ze_module_desc_t &moduleDescription, cl_int buildFailureCode);
    static cl_int mapModuleBuildResult(ze_result_t ret, cl_int buildFailureCode);
    cl_int populateIrBinaryFromModule(bool isSpirv);
    // Reads the per-device program binary (native gen binary for EXECUTABLE, IR otherwise).
    // outSize always receives the size; when outBinary is non-null the bytes are copied out.
    cl_int getDeviceBinary(uint32_t rootDeviceIndex, size_t *outSize, unsigned char *outBinary);
    // Fills the L0 module spec-constant descriptor from specConstantsValues into caller-owned
    // storage (which must outlive the build). Caller must hold specializationConstantsMutex.
    bool populateModuleConstants(ze_module_constants_t &moduleConstants,
                                 std::vector<uint32_t> &constantsIds,
                                 std::vector<const void *> &constantsValuesPtrs);
    void resetModules();

    specConstValuesMap specConstantsValues;
    std::mutex specializationConstantsMutex;
    std::string source{};
    std::string options{};
    Context *context = nullptr;
    std::map<uint32_t, ze_module_handle_t> moduleHandles{};
    std::map<uint32_t, ze_module_build_log_handle_t> buildLogHandles{};
    std::unique_ptr<char[]> irBinary;
    size_t irBinarySize = 0U;
    cl_program_binary_type programBinaryType = CL_PROGRAM_BINARY_TYPE_NONE;
    CreatedFrom createdFrom = CreatedFrom::unknown;
    cl_build_status buildStatus = CL_BUILD_NONE;
    bool isSpirv = false;
};

static_assert(NEO::NonCopyableAndNonMovable<Program>);

} // namespace LEO
} // namespace NEO
