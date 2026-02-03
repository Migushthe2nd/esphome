import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import (
    ENTITY_CATEGORY_CONFIG,
    ICON_RADAR,
)
from .. import LD2460Component, ld2460_ns

CONF_REPORTING = "reporting"

ReportingSwitch = ld2460_ns.class_(
    "ReportingSwitch", switch.Switch, cg.Parented.template(LD2460Component)
)

CONFIG_SCHEMA = {
    cv.Optional(CONF_REPORTING): switch.switch_schema(
        ReportingSwitch,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_RADAR,
    ),
}


async def to_code(config):
    ld2460_component = await cg.get_variable(config)
    if reporting_config := config.get(CONF_REPORTING):
        s = await switch.new_switch(reporting_config)
        await cg.register_parented(s, config)
        cg.add(ld2460_component.set_reporting_switch(s))
