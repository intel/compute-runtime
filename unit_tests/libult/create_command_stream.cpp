/*
 * Copyright (c) 2018, Intel Corporation
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

#include "runtime/command_stream/create_command_stream_impl.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/helpers/options.h"
#include "runtime/command_stream/aub_command_stream_receiver.h"
#include "runtime/command_stream/tbx_command_stream_receiver.h"
#include "unit_tests/libult/ult_command_stream_receiver.h"
#include <cassert>

namespace OCLRT {

extern CommandStreamReceiverCreateFunc commandStreamReceiverFactory[2 * IGFX_MAX_CORE];
bool getDevicesResult = true;

bool overrideCommandStreamReceiverCreation = false;
bool overrideDeviceWithDefaultHardwareInfo = true;

CommandStreamReceiver *createCommandStream(const HardwareInfo *pHwInfo, ExecutionEnvironment &executionEnvironment) {
    CommandStreamReceiver *commandStreamReceiver = nullptr;
    assert(nullptr != pHwInfo->pPlatform);
    auto offset = !overrideCommandStreamReceiverCreation ? IGFX_MAX_CORE : 0;
    overrideCommandStreamReceiverCreation = false;
    if (offset != 0) {
        auto funcCreate = commandStreamReceiverFactory[offset + pHwInfo->pPlatform->eRenderCoreFamily];
        if (funcCreate) {
            commandStreamReceiver = funcCreate(*pHwInfo, false, executionEnvironment);
        }
    } else {
        commandStreamReceiver = createCommandStreamImpl(pHwInfo, executionEnvironment);
    }
    return commandStreamReceiver;
}

bool getDevices(HardwareInfo **hwInfo, size_t &numDevicesReturned, ExecutionEnvironment &executionEnvironment) {
    if (overrideDeviceWithDefaultHardwareInfo) {
        *hwInfo = const_cast<HardwareInfo *>(*platformDevices);
        numDevicesReturned = numPlatformDevices;
        return getDevicesResult;
    }

    return getDevicesImpl(hwInfo, numDevicesReturned, executionEnvironment);
}
} // namespace OCLRT
