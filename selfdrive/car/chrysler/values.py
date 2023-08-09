# ruff: noqa: E501
from enum import IntFlag
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Union

from cereal import car
from panda.python import uds
from selfdrive.car import dbc_dict
from selfdrive.car.docs_definitions import CarHarness, CarInfo, CarParts
from selfdrive.car.fw_query_definitions import FwQueryConfig, Request, p16

Ecu = car.CarParams.Ecu


class ChryslerFlags(IntFlag):
  HIGHER_MIN_STEERING_SPEED = 1


class CAR:
  # Chrysler
  PACIFICA_2017_HYBRID = "CHRYSLER PACIFICA HYBRID 2017"
  PACIFICA_2018_HYBRID = "CHRYSLER PACIFICA HYBRID 2018"
  PACIFICA_2019_HYBRID = "CHRYSLER PACIFICA HYBRID 2019"
  PACIFICA_2018 = "CHRYSLER PACIFICA 2018"
  PACIFICA_2020 = "CHRYSLER PACIFICA 2020"

  # Jeep
  JEEP_CHEROKEE = "JEEP GRAND CHEROKEE V6 2018"   # includes 2017 Trailhawk
  JEEP_CHEROKEE_2019 = "JEEP GRAND CHEROKEE 2019" # includes 2020 Trailhawk

  # Ram
  RAM_1500 = "RAM 1500 5TH GEN"
  RAM_HD = "RAM HD 5TH GEN"


class CarControllerParams:
  def __init__(self, CP):
    self.STEER_STEP = 2  # 50 Hz
    self.STEER_ERROR_MAX = 80
    if CP.carFingerprint in RAM_HD:
      self.STEER_DELTA_UP = 14
      self.STEER_DELTA_DOWN = 14
      self.STEER_MAX = 361  # higher than this faults the EPS
    elif CP.carFingerprint in RAM_DT:
      self.STEER_DELTA_UP = 6
      self.STEER_DELTA_DOWN = 6
      self.STEER_MAX = 261  # EPS allows more, up to 350?
    else:
      self.STEER_DELTA_UP = 3
      self.STEER_DELTA_DOWN = 3
      self.STEER_MAX = 261  # higher than this faults the EPS


STEER_THRESHOLD = 120

RAM_DT = {CAR.RAM_1500, }
RAM_HD = {CAR.RAM_HD, }
RAM_CARS = RAM_DT | RAM_HD


@dataclass
class ChryslerCarInfo(CarInfo):
  package: str = "Adaptive Cruise Control (ACC)"
  car_parts: CarParts = field(default_factory=CarParts.common([CarHarness.fca]))


CAR_INFO: Dict[str, Optional[Union[ChryslerCarInfo, List[ChryslerCarInfo]]]] = {
  CAR.PACIFICA_2017_HYBRID: ChryslerCarInfo("Chrysler Pacifica Hybrid 2017-18"),
  CAR.PACIFICA_2018_HYBRID: None,  # same platforms
  CAR.PACIFICA_2019_HYBRID: ChryslerCarInfo("Chrysler Pacifica Hybrid 2019-23"),
  CAR.PACIFICA_2018: ChryslerCarInfo("Chrysler Pacifica 2017-18"),
  CAR.PACIFICA_2020: [
    ChryslerCarInfo("Chrysler Pacifica 2019-20"),
    ChryslerCarInfo("Chrysler Pacifica 2021", package="All"),
  ],
  CAR.JEEP_CHEROKEE: ChryslerCarInfo("Jeep Grand Cherokee 2016-18", video_link="https://www.youtube.com/watch?v=eLR9o2JkuRk"),
  CAR.JEEP_CHEROKEE_2019: ChryslerCarInfo("Jeep Grand Cherokee 2019-21", video_link="https://www.youtube.com/watch?v=jBe4lWnRSu4"),
  CAR.RAM_1500: ChryslerCarInfo("Ram 1500 2019-23", car_parts=CarParts.common([CarHarness.ram])),
  CAR.RAM_HD: [
    ChryslerCarInfo("Ram 2500 2020-22", car_parts=CarParts.common([CarHarness.ram])),
    ChryslerCarInfo("Ram 3500 2019-22", car_parts=CarParts.common([CarHarness.ram])),
  ],
}

