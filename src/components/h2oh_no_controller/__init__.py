import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import number, switch
from esphome.const import CONF_ID

AUTO_LOAD = ["switch", "number"]
DEPENDENCIES = ["preferences"]

h2oh_no_controller_ns = cg.esphome_ns.namespace("h2oh_no_controller")
H2OhNoController = h2oh_no_controller_ns.class_("H2OhNoController", cg.Component)

CONF_VALVES = "valves"
CONF_LEDS = "leds"
CONF_MAX_RUNTIMES = "max_runtimes"
CONF_STATUS_LED_PIN = "status_led_pin"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(H2OhNoController),
            cv.Optional(CONF_STATUS_LED_PIN): pins.gpio_output_pin_schema,
            cv.Required(CONF_VALVES): cv.All(
                cv.ensure_list(cv.use_id(switch.Switch)),
                cv.Length(min=8, max=8),
            ),
            cv.Required(CONF_LEDS): cv.All(
                cv.ensure_list(cv.use_id(switch.Switch)),
                cv.Length(min=8, max=8),
            ),
            cv.Required(CONF_MAX_RUNTIMES): cv.All(
                cv.ensure_list(cv.use_id(number.Number)),
                cv.Length(min=8, max=8),
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    if CONF_STATUS_LED_PIN in config:
        pin = await cg.gpio_pin_expression(config[CONF_STATUS_LED_PIN])
        cg.add(var.set_status_led_pin(pin))
    for i in range(8):
        sw = await cg.get_variable(config[CONF_VALVES][i])
        cg.add(var.set_valve(i, sw))
    for i in range(8):
        sw = await cg.get_variable(config[CONF_LEDS][i])
        cg.add(var.set_led(i, sw))
    for i in range(8):
        num = await cg.get_variable(config[CONF_MAX_RUNTIMES][i])
        cg.add(var.set_max_runtime(i, num))
