/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_debug.h"
#include "shared/source/os_interface/linux/i915_prelim.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/libult/linux/drm_mock_helper.h"
#include "shared/test/common/libult/linux/drm_query_mock.h"
#include "shared/test/common/mocks/mock_sip.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/hw_helpers/l0_hw_helper.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"
#include "level_zero/tools/source/debug/linux/prelim/debug_session.h"

#include "common/StateSaveAreaHeader.h"

#include <atomic>
#include <queue>
#include <type_traits>
#include <unordered_map>

namespace L0 {
namespace ult {

using typeOfUUID = std::decay<decltype(prelim_drm_i915_debug_event_vm_bind::uuids[0])>::type;

struct MockIoctlHandler : public L0::DebugSessionLinux::IoctlHandler {
    using EventPair = std::pair<char *, uint64_t>;
    using EventQueue = std::queue<EventPair>;

    int ioctl(int fd, unsigned long request, void *arg) override {
        ioctlCalled++;

        if ((request == PRELIM_I915_DEBUG_IOCTL_READ_EVENT) && (arg != nullptr)) {
            auto debugEvent = reinterpret_cast<prelim_drm_i915_debug_event *>(arg);
            debugEventInput = *debugEvent;

            if (!eventQueue.empty()) {
                auto frontEvent = eventQueue.front();
                memcpy(arg, frontEvent.first, frontEvent.second);
                eventQueue.pop();
                return 0;
            } else {
                debugEventRetVal = -1;
            }
            return debugEventRetVal;
        } else if ((request == PRELIM_I915_DEBUG_IOCTL_ACK_EVENT) && (arg != nullptr)) {
            auto debugEvent = reinterpret_cast<prelim_drm_i915_debug_event *>(arg);
            debugEventAcked = *debugEvent;
            ackCount++;
            return 0;
        } else if ((request == PRELIM_I915_DEBUG_IOCTL_READ_UUID) && (arg != nullptr)) {
            prelim_drm_i915_debug_read_uuid *uuid = reinterpret_cast<prelim_drm_i915_debug_read_uuid *>(arg);
            if (returnUuid) {
                uuid->client_handle = returnUuid->client_handle;
                uuid->handle = returnUuid->handle;
                uuid->flags = returnUuid->flags;
                memcpy(uuid->uuid, returnUuid->uuid, sizeof(prelim_drm_i915_debug_read_uuid::uuid));
                int returnError = 0;
                if (uuid->payload_size >= returnUuid->payload_size) {
                    memcpy(reinterpret_cast<void *>(uuid->payload_ptr), reinterpret_cast<void *>(returnUuid->payload_ptr), returnUuid->payload_size);
                } else {
                    returnError = -1;
                }
                returnUuid = nullptr;
                return returnError;
            }
            return -1;
        } else if ((request == PRELIM_I915_DEBUG_IOCTL_VM_OPEN) && (arg != nullptr)) {
            prelim_drm_i915_debug_vm_open *vmOpenIn = reinterpret_cast<prelim_drm_i915_debug_vm_open *>(arg);
            vmOpen = *vmOpenIn;
            return vmOpenRetVal;
        } else if ((request == PRELIM_I915_DEBUG_IOCTL_EU_CONTROL) && (arg != nullptr)) {
            prelim_drm_i915_debug_eu_control *euControlArg = reinterpret_cast<prelim_drm_i915_debug_eu_control *>(arg);
            euControl = *euControlArg;

            euControlArg->seqno = euControlOutputSeqno;

            if (euControlArg->bitmask_size != 0) {
                euControlBitmaskSize = euControlArg->bitmask_size;
                euControlBitmask = std::make_unique<uint8_t[]>(euControlBitmaskSize);

                memcpy(euControlBitmask.get(), reinterpret_cast<void *>(euControlArg->bitmask_ptr), euControlBitmaskSize);
            }
        }

        return ioctlRetVal;
    }

