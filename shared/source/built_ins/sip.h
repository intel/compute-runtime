/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/built_ins/sip_kernel_type.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include <string>
#include <vector>

namespace NEO {

class Device;
class GraphicsAllocation;
class MemoryManager;
class GfxCoreHelper;
class OsContext;

struct RootDeviceEnvironment;

class SipKernel : NEO::NonCopyableAndNonMovableClass {
  public:
    SipKernel(SipKernelType type, GraphicsAllocation *sipAlloc, std::vector<char> ssah, std::vector<char> binary);
    SipKernel(SipKernelType type, GraphicsAllocation *sipAlloc, std::vector<char> ssah);
    virtual ~SipKernel();

    SipKernelType getType() const {
        return type;
    }

    size_t getCtxOffset() const {
        return contextIdOffsets[0];
    }

    size_t getPidOffset() const {
        return contextIdOffsets[1];
    }

    MOCKABLE_VIRTUAL GraphicsAllocation *getSipAllocation() const;
    MOCKABLE_VIRTUAL const std::vector<char> &getStateSaveAreaHeader() const;
    MOCKABLE_VIRTUAL size_t getStateSaveAreaSize(Device *device) const;
    MOCKABLE_VIRTUAL const std::vector<char> &getBinary() const;

    MOCKABLE_VIRTUAL void parseBinaryForContextId();

    static bool initSipKernel(SipKernelType type, Device &device);
    static void freeSipKernels(RootDeviceEnvironment *rootDeviceEnvironment, MemoryManager *memoryManager);

    static const SipKernel &getSipKernel(Device &device, OsContext *context);
    static const SipKernel &getDebugSipKernel(Device &device);
    static const SipKernel &getDebugSipKernel(Device &device, OsContext *context);
    static SipKernelType getSipKernelType(Device &device);
    static SipKernelType getSipKernelType(Device &device, bool debuggingEnable);
    static SipClassType classType;

    enum class Command : uint32_t {
        resume,
        ready,
        slmRead,
        slmWrite
    };

  protected:
    static bool initSipKernelImpl(SipKernelType type, Device &device, OsContext *context);
    static const SipKernel &getSipKernelImpl(Device &device);

    static bool initBuiltinsSipKernel(SipKernelType type, Device &device, OsContext *context);
    static bool initRawBinaryFromFileKernel(SipKernelType type, Device &device, std::string &fileName);
    static std::vector<char> readStateSaveAreaHeaderFromFile(const std::string &fileName);
    static std::string createHeaderFilename(const std::string &filename);

    static bool initHexadecimalArraySipKernel(SipKernelType type, Device &device);
    static bool initSipKernelFromExternalLib(SipKernelType type, Device &device);
    static void selectSipClassType(std::string &fileName, Device &device);

    const std::vector<char> stateSaveAreaHeader;
    const std::vector<char> binary;
    size_t contextIdOffsets[2] = {0, 0};
    GraphicsAllocation *sipAllocation = nullptr;
    SipKernelType type = SipKernelType::count;
};

static_assert(NEO::NonCopyableAndNonMovable<SipKernel>);

} // namespace NEO
