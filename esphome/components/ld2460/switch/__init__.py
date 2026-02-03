import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import (
    CONF_ID,
    ENTITY_CATEGORY_CONFIG,
    ICON_RADAR,
)
from .. import CONF_LD2460_ID, LD2460Component, ld2460_ns

CONF_REPORTING = "reporting"

ReportingSwitch = ld2460_ns.class_(
    "ReportingSwitch", switch.Switch, cg.Parented.template(LD2460Component)
)

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_ID): cv.declare_id(cg.EntityBase),
    cv.GenerateID(CONF_LD2460_ID): cv.use_id(LD2460Component),
    cv.Optional(CONF_REPORTING): switch.switch_schema(
        ReportingSwitch,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_RADAR,
    ),
}


async def to_code(config):
    ld2460_component = await cg.get_variable(config[CONF_LD2460_ID])
    if reporting_config := config.get(CONF_REPORTING):
        s = await switch.new_switch(reporting_config)
        await cg.register_parented(s, config[CONF_LD2460_ID])
        cg.add(ld2460_component.set_reporting_switch(s))