    int poll(pollfd *pollFd, unsigned long int numberOfFds, int timeout) override {
        passedTimeout = timeout;
        pollCounter++;

        if (eventQueue.empty() && pollRetVal >= 0) {
            return 0;
        }
        return pollRetVal;
    }

    int64_t pread(int fd, void *buf, size_t count, off_t offset) override {
        preadCalled++;
        preadOffset = offset;
        if ((midZeroReturn > 0) && (preadCalled > 1)) {
            midZeroReturn--;
            return 0;
        }

        if (!pReadArrayRef.empty()) {
            auto offsetInMemory = offset - pReadBase;
            auto result = memcpy_s(buf, count, pReadArrayRef.begin() + offsetInMemory, std::min(count, pReadArrayRef.size() - offsetInMemory));
            if (result == 0) {
                return count;
            }
            return -1;
        }

        if (count > 0 && preadRetVal > 0) {
            memset(buf, 0xaa, count);
        }
        return preadRetVal;
    }

    int64_t pwrite(int fd, const void *buf, size_t count, off_t offset) override {
        pwriteCalled++;
        pwriteOffset = offset;
        if ((midZeroReturn > 0) && (pwriteCalled > 1)) {
            midZeroReturn--;
            return 0;
        }

        if (!pWriteArrayRef.empty()) {
            auto offsetInMemory = offset - pWriteBase;
            auto result = memcpy_s(pWriteArrayRef.begin() + offsetInMemory, pWriteArrayRef.size() - offsetInMemory, buf, count);
            if (result == 0) {
                return count;
            }
            return -1;
        }

        return pwriteRetVal;
    }

    void *mmap(void *addr, size_t size, int prot, int flags, int fd, off_t off) override {
        mmapCalled++;
        if (mmapFail) {
            return MAP_FAILED;
        }
        if (mmapRet) {
            return mmapRet + (off - mmapBase);
        } else {
            auto ret = new char[size];
            if (size > 0) {
                memset(ret, 0xaa, size);
            }
            return ret;
        }
    }

    int munmap(void *addr, size_t size) override {
        munmapCalled++;
        if (mmapRet) {
            return 0;
        }
        if (addr != MAP_FAILED && addr != 0) {
            if (*static_cast<char *>(addr) != static_cast<char>(0xaa)) {
                memoryModifiedInMunmap = true;
            }
            delete[](char *) addr;
        } else {
            return -1;
        }
        return 0;
    }

    void setPreadMemory(char *memory, size_t size, uint64_t baseAddress) {
        pReadArrayRef = ArrayRef<char>(memory, size);
        pReadBase = baseAddress;
    }

    void setPwriteMemory(char *memory, size_t size, uint64_t baseAddress) {
        pWriteArrayRef = ArrayRef<char>(memory, size);
        pWriteBase = baseAddress;
    }

    prelim_drm_i915_debug_event debugEventInput = {};
    prelim_drm_i915_debug_event debugEventAcked = {};
    prelim_drm_i915_debug_read_uuid *returnUuid = nullptr;
    prelim_drm_i915_debug_vm_open vmOpen = {};
    prelim_drm_i915_debug_eu_control euControl = {};

    std::unique_ptr<uint8_t[]> euControlBitmask;
    size_t euControlBitmaskSize = 0;

    int ioctlRetVal = 0;
    int debugEventRetVal = 0;
    int ioctlCalled = 0;
    int pollRetVal = 0;
    int vmOpenRetVal = 600;
    int passedTimeout = 0;

    ArrayRef<char> pReadArrayRef;
    uint64_t pReadBase = 0;
    int64_t preadCalled = 0;
    int64_t preadRetVal = 0;

    ArrayRef<char> pWriteArrayRef;
    uint64_t pWriteBase = 0;
    int64_t pwriteCalled = 0;
    int64_t pwriteRetVal = 0;

    uint64_t preadOffset = 0;
    uint64_t pwriteOffset = 0;

