/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <stdint.h>

namespace L0 {

namespace KmdSysman {
constexpr uint32_t KmdMaxBufferSize = 2048;
constexpr uint32_t MaxPropertyBufferSize = 128;
constexpr uint32_t PcEscapeOperation = 35;
constexpr uint32_t KmdSysmanSuccess = 0;
constexpr uint32_t KmdSysmanFail = 1;
constexpr uint32_t KmdMajorVersion = 1;
constexpr uint32_t KmdMinorVersion = 0;
constexpr uint32_t KmdPatchNumber = 0;

struct GfxSysmanMainHeaderIn {
    uint32_t inVersion;
    uint32_t inNumElements;
    uint32_t inTotalsize;
    uint8_t inBuffer[KmdMaxBufferSize];
};

struct GfxSysmanMainHeaderOut {
    uint32_t outStatus;
    uint32_t outNumElements;
    uint32_t outTotalSize;
    uint8_t outBuffer[KmdMaxBufferSize];
};

struct GfxSysmanReqHeaderIn {
    uint32_t inRequestId;
    uint32_t inCommand;
    uint32_t inComponent;
    uint32_t inCommandParam;
    uint32_t inDataSize;
};

struct GfxSysmanReqHeaderOut {
    uint32_t outRequestId;
    uint32_t outComponent;
    uint32_t outReturnCode;
    uint32_t outDataSize;
};

enum Command {
    Get = 0,
    Set,
    RegisterEvent,
    UnregisterEvent,

    MaxCommands,
};

enum Events {
    EnergyThresholdCrossed = 0,
    EnterD0,
    EnterD3,
    EnterTDR,
    ExitTDR,
    FrequencyThrottled,
    CriticalTemperature,
    TemperatureThreshold1,
    TemperatureThreshold2,
    ResetDeviceRequired,

    MaxEvents,
};

enum Component {
    InterfaceProperties = 0,
    PowerComponent,
    FrequencyComponent,
    ActivityComponent,
    FanComponent,
    TemperatureComponent,
    FpsComponent,
    SchedulerComponent,
    MemoryComponent,
    PciComponent,
    GlobalOperationsComponent,

    MaxComponents,
};

namespace Requests {

enum Interface {
    InterfaceVersion = 0,

    MaxInterfaceRequests,
};

enum Power {
    NumPowerDomains = 0,

    // support / enabled
    EnergyThresholdSupported,
    EnergyThresholdEnabled,
    PowerLimit1Enabled,
    PowerLimit2Enabled,

    // default fused values
    PowerLimit1Default,
    PowerLimit2Default,
    PowerLimit1TauDefault,
    PowerLimit4AcDefault,
    PowerLimit4DcDefault,
    EnergyThresholdDefault,
    TdpDefault,
    MinPowerLimitDefault,
    MaxPowerLimitDefault,
    EnergyCounterUnits,

    // current runtime values
    CurrentPowerLimit1,
    CurrentPowerLimit2,
    CurrentPowerLimit1Tau,
    CurrentPowerLimit4Ac,
    CurrentPowerLimit4Dc,
    CurrentEnergyThreshold,
    DisableEnergyThreshold,
    CurrentEnergyCounter,

    MaxPowerRequests,
};

enum Activity {
    NumActivityDomains = 0,

    // default fused values
    ActivityCounterNumberOfBits,
    ActivityCounterFrequency,
    TimestampFrequency,

    // current runtime values
    CurrentActivityCounter,

    MaxActivityRequests,
};

enum Temperature {
    NumTemperatureDomains = 0,

    // support / enabled
    TempCriticalEventSupported,
    TempThreshold1EventSupported,
    TempThreshold2EventSupported,
    TempCriticalEventEnabled,
    TempThreshold1EventEnabled,
    TempThreshold2EventEnabled,

    // default fused values
    MaxTempSupported,

    // current runtime values
    CurrentTemperature,

    MaxTemperatureRequests,
};

enum Frequency {
    NumFrequencyDomains = 0,

    // support / enabled
    FrequencyOcSupported,
    VoltageOverrideSupported,
    VoltageOffsetSupported,
    HighVoltageModeSupported,
    ExtendedOcSupported,
    FixedModeSupported,
    HighVoltageEnabled,
    CanControlFrequency,
    FrequencyThrottledEventSupported,

    // default fused values
    TjMaxDefault,
    IccMaxDefault,
    MaxOcFrequencyDefault,
    MaxNonOcFrequencyDefault,
    MaxOcVoltageDefault,
    MaxNonOcVoltageDefault,
    FrequencyRangeMinDefault,
    FrequencyRangeMaxDefault,

