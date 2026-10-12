import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button

from .. import Switch2Wake, switch2_wake_ns

DEPENDENCIES = ["switch2_wake"]

CONF_SWITCH2_WAKE_ID = "switch2_wake_id"

WakeButton = switch2_wake_ns.class_("WakeButton", button.Button)

CONFIG_SCHEMA = button.button_schema(WakeButton, icon="mdi:nintendo-switch").extend(
    {
        cv.GenerateID(CONF_SWITCH2_WAKE_ID): cv.use_id(Switch2Wake),
    }
)


async def to_code(config):
    var = await button.new_button(config)
    await cg.register_parented(var, config[CONF_SWITCH2_WAKE_ID])
