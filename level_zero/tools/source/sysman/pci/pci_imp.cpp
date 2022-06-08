/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "pci_imp.h"

#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/string.h"
#include "shared/source/utilities/directory.h"

#include <cstring>
namespace L0 {

//
// While computing the PCIe bandwidth, also consider that due to 8b/10b encoding
// in PCIe gen1 and gen2  real bandwidth will be reduced by 20%,
// And in case of gen3 and above due to 128b/130b encoding real bandwidth is
// reduced by approx 1.54% as compared to theoretical bandwidth.
// In below method, get real PCIe speed in pcieSpeedWithEnc in Mega bits per second
// pcieSpeedWithEnc = maxLinkSpeedInGt * (Gigabit to Megabit) * Encoding =
//                               maxLinkSpeedInGt * 1000 * Encoding
//
int64_t convertPcieSpeedFromGTsToBs(double maxLinkSpeedInGt) {
    double pcieSpeedWithEnc;
    if ((maxLinkSpeedInGt == PciLinkSpeeds::Pci32_0GigatransfersPerSecond) || (maxLinkSpeedInGt == PciLinkSpeeds::Pci16_0GigatransfersPerSecond) || (maxLinkSpeedInGt == PciLinkSpeeds::Pci8_0GigatransfersPerSecond)) {
        pcieSpeedWithEnc = maxLinkSpeedInGt * 1000 * 128 / 130;
    } else if ((maxLinkSpeedInGt == PciLinkSpeeds::Pci5_0GigatransfersPerSecond) || (maxLinkSpeedInGt == PciLinkSpeeds::Pci2_5GigatransfersPerSecond)) {
        pcieSpeedWithEnc = maxLinkSpeedInGt * 1000 * 8 / 10;
    } else {
        pcieSpeedWithEnc = 0;
    }

    //
    // PCIE speed we got above is in Mega bits per second
    //  Convert that speed in bytes/second.
    //  Now, because 1Mb/s = (1000*1000)/8 bytes/second = 125000 bytes/second
    //
    pcieSpeedWithEnc = pcieSpeedWithEnc * 125000;
    return static_cast<int64_t>(pcieSpeedWithEnc);
}

double convertPciGenToLinkSpeed(uint32_t gen) {
    switch (gen) {
    case PciGenerations::PciGen1: {
        return PciLinkSpeeds::Pci2_5GigatransfersPerSecond;
    } break;
    case PciGenerations::PciGen2: {
        return PciLinkSpeeds::Pci5_0GigatransfersPerSecond;
    } break;
    case PciGenerations::PciGen3: {
        return PciLinkSpeeds::Pci8_0GigatransfersPerSecond;
    } break;
    case PciGenerations::PciGen4: {
        return PciLinkSpeeds::Pci16_0GigatransfersPerSecond;
    } break;
    case PciGenerations::PciGen5: {
        return PciLinkSpeeds::Pci32_0GigatransfersPerSecond;
    } break;
    default: {
        return 0.0;
    } break;
    }
}

int32_t convertLinkSpeedToPciGen(double speed) {
    if (speed == PciLinkSpeeds::Pci2_5GigatransfersPerSecond) {
        return PciGenerations::PciGen1;
    } else if (speed == PciLinkSpeeds::Pci5_0GigatransfersPerSecond) {
        return PciGenerations::PciGen2;
    } else if (speed == PciLinkSpeeds::Pci8_0GigatransfersPerSecond) {
        return PciGenerations::PciGen3;
    } else if (speed == PciLinkSpeeds::Pci16_0GigatransfersPerSecond) {
        return PciGenerations::PciGen4;
    } else if (speed == PciLinkSpeeds::Pci32_0GigatransfersPerSecond) {
        return PciGenerations::PciGen5;
    } else {
        return -1;
    }
}

ze_result_t PciImp::pciStaticProperties(zes_pci_properties_t *pProperties) {
    *pProperties = pciProperties;
    return ZE_RESULT_SUCCESS;
}

ze_result_t PciImp::pciGetInitializedBars(uint32_t *pCount, zes_pci_bar_properties_t *pProperties) {
    uint32_t pciBarPropertiesSize = static_cast<uint32_t>(pciBarProperties.size());
    uint32_t numToCopy = std::min(*pCount, pciBarPropertiesSize);
    if (0 == *pCount || *pCount > pciBarPropertiesSize) {
        *pCount = pciBarPropertiesSize;
    }
    if (nullptr != pProperties) {
        for (uint32_t i = 0; i < numToCopy; i++) {
            pProperties[i].base = pciBarProperties[i]->base;
            pProperties[i].index = pciBarProperties[i]->index;
            pProperties[i].size = pciBarProperties[i]->size;
            pProperties[i].type = pciBarProperties[i]->type;

            if (pProperties[i].pNext != nullptr) {
                zes_pci_bar_properties_1_2_t *pBarPropsExt = static_cast<zes_pci_bar_properties_1_2_t *>(pProperties[i].pNext);
                if (pBarPropsExt->stype == zes_structure_type_t::ZES_STRUCTURE_TYPE_PCI_BAR_PROPERTIES_1_2) {
                    // base, index, size and type are the same as the non 1.2 struct.
                    pBarPropsExt->base = pciBarProperties[i]->base;
                    pBarPropsExt->index = pciBarProperties[i]->index;
                    pBarPropsExt->size = pciBarProperties[i]->size;
                    pBarPropsExt->type = pciBarProperties[i]->type;
                    pBarPropsExt->resizableBarSupported = static_cast<ze_bool_t>(resizableBarSupported);
                    pBarPropsExt->resizableBarEnabled = static_cast<ze_bool_t>(pOsPci->resizableBarEnabled(pBarPropsExt->index));
                }
            }
        }
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t PciImp::pciGetState(zes_pci_state_t *pState) {
    return pOsPci->getState(pState);
}

void PciImp::pciGetStaticFields() {
    pOsPci->getProperties(&pciProperties);
    resizableBarSupported = pOsPci->resizableBarSupported();
    std::string bdf;
    pOsPci->getPciBdf(pciProperties);
    int32_t maxLinkWidth = -1;
    int64_t maxBandWidth = -1;
    double maxLinkSpeed = 0;
    pOsPci->getMaxLinkCaps(maxLinkSpeed, maxLinkWidth);
    maxBandWidth = maxLinkWidth * convertPcieSpeedFromGTsToBs(maxLinkSpeed);
    if (maxBandWidth == 0) {
        pciProperties.maxSpeed.maxBandwidth = -1;
    } else {
        pciProperties.maxSpeed.maxBandwidth = maxBandWidth;
    }
    pciProperties.maxSpeed.width = maxLinkWidth;
    pciProperties.maxSpeed.gen = convertLinkSpeedToPciGen(maxLinkSpeed);
    pOsPci->initializeBarProperties(pciBarProperties);
}

void PciImp::init() {
    if (pOsPci == nullptr) {
        pOsPci = OsPci::create(pOsSysman);
    }
    UNRECOVERABLE_IF(nullptr == pOsPci);

    pciGetStaticFields();
}

PciImp::~PciImp() {
    for (zes_pci_bar_properties_t *pProperties : pciBarProperties) {
        delete pProperties;
        pProperties = nullptr;
    }
    if (nullptr != pOsPci) {
        delete pOsPci;
        pOsPci = nullptr;
    }
}
} // namespace L0
