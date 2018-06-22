/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once
#include "platform_info.h"
#include "runtime/api/cl_types.h"
#include "runtime/device/device_vector.h"
#include "runtime/helpers/base_object.h"
#include <condition_variable>
#include <vector>

namespace OCLRT {

class CompilerInterface;
class Device;
class AsyncEventsHandler;
class ExecutionEnvironment;
struct HardwareInfo;

template <>
struct OpenCLObjectMapper<_cl_platform_id> {
    typedef class Platform DerivedType;
};

class Platform : public BaseObject<_cl_platform_id> {
  public:
    static const cl_ulong objectMagic = 0x8873ACDEF2342133LL;

    Platform();
    ~Platform() override;

    cl_int getInfo(cl_platform_info paramName,
                   size_t paramValueSize,
                   void *paramValue,
                   size_t *paramValueSizeRet);

    const std::string &peekCompilerExtensions() const;

    bool initialize();
    bool isInitialized();
    void shutdown();

    size_t getNumDevices() const;
    Device **getDevices();
    Device *getDevice(size_t deviceOrdinal);

    const PlatformInfo &getPlatformInfo() const;
    AsyncEventsHandler *getAsyncEventsHandler();
    std::unique_ptr<AsyncEventsHandler> setAsyncEventsHandler(std::unique_ptr<AsyncEventsHandler> handler);
    ExecutionEnvironment *peekExecutionEnvironment() { return executionEnvironment; }

  protected:
    enum {
        StateNone,
        StateIniting,
        StateInited,
    };
    cl_uint state = StateNone;
    void fillGlobalDispatchTable();

    PlatformInfo *platformInfo = nullptr;
    DeviceVector devices;
    std::string compilerExtensions;
    std::unique_ptr<AsyncEventsHandler> asyncEventsHandler;
    ExecutionEnvironment *executionEnvironment = nullptr;
};

Platform *platform();
} // namespace OCLRT