    uint8_t midZeroReturn = 0;
    int64_t mmapCalled = 0;
    int64_t munmapCalled = 0;
    bool mmapFail = false;
    char *mmapRet = nullptr;
    uint64_t mmapBase = 0;
    bool memoryModifiedInMunmap = false;
    std::atomic<int> pollCounter = 0;
    EventQueue eventQueue;
    uint64_t euControlOutputSeqno = 10;
    uint32_t ackCount = 0;
};

struct MockDebugSessionLinux : public L0::DebugSessionLinux {
    using L0::DebugSessionImp::allThreads;
    using L0::DebugSessionImp::apiEvents;
    using L0::DebugSessionImp::attachTile;
    using L0::DebugSessionImp::detachTile;
    using L0::DebugSessionImp::enqueueApiEvent;
    using L0::DebugSessionImp::expectedAttentionEvents;
    using L0::DebugSessionImp::interruptSent;
    using L0::DebugSessionImp::isValidGpuAddress;
    using L0::DebugSessionImp::newAttentionRaised;
    using L0::DebugSessionImp::stateSaveAreaHeader;
    using L0::DebugSessionImp::tileAttachEnabled;
    using L0::DebugSessionImp::tileSessions;
    using L0::DebugSessionImp::tileSessionsEnabled;
    using L0::DebugSessionImp::triggerEvents;

    using L0::DebugSessionLinux::asyncThread;
    using L0::DebugSessionLinux::checkAllEventsCollected;
    using L0::DebugSessionLinux::clientHandle;
    using L0::DebugSessionLinux::clientHandleClosed;
    using L0::DebugSessionLinux::clientHandleToConnection;
    using L0::DebugSessionLinux::closeAsyncThread;
    using L0::DebugSessionLinux::createTileSessionsIfEnabled;
    using L0::DebugSessionLinux::debugArea;
    using L0::DebugSessionLinux::euControlInterruptSeqno;
    using L0::DebugSessionLinux::eventsToAck;
    using L0::DebugSessionLinux::extractVaFromUuidString;
    using L0::DebugSessionLinux::fd;
    using L0::DebugSessionLinux::getIsaInfoForAllInstances;
    using L0::DebugSessionLinux::getISAVMHandle;
    using L0::DebugSessionLinux::getRegisterSetProperties;
    using L0::DebugSessionLinux::getSbaBufferGpuVa;
    using L0::DebugSessionLinux::getStateSaveAreaHeader;
    using L0::DebugSessionLinux::handleEvent;
    using L0::DebugSessionLinux::handleEventsAsync;
    using L0::DebugSessionLinux::handleVmBindEvent;
    using L0::DebugSessionLinux::internalEventQueue;
    using L0::DebugSessionLinux::interruptImp;
    using L0::DebugSessionLinux::ioctl;
    using L0::DebugSessionLinux::ioctlHandler;
    using L0::DebugSessionLinux::newlyStoppedThreads;
    using L0::DebugSessionLinux::pendingInterrupts;
    using L0::DebugSessionLinux::pendingVmBindEvents;
    using L0::DebugSessionLinux::printContextVms;
    using L0::DebugSessionLinux::pushApiEvent;
    using L0::DebugSessionLinux::readEventImp;
    using L0::DebugSessionLinux::readGpuMemory;
    using L0::DebugSessionLinux::readInternalEventsAsync;
    using L0::DebugSessionLinux::readModuleDebugArea;
    using L0::DebugSessionLinux::readSbaBuffer;
    using L0::DebugSessionLinux::readStateSaveAreaHeader;
    using L0::DebugSessionLinux::startAsyncThread;
    using L0::DebugSessionLinux::threadControl;
    using L0::DebugSessionLinux::ThreadControlCmd;
    using L0::DebugSessionLinux::typeToRegsetDesc;
    using L0::DebugSessionLinux::typeToRegsetFlags;
    using L0::DebugSessionLinux::uuidL0CommandQueueHandleToDevice;
    using L0::DebugSessionLinux::writeGpuMemory;

