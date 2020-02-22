/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/command_stream/command_stream_receiver_with_aub_dump.inl"
#include "opencl/source/os_interface/linux/device_command_stream.inl"
#include "opencl/source/os_interface/linux/drm_command_stream.inl"
#include "opencl/source/os_interface/linux/drm_command_stream_bdw_plus.inl"

namespace NEO {

template class DeviceCommandStreamReceiver<TGLLPFamily>;
template class DrmCommandStreamReceiver<TGLLPFamily>;
template class CommandStreamReceiverWithAUBDump<DrmCommandStreamReceiver<TGLLPFamily>>;
} // namespace NEO
