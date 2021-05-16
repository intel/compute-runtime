/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/built_ins/sip_kernel_type.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/program/program_info.h"

#include <memory>
#include <vector>
namespace NEO {

class Device;
class GraphicsAllocation;
class MemoryManager;
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

    static bool initSipKernel(SipKernelType type, Device &device);
    static void freeSipKernels(RootDeviceEnvironment *rootDeviceEnvironment, MemoryManager *memoryManager);

    static const SipKernel &getSipKernel(Device &device);
    static SipKernelType getSipKernelType(Device &device);

    static const size_t maxDbgSurfaceSize;
    static SipClassType classType;

  protected:
    static bool initSipKernelImpl(SipKernelType type, Device &device);
    static const SipKernel &getSipKernelImpl(Device &device);

    static bool initBuiltinsSipKernel(SipKernelType type, Device &device);
    static bool initRawBinaryFromFileKernel(SipKernelType type, Device &device, std::string &fileName);

    static void selectSipClassType(std::string &fileName);

    const std::vector<char> stateSaveAreaHeader;
    GraphicsAllocation *sipAllocation = nullptr;
    SipKernelType type = SipKernelType::COUNT;
};
} // namespace NEO