    MockDebugSessionLinux(const zet_debug_config_t &config, L0::Device *device, int debugFd) : DebugSessionLinux(config, device, debugFd) {
        clientHandleToConnection[mockClientHandle].reset(new ClientConnection);
        clientHandle = mockClientHandle;
        createEuThreads();
    }

    ze_result_t initialize() override {
        if (initializeRetVal != ZE_RESULT_FORCE_UINT32) {
            bool isRootDevice = !connectedDevice->getNEODevice()->isSubDevice();
            if (isRootDevice && !tileAttachEnabled) {
                createEuThreads();
            }
            createTileSessionsIfEnabled();

            clientHandle = mockClientHandle;
            return initializeRetVal;
        }
        return DebugSessionLinux::initialize();
    }

    std::unordered_map<uint64_t, std::pair<std::string, uint32_t>> &getClassHandleToIndex() {
        return clientHandleToConnection[mockClientHandle]->classHandleToIndex;
    }

    int64_t getTimeDifferenceMilliseconds(std::chrono::high_resolution_clock::time_point time) override {
        if (returnTimeDiff != -1) {
            return returnTimeDiff;
        }
        return L0::DebugSessionLinux::getTimeDifferenceMilliseconds(time);
    }

    int threadControl(const std::vector<EuThread::ThreadId> &threads, uint32_t tile, ThreadControlCmd threadCmd, std::unique_ptr<uint8_t[]> &bitmask, size_t &bitmaskSize) override {
        numThreadsPassedToThreadControl = threads.size();
        return L0::DebugSessionLinux::threadControl(threads, tile, threadCmd, bitmask, bitmaskSize);
    }

    ze_result_t readRegisters(ze_device_thread_t thread, uint32_t type, uint32_t start, uint32_t count, void *pRegisterValues) override {
        readRegistersCallCount++;
        return L0::DebugSessionLinux::readRegisters(thread, type, start, count, pRegisterValues);
    }

    ze_result_t writeRegisters(ze_device_thread_t thread, uint32_t type, uint32_t start, uint32_t count, void *pRegisterValues) override {
        writeRegistersCallCount++;
        writeRegistersReg = type;
        return L0::DebugSessionLinux::writeRegisters(thread, type, start, count, pRegisterValues);
    }

    ze_result_t resumeImp(const std::vector<EuThread::ThreadId> &threads, uint32_t deviceIndex) override {
        resumedThreads.push_back(threads);
        resumedDevices.push_back(deviceIndex);
        return L0::DebugSessionLinux::resumeImp(threads, deviceIndex);
    }

    ze_result_t interruptImp(uint32_t deviceIndex) override {
        interruptedDevice = deviceIndex;
        return L0::DebugSessionLinux::interruptImp(deviceIndex);
    }

    void handleEvent(prelim_drm_i915_debug_event *event) override {
        handleEventCalledCount++;
        L0::DebugSessionLinux::handleEvent(event);
    }

    bool areRequestedThreadsStopped(ze_device_thread_t thread) override {
        if (allThreadsStopped) {
            return allThreadsStopped;
        }
        return L0::DebugSessionLinux::areRequestedThreadsStopped(thread);
    }

    void ensureThreadStopped(ze_device_thread_t thread) {
        auto threadId = convertToThreadId(thread);
        if (allThreads.find(threadId) == allThreads.end()) {
            allThreads[threadId] = std::make_unique<EuThread>(threadId);
        }
        allThreads[threadId]->stopThread(vmHandle);
    }

    bool readSystemRoutineIdent(EuThread *thread, uint64_t vmHandle, SIP::sr_ident &srIdent) override {
        srIdent.count = 0;
        if (stoppedThreads.size()) {
            auto entry = stoppedThreads.find(thread->getThreadId());
            if (entry != stoppedThreads.end()) {
                srIdent.count = entry->second;
            }
            return true;
        }
        return L0::DebugSessionImp::readSystemRoutineIdent(thread, vmHandle, srIdent);
    }

    bool writeResumeCommand(const std::vector<EuThread::ThreadId> &threadIds) override {
        writeResumeCommandCalled++;
        if (skipWriteResumeCommand) {
            return true;
        }
        return L0::DebugSessionLinux::writeResumeCommand(threadIds);
    }

