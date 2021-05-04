//
//  DYWMI.cpp
//  YogaSMC
//
//  Created by Zhen on 4/24/21.
//  Copyright © 2021 Zhen. All rights reserved.
//

#include "DYWMI.hpp"
OSDefineMetaClassAndStructors(DYWMI, YogaWMI);

DYWMI* DYWMI::withDevice(IOService *provider) {
    DYWMI* dev = OSTypeAlloc(DYWMI);

    OSDictionary *dictionary = OSDictionary::withCapacity(1);

    dev->name = provider->getName();

    if (!dev->init(dictionary) ||
        !dev->attach(provider)) {
        OSSafeReleaseNULL(dev);
    }

    dictionary->release();
    return dev;
}

IOReturn DYWMI::setProperties(OSObject *props) {
    commandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &DYWMI::setPropertiesGated), props);
    return kIOReturnSuccess;
}

void DYWMI::setPropertiesGated(OSObject* props) {
    OSDictionary* dict = OSDynamicCast(OSDictionary, props);
    if (!dict)
        return;

    OSCollectionIterator* i = OSCollectionIterator::withCollection(dict);
    if (i) {
        while (OSString* key = OSDynamicCast(OSString, i->getNextObject())) {
            if (key->isEqualTo(updateSensorPrompt)) {
                OSNumber *value;
                getPropertyNumber(updateSensorPrompt);

                if (value->unsigned8BitValue() > sensorRange) {
                    AlwaysLog(valueMatched, updateSensorPrompt, sensorRange);
                    continue;
                }

                OSObject *result;
                if (getSensorInfo(value->unsigned8BitValue(), &result)) {
                    char sensorName[10];
                    snprintf(sensorName, 10, "Sensor %d", value->unsigned8BitValue());
                    if (result) {
                        setProperty(sensorName, result);
                        DebugLog(toggleSuccess, updateSensorPrompt, value->unsigned8BitValue(), "see ioreg");
                    } else {
                        AlwaysLog("%s empty result, please check the method return", sensorName);
                    }
                } else {
                    AlwaysLog(toggleFailure, updateSensorPrompt);
                }
                OSSafeReleaseNULL(result);
            } else {
                AlwaysLog("Unknown property %s", key->getCStringNoCopy());
            }
        }
        i->release();
    }

    return;
}

void DYWMI::processWMI() {
    if (YWMI->hasMethod(SENSOR_DATA_WMI_METHOD, 0)) {
        setProperty("Feature", "Sensor");
        sensorRange = YWMI->getInstanceCount(SENSOR_DATA_WMI_METHOD);
        registerService();
    }
}

bool DYWMI::getSensorInfo(UInt8 index, OSObject **result) {
    bool ret;

    OSObject* params[1] = {
        OSNumber::withNumber(index, 32),
    };

    ret = YWMI->executeMethod(SENSOR_DATA_WMI_METHOD, result, params, 1, true);
    params[0]->release();

    if (!ret)
        AlwaysLog("Sensor %d evaluation failed", index);
    return ret;
}