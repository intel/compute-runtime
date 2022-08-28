/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/built_ins/sip_kernel_type.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace NEO {

class Device;
class GraphicsAllocation;
class MemoryManager;

struct HardwareInfo;
struct RootDeviceEnvironment;

class SipKernel {
  public:
    SipKernel(SipKernelType type, GraphicsAllocation *sipAlloc, std::vector<char> ssah);
    SipKernel(const SipKernel &) = delete;
    SipKernel &operator=(const SipKernel &) = delete;
    SipKernel(SipKernel &&) = delete;
    SipKernel &operator=(SipKernel &&) = delete;
    virtual ~SipKernel();

    SipKernelType getType() const {
        return type;
    }

    MOCKABLE_VIRTUAL GraphicsAllocation *getSipAllocation() const;
    MOCKABLE_VIRTUAL const std::vector<char> &getStateSaveAreaHeader() const;
    MOCKABLE_VIRTUAL size_t getStateSaveAreaSize(Device *device) const;

    static bool initSipKernel(SipKernelType type, Device &device);
    static void freeSipKernels(RootDeviceEnvironment *rootDeviceEnvironment, MemoryManager *memoryManager);

    static const SipKernel &getSipKernel(Device &device);
    static const SipKernel &getBindlessDebugSipKernel(Device &device);
    static SipKernelType getSipKernelType(Device &device);
    static SipKernelType getSipKernelType(Device &device, bool debuggingEnable);
    static SipClassType classType;

    enum class COMMAND : uint32_t {
        RESUME,
        READY,
        SLM_READ,
        SLM_WRITE
    };

  protected:
    static bool initSipKernelImpl(SipKernelType type, Device &device);
    static const SipKernel &getSipKernelImpl(Device &device);

    static bool initBuiltinsSipKernel(SipKernelType type, Device &device);
    static bool initRawBinaryFromFileKernel(SipKernelType type, Device &device, std::string &fileName);
    static std::vector<char> readStateSaveAreaHeaderFromFile(const std::string &fileName);
    static std::string createHeaderFilename(const std::string &filename);

    static bool initHexadecimalArraySipKernel(SipKernelType type, Device &device);
    static void selectSipClassType(std::string &fileName, const HardwareInfo &hwInfo);

    const std::vector<char> stateSaveAreaHeader;
    GraphicsAllocation *sipAllocation = nullptr;
    SipKernelType type = SipKernelType::COUNT;
};

} // namespace NEO
