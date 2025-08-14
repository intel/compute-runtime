/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/os_interface/linux/hw_device_id.h"

#include <mutex>

namespace L0 {
namespace Sysman {
class SysmanHwDeviceIdDrm : public NEO::HwDeviceIdDrm, NEO::NonCopyableAndNonMovableClass {
  public:
    using NEO::HwDeviceIdDrm::HwDeviceIdDrm;
    class SingleInstance {
      public:
        SingleInstance(SysmanHwDeviceIdDrm &input) : instance(input), fileDescriptor(input.openFileDescriptor()) {}
        ~SingleInstance() { instance.closeFileDescriptor(); }
        int getFileDescriptor() const { return fileDescriptor; }

      private:
        SysmanHwDeviceIdDrm &instance;
        const int fileDescriptor;
    };
    SysmanHwDeviceIdDrm() = delete;

    SingleInstance getSingleInstance() {
        return SingleInstance(*this);
    }

  private:
    MOCKABLE_VIRTUAL int openFileDescriptor();
    MOCKABLE_VIRTUAL int closeFileDescriptor();

    std::mutex fdMutex{};
    uint32_t fdRefCounter = 0;
};

static_assert(NEO::NonCopyableAndNonMovable<SysmanHwDeviceIdDrm>);

} // namespace Sysman
} // namespace L0