    bool checkThreadIsResumed(const EuThread::ThreadId &threadID) override {
        checkThreadIsResumedCalled++;
        if (skipcheckThreadIsResumed) {
            return true;
        }
        return L0::DebugSessionLinux::checkThreadIsResumed(threadID);
    }

    void startInternalEventsThread() override {
        if (synchronousInternalEventRead) {
            internalThreadHasStarted = true;
            return;
        }
        return DebugSessionLinux::startInternalEventsThread();
    }

    std::unique_ptr<uint64_t[]> getInternalEvent() override {
        getInternalEventCounter++;
        if (synchronousInternalEventRead) {
            readInternalEventsAsync();
        }
        return DebugSessionLinux::getInternalEvent();
    }

    void processPendingVmBindEvents() override {
        processPendingVmBindEventsCalled++;
        return DebugSessionLinux::processPendingVmBindEvents();
    }

    TileDebugSessionLinux *createTileSession(const zet_debug_config_t &config, L0::Device *device, L0::DebugSessionImp *rootDebugSession) override;

    ze_result_t initializeRetVal = ZE_RESULT_FORCE_UINT32;
    bool allThreadsStopped = false;
    int64_t returnTimeDiff = -1;
    static constexpr uint64_t mockClientHandle = 1;
    size_t numThreadsPassedToThreadControl = 0;
    uint32_t readRegistersCallCount = 0;
    uint32_t writeRegistersCallCount = 0;
    uint32_t writeRegistersReg = 0;
    uint32_t handleEventCalledCount = 0;
    uint64_t vmHandle = UINT64_MAX;
    bool skipWriteResumeCommand = true;
    uint32_t writeResumeCommandCalled = 0;
    bool skipcheckThreadIsResumed = true;
    uint32_t checkThreadIsResumedCalled = 0;
    uint32_t interruptedDevice = std::numeric_limits<uint32_t>::max();
    uint32_t processPendingVmBindEventsCalled = 0;

    std::vector<uint32_t> resumedDevices;
    std::vector<std::vector<EuThread::ThreadId>> resumedThreads;

    std::unordered_map<uint64_t, uint8_t> stoppedThreads;

    std::atomic<int> getInternalEventCounter = 0;
    bool synchronousInternalEventRead = false;
};

struct MockTileDebugSessionLinux : TileDebugSessionLinux {
    using DebugSession::allThreads;
    using DebugSessionImp::apiEvents;
    using DebugSessionImp::checkTriggerEventsForAttention;
    using DebugSessionImp::expectedAttentionEvents;
    using DebugSessionImp::interruptImp;
    using DebugSessionImp::newlyStoppedThreads;
    using DebugSessionImp::resumeImp;
    using DebugSessionImp::sendInterrupts;
    using DebugSessionImp::stateSaveAreaHeader;
    using DebugSessionImp::triggerEvents;
    using DebugSessionLinux::detached;
    using DebugSessionLinux::pushApiEvent;
    using TileDebugSessionLinux::cleanRootSessionAfterDetach;
    using TileDebugSessionLinux::getAllMemoryHandles;
    using TileDebugSessionLinux::getContextStateSaveAreaGpuVa;
    using TileDebugSessionLinux::getSbaBufferGpuVa;
    using TileDebugSessionLinux::isAttached;
    using TileDebugSessionLinux::modules;
    using TileDebugSessionLinux::processEntryState;
    using TileDebugSessionLinux::readGpuMemory;
    using TileDebugSessionLinux::readModuleDebugArea;
    using TileDebugSessionLinux::readSbaBuffer;
    using TileDebugSessionLinux::readStateSaveAreaHeader;
    using TileDebugSessionLinux::startAsyncThread;
    using TileDebugSessionLinux::TileDebugSessionLinux;
    using TileDebugSessionLinux::writeGpuMemory;

