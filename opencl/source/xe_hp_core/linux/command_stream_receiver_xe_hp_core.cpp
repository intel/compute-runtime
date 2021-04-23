/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/command_stream/command_stream_receiver_with_aub_dump.inl"
#include "opencl/source/os_interface/linux/device_command_stream.inl"
#include "opencl/source/os_interface/linux/drm_command_stream.inl"
#include "opencl/source/os_interface/linux/drm_command_stream_xehp_plus.inl"

namespace NEO {

template class DeviceCommandStreamReceiver<XeHpFamily>;
template class DrmCommandStreamReceiver<XeHpFamily>;
template class CommandStreamReceiverWithAUBDump<DrmCommandStreamReceiver<XeHpFamily>>;
} // namespace NEO