    // current runtime values
    CurrentFrequencyTarget,
    CurrentVoltageTarget,
    CurrentVoltageOffset,
    CurrentVoltageMode,
    CurrentFixedMode,
    CurrentTjMax,
    CurrentIccMax,
    CurrentVoltage,
    CurrentRequestedFrequency,
    CurrentTdpFrequency,
    CurrentEfficientFrequency,
    CurrentResolvedFrequency,
    CurrentThrottleReasons,
    CurrentThrottleTime,
    CurrentFrequencyRange,

    MaxFrequencyRequests,
};

enum Fans {
    NumFanDomains = 0,

    // default fused values
    MaxFanControlPointsSupported,
    MaxFanSpeedSupported,

    // current runtime values
    CurrentNumOfControlPoints,
    CurrentFanPoint,
    CurrentFanSpeed,

    MaxFanRequests,
};

enum Fps {
    NumFpsDomains = 0,
    IsDisplayAttached,
    InstRenderTime,
    TimeToFlip,
    AvgFps,
    AvgRenderTime,
    AvgInstFps,

    MaxFpsRequests,
};

enum Scheduler {
    NumSchedulerDomains = 0,

    MaxSchedulerRequests,
};

enum Memory {
    NumMemoryDomains = 0,

    // default fused values
    MemoryType,
    MemoryLocation,
    PhysicalSize,
    StolenSize,
    SystemSize,
    DedicatedSize,
    MemoryWidth,
    NumChannels,
    MaxBandwidth,

    // current runtime values
    CurrentBandwidthRead,
    CurrentBandwidthWrite,
    CurrentFreeMemorySize,
    CurrentTotalAllocableMem,

    MaxMemoryRequests
};

enum Pci {
    NumPciDomains = 0,

    // support / enabled
    BandwidthCountersSupported,
    PacketCountersSupported,
    ReplayCountersSupported,

    // default fused values
    DeviceId,
    VendorId,
    Domain,
    Bus,
    Device,
    Function,
    Gen,
    DevType,
    MaxLinkWidth,
    MaxLinkSpeed,
    BusInterface,
    BusWidth,
    BarType,
    BarIndex,
    BarBase,
    BarSize,

    // current runtime values
    CurrentLinkWidth,
    CurrentLinkSpeed,
    CurrentLinkStatus,
    CurrentLinkQualityFlags,
    CurrentLinkStabilityFlags,
    CurrentLinkReplayCounter,
    CurrentLinkPacketCounter,
    CurrentLinkRxCounter,
    CurrentLinkTxCounter,

    // resizable bar
    ResizableBarSupported,
    ResizableBarEnabled,

    MaxPciRequests,
};

enum GlobalOperation {
    NumGlobalOperationDomains = 0,

    TriggerDeviceLevelReset
};

} // namespace Requests

enum FlipType {
    MMIOFlip = 0,
    MMIOAsyncFlip,
    DMAFlip,
    DMAAsyncFlip,

    MaxFlipTypes,
};

enum GeneralDomainsType {
    GeneralDomainDGPU = 0,
    GeneralDomainHBM,

    GeneralDomainMaxTypes,
};

enum TemperatureDomainsType {
    TemperatureDomainPackage = 0,
    TemperatureDomainDGPU,
    TemperatureDomainHBM,

    TempetatureMaxDomainTypes,
};

enum ActivityDomainsType {
    ActitvityDomainGT = 0,
    ActivityDomainRenderCompute,
    ActivityDomainMedia,

    ActivityDomainMaxTypes,
};

enum PciDomainsType {
    PciCurrentDevice = 0,
    PciParentDevice,
    PciRootPort,

    PciDomainMaxTypes,
};

enum MemoryType {
    DDR4 = 0,
    DDR5,
    LPDDR5,
    LPDDR4,
    DDR3,
    LPDDR3,
    GDDR4,
    GDDR5,
    GDDR5X,
    GDDR6,
    GDDR6X,
    GDDR7,
    UknownMemType,

    MaxMemoryTypes,
};

enum MemoryWidthType {
    MemWidth8x = 0,
    MemWidth16x,
    MemWidth32x,

    UnknownMemWidth,

    MaxMemoryWidthTypes,
};

enum MemoryLocationsType {
    SystemMemory = 0,
    DeviceMemory,

    UnknownMemoryLocation,

    MaxMemoryLocationTypes,
};

enum PciGensType {
    PciGen1_1 = 0,
    PciGen2_0,
    PciGen3_0,
    PciGen4_0,

    UnknownPciGen,

    MaxPciGenTypes,
};

enum PciLinkSpeedType {
    UnknownPciLinkSpeed = 0,
    PciLinkSpeed2_5 = 1,
    PciLinkSpeed5_0,
    PciLinkSpeed8_0,
    PciLinkSpeed16_0,

    MaxPciLinkSpeedTypes,
};

enum ReturnCodes {
    Success = 0,