    void ensureThreadStopped(ze_device_thread_t thread, uint64_t vmHandle) {
        auto threadId = convertToThreadId(thread);
        if (allThreads.find(threadId) == allThreads.end()) {
            allThreads[threadId] = std::make_unique<EuThread>(threadId);
        }
        allThreads[threadId]->stopThread(vmHandle);
    }

    bool writeResumeCommand(const std::vector<EuThread::ThreadId> &threadIds) override {
        if (writeResumeResult != -1) {
            return writeResumeResult == 0 ? false : true;
        }
        return writeResumeCommand(threadIds);
    }

    bool readSystemRoutineIdent(EuThread *thread, uint64_t vmHandle, SIP::sr_ident &srIdent) override {
        srIdent.count = 0;
        if (stoppedThreads.size()) {
            auto entry = stoppedThreads.find(thread->getThreadId());
            if (entry != stoppedThreads.end()) {
                srIdent.count = entry->second;
            }
            return true;
        }
        return L0::DebugSessionLinux::readSystemRoutineIdent(thread, vmHandle, srIdent);
    }

    int64_t getTimeDifferenceMilliseconds(std::chrono::high_resolution_clock::time_point time) override {
        if (returnTimeDiff != -1) {
            return returnTimeDiff;
        }
        return L0::DebugSessionLinux::getTimeDifferenceMilliseconds(time);
    }

    int writeResumeResult = -1;
    int64_t returnTimeDiff = -1;
    std::unordered_map<uint64_t, uint8_t> stoppedThreads;
};

struct DebugApiLinuxFixture : public DeviceFixture {
    void setUp() {
        setUp(nullptr);
    }

    void setUp(NEO::HardwareInfo *hwInfo);

    void tearDown() {
        DeviceFixture::tearDown();
    }
    DrmQueryMock *mockDrm = nullptr;
    static constexpr uint8_t bufferSize = 16;
};

struct DebugApiLinuxMultiDeviceFixture : public MultipleDevicesWithCustomHwInfo {
    void setUp();

    void tearDown() {
        MultipleDevicesWithCustomHwInfo::tearDown();
    }
    NEO::Device *neoDevice = nullptr;
    L0::DeviceImp *deviceImp = nullptr;
    DrmQueryMock *mockDrm = nullptr;
    static constexpr uint8_t bufferSize = 16;
};

struct MockDebugSessionLinuxHelper {

