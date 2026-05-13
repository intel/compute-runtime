/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/events/linux/sysman_os_events_netlink.h"

#include "shared/source/debug_settings/debug_settings_manager.h"

#include "level_zero/sysman/source/api/events/linux/sysman_os_events_imp.h"
#include "level_zero/sysman/source/api/ras/linux/ras_util/sysman_ras_util.h"
#include "level_zero/sysman/source/driver/sysman_driver_handle_imp.h"
#include "level_zero/sysman/source/shared/linux/nl_api/sysman_drm_nl_api.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_driver_imp.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_imp.h"

namespace L0 {
namespace Sysman {

ze_result_t netlinkInitialize(LinuxSysmanDriverImp *pDriver, DrmNlApi *&pDrmNl, SysmanDeviceImp *pSysmanDevice) {
    // Check if netlink events are supported on this platform/product
    if (pSysmanDevice != nullptr) {
        auto *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pSysmanDevice->deviceGetOsInterface());
        auto pSysmanProductHelper = pLinuxSysmanImp->getSysmanProductHelper();
        if (!pSysmanProductHelper->isNetlinkEventSupported()) {
            pDrmNl = nullptr;
            return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
    }

    pDrmNl = pDriver->getDrmNlApiHandle();

    // Cache RAS nodes list if not already populated
    // This is needed to map nodeId from events to device and error type
    ze_result_t result = NetlinkRasUtil::initializeRasNodes(pDrmNl);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    result = pDrmNl->subscribeToEvents();
    if (result != ZE_RESULT_SUCCESS) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                     "Failed to subscribe to netlink RAS events: 0x%x\n", result);
        pDrmNl = nullptr;
        return result;
    }

    return ZE_RESULT_SUCCESS;
}

bool netlinkHandleEvents(DrmNlApi *pDrmNl,
                         zes_event_type_flags_t *pEvents,
                         uint32_t count,
                         zes_device_handle_t *phDevices,
                         const std::vector<zes_event_type_flags_t> &registeredEvents) {
    DrmRasEvent netlinkEvent = {};
    ze_result_t result = pDrmNl->pollEvent(netlinkEvent);

    if (result != ZE_RESULT_SUCCESS) {
        return false;
    }

    return netlinkProcessRasEvent(netlinkEvent, pEvents, count, phDevices, registeredEvents);
}

bool netlinkProcessRasEvent(const DrmRasEvent &netlinkEvent,
                            zes_event_type_flags_t *pEvents,
                            uint32_t count,
                            zes_device_handle_t *phDevices,
                            const std::vector<zes_event_type_flags_t> &registeredEvents) {
    // Determine event type from nodeId by looking up the node in cached RAS nodes
    zes_event_type_flags_t eventType = 0;
    std::string nodeName;

    const auto &rasNodes = NetlinkRasUtil::getRasNodes();
    for (const auto &node : rasNodes) {
        if (node.nodeId == netlinkEvent.nodeId) {
            nodeName = node.nodeName;
            break;
        }
    }

    if (nodeName == "correctable-errors") {
        eventType = ZES_EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS;
    } else if (nodeName == "uncorrectable-errors") {
        eventType = ZES_EVENT_TYPE_FLAG_RAS_UNCORRECTABLE_ERRORS;
    } else {
        // Unknown node type, skip this event
        return false;
    }

    // Match nodeId to device and check if device is interested in this event type
    for (uint32_t devIndex = 0; devIndex < count; devIndex++) {
        if (!(registeredEvents[devIndex] & eventType)) {
            continue;
        }

        auto device = static_cast<SysmanDeviceImp *>(
            L0::Sysman::SysmanDevice::fromHandle(phDevices[devIndex]));

        if (device == nullptr) {
            continue;
        }

        // Get device PCI BDF for correlation
        auto *osInterface = static_cast<LinuxSysmanImp *>(device->deviceGetOsInterface());
        if (!osInterface) {
            continue;
        }

        std::string devicePciBdf = osInterface->getDevicePciBdf();

        // Find matching node in cached RAS nodes
        auto it = std::find_if(rasNodes.begin(),
                               rasNodes.end(),
                               [&](const DrmRasNode &node) {
                                   return node.nodeId == netlinkEvent.nodeId &&
                                          node.deviceName == devicePciBdf;
                               });

        if (it != rasNodes.end()) {
            pEvents[devIndex] |= eventType;
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                         "Netlink RAS event received for device %d: nodeId=%u errorId=%u type=0x%x\n",
                         devIndex, netlinkEvent.nodeId, netlinkEvent.errorId, eventType);
            return true;
        }
    }

    return false;
}

int netlinkGetSocketFd(DrmNlApi *pDrmNl) {
    if (pDrmNl == nullptr) {
        return -1;
    }
    return pDrmNl->getEventSocketFd();
}

bool isNetlinkSubscribed(DrmNlApi *pDrmNl) {
    if (pDrmNl == nullptr) {
        return false;
    }
    return pDrmNl->isSubscribedToEvents();
}

} // namespace Sysman
} // namespace L0
