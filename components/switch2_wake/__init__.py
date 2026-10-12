"""ESPHome external component: wake a Nintendo Switch 2 over BLE.

The Switch 2 wakes from sleep when it receives a specific BLE advertisement
from a controller it is already paired with. This component reproduces that
advertisement from an ESP32, using the paired controller's Bluetooth address.
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import esp32_ble
from esphome.const import CONF_DURATION, CONF_ID

CODEOWNERS = ["@thiagobucca"]
DEPENDENCIES = ["esp32"]
AUTO_LOAD = ["esp32_ble"]

CONF_CONTROLLER_MAC = "controller_mac"
CONF_CONSOLE_MAC = "console_mac"
CONF_PRODUCT_ID = "product_id"
CONF_INTERVAL = "interval"

switch2_wake_ns = cg.esphome_ns.namespace("switch2_wake")
Switch2Wake = switch2_wake_ns.class_(
    "Switch2Wake", cg.Component, cg.Parented.template(esp32_ble.ESP32BLE)
)
SendAction = switch2_wake_ns.class_("SendAction", automation.Action)

# Known Nintendo product IDs. Prefer the value from your own capture.
PRODUCT_IDS = {
    "joycon2_left": 0x2066,
    "joycon2_right": 0x2067,
    "pro_controller2": 0x2069,
}


def _product_id(value):
    if isinstance(value, str) and value.lower() in PRODUCT_IDS:
        return PRODUCT_IDS[value.lower()]
    return cv.hex_uint16_t(value)


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(Switch2Wake),
        cv.GenerateID(esp32_ble.CONF_BLE_ID): cv.use_id(esp32_ble.ESP32BLE),
        # Bluetooth address of a controller paired with the console. The ESP32
        # advertises from this address, which is what the console checks.
        cv.Required(CONF_CONTROLLER_MAC): cv.mac_address,
        # Bluetooth address of the console (embedded in the payload).
        cv.Required(CONF_CONSOLE_MAC): cv.mac_address,
        cv.Optional(CONF_PRODUCT_ID, default="joycon2_left"): _product_id,
        cv.Optional(CONF_DURATION, default="2s"): cv.All(
            cv.positive_time_period_milliseconds,
            cv.Range(min=cv.TimePeriod(milliseconds=100), max=cv.TimePeriod(seconds=30)),
        ),
        cv.Optional(CONF_INTERVAL, default="20ms"): cv.All(
            cv.positive_time_period_milliseconds,
            cv.Range(min=cv.TimePeriod(milliseconds=20), max=cv.TimePeriod(milliseconds=1000)),
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    ble = await cg.get_variable(config[esp32_ble.CONF_BLE_ID])
    cg.add(var.set_parent(ble))
    esp32_ble.register_gap_event_handler(ble, var)

    cg.add(var.set_controller_address(config[CONF_CONTROLLER_MAC].as_hex))
    cg.add(var.set_console_address(config[CONF_CONSOLE_MAC].as_hex))
    cg.add(var.set_product_id(config[CONF_PRODUCT_ID]))
    cg.add(var.set_duration(config[CONF_DURATION].total_milliseconds))
    cg.add(var.set_interval(config[CONF_INTERVAL].total_milliseconds))


@automation.register_action(
    "switch2_wake.send",
    SendAction,
    automation.maybe_simple_id({cv.GenerateID(): cv.use_id(Switch2Wake)}),
    synchronous=True,
)
async def send_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var