    void setupSessionClassHandlesAndUuidMap(MockDebugSessionLinux *session) {
        session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->classHandleToIndex[sbaClassHandle] = {"SBA AREA", static_cast<uint32_t>(NEO::DrmResourceClass::SbaTrackingBuffer)};
        session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->classHandleToIndex[moduleDebugClassHandle] = {"DEBUG AREA", static_cast<uint32_t>(NEO::DrmResourceClass::ModuleHeapDebugArea)};
        session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->classHandleToIndex[contextSaveClassHandle] = {"CONTEXT SAVE AREA", static_cast<uint32_t>(NEO::DrmResourceClass::ContextSaveArea)};
        session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->classHandleToIndex[isaClassHandle] = {"ISA", static_cast<uint32_t>(NEO::DrmResourceClass::Isa)};
        session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->classHandleToIndex[elfClassHandle] = {"ELF", static_cast<uint32_t>(NEO::DrmResourceClass::Elf)};

        DebugSessionLinux::UuidData isaUuidData = {
            .handle = isaUUID,
            .classHandle = isaClassHandle,
            .classIndex = NEO::DrmResourceClass::Isa};
        DebugSessionLinux::UuidData elfUuidData = {
            .handle = elfUUID,
            .classHandle = elfClassHandle,
            .classIndex = NEO::DrmResourceClass::Elf,
            .data = std::make_unique<char[]>(elfSize),
            .dataSize = elfSize};
        memcpy(elfUuidData.data.get(), "ELF", sizeof("ELF"));

        DebugSessionLinux::UuidData cookieUuidData = {
            .handle = cookieUUID,
            .classHandle = isaUUID,
            .classIndex = NEO::DrmResourceClass::MaxSize,
        };
        DebugSessionLinux::UuidData sbaUuidData = {
            .handle = sbaUUID,
            .classHandle = sbaClassHandle,
            .classIndex = NEO::DrmResourceClass::SbaTrackingBuffer,
            .data = nullptr,
            .dataSize = 0};

        DebugSessionLinux::UuidData debugAreaUuidData = {
            .handle = debugAreaUUID,
            .classHandle = moduleDebugClassHandle,
            .classIndex = NEO::DrmResourceClass::ModuleHeapDebugArea,
            .data = nullptr,
            .dataSize = 0};

        DebugSessionLinux::UuidData stateSaveUuidData = {
            .handle = stateSaveUUID,
            .classHandle = contextSaveClassHandle,
            .classIndex = NEO::DrmResourceClass::ContextSaveArea,
            .data = nullptr,
            .dataSize = 0};

        session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap.emplace(isaUUID, std::move(isaUuidData));
        session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap.emplace(elfUUID, std::move(elfUuidData));
        session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap.emplace(cookieUUID, std::move(cookieUuidData));
        session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap.emplace(sbaUUID, std::move(sbaUuidData));
        session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap.emplace(debugAreaUUID, std::move(debugAreaUuidData));
        session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap.emplace(stateSaveUUID, std::move(stateSaveUuidData));

        DebugSessionLinux::UuidData zebinModuleUuidData = {
            .handle = zebinModuleUUID,
            .classHandle = zebinModuleClassHandle,
            .classIndex = NEO::DrmResourceClass::L0ZebinModule,
            .data = std::make_unique<char[]>(sizeof(kernelCount)),
            .dataSize = sizeof(kernelCount)};

        memcpy(zebinModuleUuidData.data.get(), &kernelCount, sizeof(kernelCount));
        session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->classHandleToIndex[zebinModuleClassHandle] = {"L0_ZEBIN_MODULE", static_cast<uint32_t>(NEO::DrmResourceClass::L0ZebinModule)};
        session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap.emplace(zebinModuleUUID, std::move(zebinModuleUuidData));
        session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidToModule[zebinModuleUUID].segmentCount = 2;

        session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->uuidMap[elfUUID].ptr = elfVa;
    }

    void setupVmToTile(MockDebugSessionLinux *session) {
        session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->vmToTile[vm0] = 0;
        session->clientHandleToConnection[MockDebugSessionLinux::mockClientHandle]->vmToTile[vm1] = 1;
    }

    void addIsaVmBindEvent(MockDebugSessionLinux *session, uint64_t vm, bool ack, bool create) {
        uint64_t vmBindIsaData[(sizeof(prelim_drm_i915_debug_event_vm_bind) + 3 * sizeof(typeOfUUID) + sizeof(uint64_t)) / sizeof(uint64_t)];
        prelim_drm_i915_debug_event_vm_bind *vmBindIsa = reinterpret_cast<prelim_drm_i915_debug_event_vm_bind *>(&vmBindIsaData);

        vmBindIsa->base.type = PRELIM_DRM_I915_DEBUG_EVENT_VM_BIND;
        if (create) {
            vmBindIsa->base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
        } else {
            vmBindIsa->base.flags = PRELIM_DRM_I915_DEBUG_EVENT_DESTROY;
        }

        if (ack) {
            vmBindIsa->base.flags |= PRELIM_DRM_I915_DEBUG_EVENT_NEED_ACK;
        }
        vmBindIsa->base.size = sizeof(prelim_drm_i915_debug_event_vm_bind) + 3 * sizeof(typeOfUUID);
        vmBindIsa->base.seqno = 20u;
        vmBindIsa->client_handle = MockDebugSessionLinux::mockClientHandle;
        vmBindIsa->va_start = isaGpuVa;
        vmBindIsa->va_length = isaSize;
        vmBindIsa->vm_handle = vm;
        vmBindIsa->num_uuids = 3;

        auto *uuids = reinterpret_cast<typeOfUUID *>(ptrOffset(vmBindIsaData, sizeof(prelim_drm_i915_debug_event_vm_bind)));

        typeOfUUID uuidsTemp[3];
        uuidsTemp[0] = static_cast<typeOfUUID>(isaUUID);
        uuidsTemp[1] = static_cast<typeOfUUID>(cookieUUID);
        uuidsTemp[2] = static_cast<typeOfUUID>(elfUUID);

        memcpy(uuids, uuidsTemp, sizeof(uuidsTemp));

        session->handleEvent(&vmBindIsa->base);
    }

