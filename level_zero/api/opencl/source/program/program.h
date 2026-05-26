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

#include <span>

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

    cl_int createFromBinary(cl_device_id device, size_t length, const unsigned char *binary);
    cl_int buildFromSource(cl_device_id device, const char *options);
    cl_int compileFromSourceWithHeaders(cl_device_id device, const char *options, cl_uint numInputHeaders,
                                        const cl_program *inputHeaders, const char **headerIncludeNames);
    cl_int buildFromIL(cl_device_id device, const char *options);
    cl_int setProgramSpecializationConstant(cl_uint specId, size_t specSize, const void *specValue);
    void setProgramBinaryType(cl_program_binary_type programBinaryType) { this->programBinaryType = programBinaryType; }
    void setBuildStatus(cl_build_status buildStatus) { this->buildStatus = buildStatus; }
    void setBuildLogHandle(ze_module_build_log_handle_t buildLogHandle) { this->buildLogHandle = buildLogHandle; }
    void setModuleHandle(ze_module_handle_t moduleHandle) { this->moduleHandle = moduleHandle; }

    CreatedFrom getCreatedFrom() const;
    Context *getContext();
    std::string_view getSource() const;
    const char *getIrBinary() const;
    size_t getIrBinarySize() const;
    bool getIsSpirv() const;
    ze_module_handle_t getModuleHandle() const;
    L0::ModuleImp *getL0Object() const { return static_cast<L0::ModuleImp *>(L0::Module::fromHandle(this->moduleHandle)); }
    static bool isValidCallback(void(CL_CALLBACK *funcNotify)(cl_program program, void *userData), void *userData);
    void invokeCallback(void(CL_CALLBACK *funcNotify)(cl_program program, void *userData), void *userData);

  protected:
    specConstValuesMap specConstantsValues;
    std::mutex specializationConstantsMutex;
    std::string source{};
    std::string options{};
    Context *context = nullptr;
    ze_module_handle_t moduleHandle = nullptr;
    ze_module_build_log_handle_t buildLogHandle = nullptr;
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