    PcuError,
    IllegalCommand,
    TimeOut,
    IllegalData,
    IllegalSubCommand,
    OverclockingLocked,
    DomainServiceNotSupported,
    FrequencyExceedsMax,
    VoltageExceedsMax,
    OverclockingNotSupported,
    InvalidVr,
    InvalidIccMax,
    VoltageOverrideDisabled,
    ServiceNotAvailable,
    InvalidRequestType,
    InvalidComponent,
    BufferNotLargeEnough,
    GetNotSupported,
    SetNotSupported,
    MissingProperties,
    InvalidEvent,
    CreateEventError,
    ErrorVersion,
    ErrorSize,
    ErrorNoElements,
    ErrorBufferCorrupted,
    VTNotSupported,
    NotInitialized,
    PropertyNotSet,
    InvalidFlipType,
};

enum PciLinkWidthType {
    PciLinkWidth1x = 0,
    PciLinkWidth2x,
    PciLinkWidth4x,
    PciLinkWidth8x,
    PciLinkWidth12x,
    PciLinkWidth16x,
    PciLinkWidth32x,

    UnknownPciLinkWidth,

    MaxPciLinkWidthTypes,
};

struct KmdSysmanVersion {
    KmdSysmanVersion() : data(0) {}
    union {
        struct {
            uint32_t reservedBits : 8;
            uint32_t majorVersion : 8;
            uint32_t minorVersion : 8;
            uint32_t patchNumber : 8;
        };
        uint32_t data;
    };
};

struct RequestProperty {
    RequestProperty() : requestId(0), commandId(0), componentId(0), paramInfo(0), dataSize(0) {}
    RequestProperty(const RequestProperty &other) {
        requestId = other.requestId;
        commandId = other.commandId;
        componentId = other.componentId;
        paramInfo = other.paramInfo;
        dataSize = other.dataSize;
        if (other.dataSize > 0 && other.dataSize < MaxPropertyBufferSize && other.dataBuffer) {
            memcpy_s(dataBuffer, other.dataSize, other.dataBuffer, other.dataSize);
        }
    }
    RequestProperty(uint32_t _requestId,
                    uint32_t _commandId,
                    uint32_t _componentId,
                    uint32_t _paramInfo,
                    uint32_t _dataSize,
                    uint8_t *_dataBuffer) {
        requestId = _requestId;
        commandId = _commandId;
        componentId = _componentId;
        paramInfo = _paramInfo;
        dataSize = _dataSize;
        if (dataSize > 0 && dataSize < MaxPropertyBufferSize && _dataBuffer) {
            memcpy_s(dataBuffer, dataSize, _dataBuffer, dataSize);
        }
    }
    RequestProperty &operator=(const RequestProperty &other) {
        requestId = other.requestId;
        commandId = other.commandId;
        componentId = other.componentId;
        paramInfo = other.paramInfo;
        dataSize = other.dataSize;
        if (other.dataSize > 0 && other.dataSize < MaxPropertyBufferSize && other.dataBuffer) {
            memcpy_s(dataBuffer, other.dataSize, other.dataBuffer, other.dataSize);
        }
        return *this;
    }
    uint32_t requestId;
    uint32_t commandId;
    uint32_t componentId;
    uint32_t paramInfo;
    uint32_t dataSize;
    uint8_t dataBuffer[MaxPropertyBufferSize] = {0};
};

struct ResponseProperty {
    ResponseProperty() : requestId(0), returnCode(0), componentId(0), dataSize(0) {}
    ResponseProperty(const ResponseProperty &other) {
        requestId = other.requestId;
        returnCode = other.returnCode;
        componentId = other.componentId;
        dataSize = other.dataSize;
        if (other.dataSize > 0 && other.dataSize < MaxPropertyBufferSize && other.dataBuffer) {
            memcpy_s(dataBuffer, other.dataSize, other.dataBuffer, other.dataSize);
        }
    }
    ResponseProperty(uint32_t _requestId,
                     uint32_t _returnCode,
                     uint32_t _componentId,
                     uint32_t _dataSize,
                     uint8_t *_dataBuffer) {
        requestId = _requestId;
        returnCode = _returnCode;
        componentId = _componentId;
        dataSize = _dataSize;
        if (dataSize > 0 && dataSize < MaxPropertyBufferSize && _dataBuffer) {
            memcpy_s(dataBuffer, dataSize, _dataBuffer, dataSize);
        }
    }
    ResponseProperty &operator=(const ResponseProperty &other) {
        this->requestId = other.requestId;
        this->returnCode = other.returnCode;
        this->componentId = other.componentId;
        this->dataSize = other.dataSize;
        if (other.dataSize > 0 && other.dataSize < MaxPropertyBufferSize && other.dataBuffer) {
            memcpy_s(this->dataBuffer, other.dataSize, other.dataBuffer, other.dataSize);
        }
        return *this;
    }

    uint32_t requestId;
    uint32_t returnCode;
    uint32_t componentId;
    uint32_t dataSize;
    uint8_t dataBuffer[MaxPropertyBufferSize] = {0};
};
} // namespace KmdSysman
} // namespace L0