    void addZebinVmBindEvent(MockDebugSessionLinux *session, uint64_t vm, bool ack, bool create, uint64_t kernelIndex) {
        uint64_t vmBindIsaData[(sizeof(prelim_drm_i915_debug_event_vm_bind) + 4 * sizeof(typeOfUUID) + sizeof(uint64_t)) / sizeof(uint64_t)];
        prelim_drm_i915_debug_event_vm_bind *vmBindIsa = reinterpret_cast<prelim_drm_i915_debug_event_vm_bind *>(&vmBindIsaData);

        vmBindIsa->base.type = PRELIM_DRM_I915_DEBUG_EVENT_VM_BIND;
        if (create) {
            vmBindIsa->base.flags = PRELIM_DRM_I915_DEBUG_EVENT_CREATE;
        } else {
            vmBindIsa->base.flags = PRELIM_DRM_I915_DEBUG_EVENT_DESTROY;
        }

        if (ack) {
            vmBindIsa->base.flags |= PRELIM_DRM_I915_DEBUG_EVENT_NEED_ACK;
        }

        vmBindIsa->base.size = sizeof(prelim_drm_i915_debug_event_vm_bind) + 4 * sizeof(typeOfUUID);
        vmBindIsa->base.seqno = 10;
        vmBindIsa->client_handle = MockDebugSessionLinux::mockClientHandle;
        vmBindIsa->va_start = kernelIndex == 0 ? isaGpuVa : isaGpuVa + isaSize;
        vmBindIsa->va_length = isaSize;
        vmBindIsa->vm_handle = vm;
        vmBindIsa->num_uuids = 4;
        auto *uuids = reinterpret_cast<typeOfUUID *>(ptrOffset(vmBindIsaData, sizeof(prelim_drm_i915_debug_event_vm_bind)));
        typeOfUUID uuidsTemp[4];
        uuidsTemp[0] = static_cast<typeOfUUID>(isaUUID);
        uuidsTemp[1] = static_cast<typeOfUUID>(cookieUUID);
        uuidsTemp[2] = static_cast<typeOfUUID>(elfUUID);
        uuidsTemp[3] = static_cast<typeOfUUID>(zebinModuleUUID);

        memcpy_s(uuids, 4 * sizeof(typeOfUUID), uuidsTemp, sizeof(uuidsTemp));
        session->handleEvent(&vmBindIsa->base);
    }

    const uint64_t sbaClassHandle = 1;
    const uint64_t moduleDebugClassHandle = 2;
    const uint64_t contextSaveClassHandle = 3;
    const uint64_t isaClassHandle = 4;
    const uint64_t elfClassHandle = 5;

    const uint64_t isaUUID = 2;
    const uint64_t elfUUID = 3;
    const uint64_t cookieUUID = 5;
    const uint64_t sbaUUID = 6;
    const uint64_t debugAreaUUID = 7;
    const uint64_t stateSaveUUID = 8;

    const uint64_t zebinModuleClassHandle = 101;
    const uint64_t zebinModuleUUID = 9;
    const uint32_t kernelCount = 2;

    const uint64_t vm0 = 1;
    const uint64_t vm1 = 2;

    const uint64_t isaGpuVa = 0x345000;
    const uint64_t isaSize = 0x2000;

    const uint64_t elfVa = 0x1000;
    const uint32_t elfSize = 10;
};

} // namespace ult
} // namespace L0
