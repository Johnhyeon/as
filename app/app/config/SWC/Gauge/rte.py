# SSAS - Simple Smart Automotive Software
# Copyright (C) 2022 Parai Wang <parai@foxmail.com>
from generator import asar
import autosar
from bswcom import *


C_VehicleSpeed_IV = C_CAN1_RxMsgAbsInfo_VehicleSpeed_IV
C_TachoSpeed_IV = C_CAN1_RxMsgAbsInfo_TachoSpeed_IV

VehicleSpeed = asar.createSenderReceiverPortTemplate('Com', COM_I, C_VehicleSpeed_IV, aliveTimeout=30, elemName='VehicleSpeed')
TachoSpeed = asar.createSenderReceiverPortTemplate('Com', COM_I, C_TachoSpeed_IV, aliveTimeout=30, elemName='TachoSpeed')
COM_D.append(asar.createDataElementTemplate('VehicleSpeed', asar.UINT16_T))
COM_D.append(asar.createDataElementTemplate('TachoSpeed', asar.UINT16_T))

STMO_D = []
STMO_D.append(asar.createDataElementTemplate('VehicleSpeed', asar.UINT16_T))
STMO_D.append(asar.createDataElementTemplate('TachoSpeed', asar.UINT16_T))

STMO_I = asar.createSenderReceiverInterfaceTemplate("STMO_I", STMO_D)
VehicleSpeed_stmo = asar.createSenderReceiverPortTemplate(
    "Stmo", STMO_I, C_VehicleSpeed_IV, aliveTimeout=30, elemName="VehicleSpeed"
)
TachoSpeed_stmo = asar.createSenderReceiverPortTemplate(
    "Stmo", STMO_I, C_TachoSpeed_IV, aliveTimeout=30, elemName="TachoSpeed"
)


class Gauge(autosar.Template):
    @classmethod
    def apply(cls, ws):
        componentName = cls.__name__
        package = ws.getComponentTypePackage()
        if package.find(componentName) is None:
            swc = package.createApplicationSoftwareComponent(componentName)
            cls.addPorts(swc)
            cls.addBehavior(swc)

    @classmethod
    def addPorts(cls, swc):
        componentName = cls.__name__
        swc.apply(VehicleSpeed.Receive)
        swc.apply(TachoSpeed.Receive)
        swc.apply(VehicleSpeed_stmo.Send)
        swc.apply(TachoSpeed_stmo.Send)

    @classmethod
    def addBehavior(cls, swc):
        componentName = cls.__name__
        swc.behavior.createRunnable(componentName + "_Init")
        swc.behavior.createRunnable(componentName + "_Exit")
        swc.behavior.createRunnable(
            componentName + "_Run",
            portAccess=["%s/%s" % (p.name, p.comspec[0].name) for p in swc.requirePorts + swc.providePorts],
        )
        swc.behavior.createTimerEvent(componentName + "_Run", 20)


def main(dir):
    asar.Gen(Gauge, dir)
