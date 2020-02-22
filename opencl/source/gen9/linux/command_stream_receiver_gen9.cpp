/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/command_stream/command_stream_receiver_with_aub_dump.inl"
#include "opencl/source/os_interface/linux/device_command_stream.inl"
#include "opencl/source/os_interface/linux/drm_command_stream.inl"
#include "opencl/source/os_interface/linux/drm_command_stream_bdw_plus.inl"

namespace NEO {

template class DeviceCommandStreamReceiver<SKLFamily>;
template class DrmCommandStreamReceiver<SKLFamily>;
template class CommandStreamReceiverWithAUBDump<DrmCommandStreamReceiver<SKLFamily>>;
} // namespace NEO
