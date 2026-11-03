"""Powerpal BLE sensors implemented by the PalPulse ESPHome component."""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import ble_client, sensor, time
from esphome.const import (
    CONF_BATTERY_LEVEL,
    CONF_ID,
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_POWER,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_KILOWATT_HOURS,
    UNIT_PERCENT,
    UNIT_WATT,
)

DEPENDENCIES = ["ble_client"]
AUTO_LOAD = ["sensor"]

CONF_PAIRING_CODE = "pairing_code"
CONF_NOTIFICATION_INTERVAL = "notification_interval"
CONF_PULSES_PER_KWH = "pulses_per_kwh"
CONF_TIME_ID = "time_id"
CONF_LIVE_POWER = "live_power"
CONF_POWER = "power"
CONF_ENERGY = "energy"
CONF_DAILY_ENERGY = "daily_energy"
CONF_LOG_API_KEY = "log_api_key"
CONF_HISTORY_EVENTS = "history_events"

palpulse_ns = cg.esphome_ns.namespace("powerpal_ble")
PalPulse = palpulse_ns.class_("PalPulse", cg.Component, ble_client.BLEClientNode)


def _validate(config):
    if config.get(CONF_LOG_API_KEY):
        raise cv.Invalid(
            "PalPulse never logs Powerpal cloud API credentials; remove log_api_key: true"
        )
    if config.get(CONF_LIVE_POWER, CONF_POWER in config) and CONF_POWER not in config:
        raise cv.Invalid("live_power: true requires a power sensor")
    if CONF_LIVE_POWER not in config:
        config[CONF_LIVE_POWER] = CONF_POWER in config
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(PalPulse),
            cv.Required(CONF_PAIRING_CODE): cv.int_range(min=1, max=999999),
            cv.Required(CONF_NOTIFICATION_INTERVAL): cv.int_range(min=1, max=60),
            cv.Required(CONF_PULSES_PER_KWH): cv.positive_float,
            cv.Optional(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
            cv.Optional(CONF_LIVE_POWER): cv.boolean,
            cv.Optional(CONF_HISTORY_EVENTS, default=False): cv.boolean,
            cv.Optional(CONF_LOG_API_KEY, default=False): cv.boolean,
            cv.Optional(CONF_POWER): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_POWER,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_ENERGY): sensor.sensor_schema(
                unit_of_measurement=UNIT_KILOWATT_HOURS,
                accuracy_decimals=5,
                device_class=DEVICE_CLASS_ENERGY,
                state_class=STATE_CLASS_TOTAL_INCREASING,
            ),
            cv.Optional(CONF_DAILY_ENERGY): sensor.sensor_schema(
                unit_of_measurement=UNIT_KILOWATT_HOURS,
                accuracy_decimals=5,
                device_class=DEVICE_CLASS_ENERGY,
                state_class=STATE_CLASS_TOTAL_INCREASING,
            ),
            cv.Optional(CONF_BATTERY_LEVEL): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_BATTERY,
                state_class=STATE_CLASS_MEASUREMENT,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA).extend(ble_client.BLE_CLIENT_SCHEMA),
    _validate,
)


async def to_code(config):
    device = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(device, config)
    await ble_client.register_ble_node(device, config)
    cg.add(device.set_pairing_code(config[CONF_PAIRING_CODE]))
    cg.add(device.set_pulses_per_kwh(config[CONF_PULSES_PER_KWH]))
    cg.add(device.set_notification_interval(config[CONF_NOTIFICATION_INTERVAL]))
    cg.add(device.set_live_power(config[CONF_LIVE_POWER]))
    cg.add(device.set_history_events(config[CONF_HISTORY_EVENTS]))
    if CONF_TIME_ID in config:
        cg.add(device.set_time(await cg.get_variable(config[CONF_TIME_ID])))
    for option, setter in (
        (CONF_POWER, device.set_power_sensor),
        (CONF_ENERGY, device.set_energy_sensor),
        (CONF_DAILY_ENERGY, device.set_daily_energy_sensor),
        (CONF_BATTERY_LEVEL, device.set_battery_sensor),
    ):
        if option in config:
            cg.add(setter(await sensor.new_sensor(config[option])))