# Unique CAN messages:
# Only the hybrids have 270: 8
# Only the gas have 55: 8, 416: 7
# For 564, All 2017 have length 4, whereas 2018-19 have length 8.
# For 924, Pacifica 2017 has length 3, whereas all 2018-19 have length 8.
# For 560, All 2019 have length 8, whereas all 2017-18 have length 4.

# Jeep Grand Cherokee unique messages:
# 2017 Trailhawk: 618: 8
# For 924, Trailhawk 2017 has length 3, whereas 2018 V6 has length 8.

CHRYSLER_VERSION_REQUEST = bytes([uds.SERVICE_TYPE.READ_DATA_BY_IDENTIFIER]) + \
  p16(0xf132)
CHRYSLER_VERSION_RESPONSE = bytes([uds.SERVICE_TYPE.READ_DATA_BY_IDENTIFIER + 0x40]) + \
  p16(0xf132)

CHRYSLER_SOFTWARE_VERSION_REQUEST = bytes([uds.SERVICE_TYPE.READ_DATA_BY_IDENTIFIER]) + \
  p16(uds.DATA_IDENTIFIER_TYPE.SYSTEM_SUPPLIER_ECU_SOFTWARE_NUMBER)
CHRYSLER_SOFTWARE_VERSION_RESPONSE = bytes([uds.SERVICE_TYPE.READ_DATA_BY_IDENTIFIER + 0x40]) + \
  p16(uds.DATA_IDENTIFIER_TYPE.SYSTEM_SUPPLIER_ECU_SOFTWARE_NUMBER)

CHRYSLER_RX_OFFSET = -0x280

FW_QUERY_CONFIG = FwQueryConfig(
  requests=[
    Request(
      [CHRYSLER_VERSION_REQUEST],
      [CHRYSLER_VERSION_RESPONSE],
      whitelist_ecus=[Ecu.abs, Ecu.eps, Ecu.srs, Ecu.fwdRadar, Ecu.fwdCamera, Ecu.combinationMeter],
      rx_offset=CHRYSLER_RX_OFFSET,
      bus=0,
    ),
    Request(
      [CHRYSLER_VERSION_REQUEST],
      [CHRYSLER_VERSION_RESPONSE],
      whitelist_ecus=[Ecu.abs, Ecu.hybrid, Ecu.engine, Ecu.transmission],
      bus=0,
    ),
    Request(
      [CHRYSLER_SOFTWARE_VERSION_REQUEST],
      [CHRYSLER_SOFTWARE_VERSION_RESPONSE],
      whitelist_ecus=[Ecu.engine, Ecu.transmission],
      bus=0,
    ),
  ],
  extra_ecus=[
    (Ecu.hybrid, 0x7e2, None),  # manages transmission on hybrids
    (Ecu.abs, 0x7e4, None),     # alt address for abs on hybrids
  ],
)

DBC = {
  CAR.PACIFICA_2017_HYBRID: dbc_dict('chrysler_pacifica_2017_hybrid_generated', 'chrysler_pacifica_2017_hybrid_private_fusion'),
  CAR.PACIFICA_2018: dbc_dict('chrysler_pacifica_2017_hybrid_generated', 'chrysler_pacifica_2017_hybrid_private_fusion'),
  CAR.PACIFICA_2020: dbc_dict('chrysler_pacifica_2017_hybrid_generated', 'chrysler_pacifica_2017_hybrid_private_fusion'),
  CAR.PACIFICA_2018_HYBRID: dbc_dict('chrysler_pacifica_2017_hybrid_generated', 'chrysler_pacifica_2017_hybrid_private_fusion'),
  CAR.PACIFICA_2019_HYBRID: dbc_dict('chrysler_pacifica_2017_hybrid_generated', 'chrysler_pacifica_2017_hybrid_private_fusion'),
  CAR.JEEP_CHEROKEE: dbc_dict('chrysler_pacifica_2017_hybrid_generated', 'chrysler_pacifica_2017_hybrid_private_fusion'),
  CAR.JEEP_CHEROKEE_2019: dbc_dict('chrysler_pacifica_2017_hybrid_generated', 'chrysler_pacifica_2017_hybrid_private_fusion'),
  CAR.RAM_1500: dbc_dict('chrysler_ram_dt_generated', None),
  CAR.RAM_HD: dbc_dict('chrysler_ram_hd_generated', None),
}
