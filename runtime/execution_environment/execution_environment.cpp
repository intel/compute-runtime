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

#include "runtime/execution_environment/execution_environment.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/source_level_debugger/source_level_debugger.h"
#include "runtime/built_ins/sip.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/os_interface/device_factory.h"

namespace OCLRT {
ExecutionEnvironment::ExecutionEnvironment() = default;
ExecutionEnvironment::~ExecutionEnvironment() = default;
extern CommandStreamReceiver *createCommandStream(const HardwareInfo *pHwInfo);

void ExecutionEnvironment::initGmm(const HardwareInfo *hwInfo) {
    if (!gmmHelper) {
        gmmHelper.reset(new GmmHelper(hwInfo));
    }
}
bool ExecutionEnvironment::initializeCommandStreamReceiver(const HardwareInfo *pHwInfo) {
    if (this->commandStreamReceiver) {
        return true;
    }
    CommandStreamReceiver *commandStreamReceiver = createCommandStream(pHwInfo);
    if (!commandStreamReceiver) {
        return false;
    }
    this->commandStreamReceiver.reset(commandStreamReceiver);
    return true;
}
void ExecutionEnvironment::initializeMemoryManager(bool enable64KBpages) {
    if (this->memoryManager) {
        commandStreamReceiver->setMemoryManager(this->memoryManager.get());
        return;
    }

    memoryManager.reset(commandStreamReceiver->createMemoryManager(enable64KBpages));
    commandStreamReceiver->setMemoryManager(memoryManager.get());

    DEBUG_BREAK_IF(!this->memoryManager);
}
void ExecutionEnvironment::initSourceLevelDebugger(const HardwareInfo &hwInfo) {
    if (hwInfo.capabilityTable.sourceLevelDebuggerSupported) {
        sourceLevelDebugger.reset(SourceLevelDebugger::create());
    }
    if (sourceLevelDebugger) {
        bool localMemorySipAvailable = (SipKernelType::DbgCsrLocal == SipKernel::getSipKernelType(hwInfo.pPlatform->eRenderCoreFamily, true));
        sourceLevelDebugger->initialize(localMemorySipAvailable);
    }
}
GmmHelper *ExecutionEnvironment::getGmmHelper() const {
    return gmmHelper.get();
}
} // namespace OCLRT
