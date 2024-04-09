/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/device/device.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/driver_model_type.h"
#include "shared/source/os_interface/os_interface.h"

#include "level_zero/core/source/builtin/builtin_functions_lib_impl.h"
#include "level_zero/core/source/device/device.h"

namespace L0 {

std::unique_ptr<BuiltinFunctionsLib> BuiltinFunctionsLib::create(Device *device,
                                                                 NEO::BuiltIns *builtins) {
    return std::unique_ptr<BuiltinFunctionsLib>(new BuiltinFunctionsLibImpl(device, builtins));
}

bool BuiltinFunctionsLibImpl::initBuiltinsAsyncEnabled(Device *device) {
    return device->getNEODevice()->getRootDeviceEnvironment().osInterface.get() &&
           device->getNEODevice()->getRootDeviceEnvironment().osInterface->getDriverModel()->getDriverModelType() == NEO::DriverModelType::drm &&
           device->getNEODevice()->getDefaultEngine().commandStreamReceiver->getType() == NEO::CommandStreamReceiverType::hardware &&
           device->getNEODevice()->getRootDeviceEnvironment().getProductHelper().isInitBuiltinAsyncSupported(device->getHwInfo());
}

} // namespace L0
