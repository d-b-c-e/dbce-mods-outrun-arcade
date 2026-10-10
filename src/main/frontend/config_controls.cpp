/***************************************************************************
    Controls settings, read from config.xml (Config::load) apart from the rest of the
    configuration, so tests can read a written config.xml through the same parser and key
    paths without the engine (tests/config_controls_test.cpp).
***************************************************************************/

#include "config.hpp"

void Config::load_controls(const xml_parser::ptree& xml)
{
    controls.gear          = xml.get_int("controls.gear",               2);
    controls.steer_speed   = xml.get_int("controls.steerspeed",         3);
    controls.pedal_speed   = xml.get_int("controls.pedalspeed",         4);
    controls.rumble        = xml.get_float("controls.rumble",           1.25f);
    controls.keyconfig[0]  = xml.get_int("controls.keyconfig.up",       1073741906);
    controls.keyconfig[1]  = xml.get_int("controls.keyconfig.down",     1073741905);
    controls.keyconfig[2]  = xml.get_int("controls.keyconfig.left",     1073741904);
    controls.keyconfig[3]  = xml.get_int("controls.keyconfig.right",    1073741903);
    controls.keyconfig[4]  = xml.get_int("controls.keyconfig.acc",      97);
    controls.keyconfig[5]  = xml.get_int("controls.keyconfig.brake",    122);
    controls.keyconfig[6]  = xml.get_int("controls.keyconfig.gear1",    103);
    controls.keyconfig[7]  = xml.get_int("controls.keyconfig.gear2",    104);
    controls.keyconfig[8]  = xml.get_int("controls.keyconfig.start",    115);
    controls.keyconfig[9]  = xml.get_int("controls.keyconfig.coin",     99);
    controls.keyconfig[10] = xml.get_int("controls.keyconfig.menu",     109);
    controls.keyconfig[11] = xml.get_int("controls.keyconfig.view",     118);
    controls.padconfig[0]  = xml.get_int("controls.padconfig.acc",      -1);
    controls.padconfig[1]  = xml.get_int("controls.padconfig.brake",    -1);
    controls.padconfig[2]  = xml.get_int("controls.padconfig.gear1",    -1);
    controls.padconfig[3]  = xml.get_int("controls.padconfig.gear2",    -1);
    controls.padconfig[4]  = xml.get_int("controls.padconfig.start",    -1);
    controls.padconfig[5]  = xml.get_int("controls.padconfig.coin",     -1);
    controls.padconfig[6]  = xml.get_int("controls.padconfig.menu",     -1);
    controls.padconfig[7]  = xml.get_int("controls.padconfig.view",     -1);
    controls.padconfig[8]  = xml.get_int("controls.padconfig.up",       -1);
    controls.padconfig[9]  = xml.get_int("controls.padconfig.down",     -1);
    controls.padconfig[10] = xml.get_int("controls.padconfig.left",     -1);
    controls.padconfig[11] = xml.get_int("controls.padconfig.right",    -1);
    controls.padconfig[12] = xml.get_int("controls.padconfig.limit_l",  -1);
    controls.padconfig[13] = xml.get_int("controls.padconfig.limit_c",  -1);
    controls.padconfig[14] = xml.get_int("controls.padconfig.limit_r",  -1);
    controls.analog        = xml.get_int("controls.analog.<xmlattr>.enabled", 1);
    controls.pad_id        = xml.get_int("controls.pad_id",             0);
    controls.axis[0]       = xml.get_int("controls.analog.axis.wheel",  -1);
    controls.axis[1]       = xml.get_int("controls.analog.axis.accel",  -1);
    controls.axis[2]       = xml.get_int("controls.analog.axis.brake",  -1);
    controls.axis[3]       = xml.get_int("controls.analog.axis.motor",  -1);
    controls.invert[1]     = xml.get_int("controls.analog.axis.accel.<xmlattr>.invert", 0);
    controls.invert[2]     = xml.get_int("controls.analog.axis.brake.<xmlattr>.invert", 0);
    // Rig profile (STD-033, profile_device.hpp). Malformed values fail closed: an unreadable identity is not qualified
    // (joystick off), an unreadable rest reads as a released pedal. Config::save keeps these keys (it reuses cfg).
    {
        const std::string vendor  = xml.get_string("controls.pad_device.<xmlattr>.vendor", "");
        const std::string product = xml.get_string("controls.pad_device.<xmlattr>.product", "");
        controls.pad_device = profile_device::Want{};
        controls.pad_device.set = !vendor.empty() || !product.empty();
        if (controls.pad_device.set && (!profile_device::parseId(vendor, controls.pad_device.vendor) ||
                                        !profile_device::parseId(product, controls.pad_device.product)))
            controls.pad_device.vendor = controls.pad_device.product = 0;
        controls.pad_device.name = xml.get_string("controls.pad_device.<xmlattr>.name", "");
        // The profile wheel's DirectInput instance (Wheelkit writes it): what test injection's raw commands name as dev=.
        controls.pad_device_instance = xml.get_string("controls.pad_device.<xmlattr>.instance", "");
        const char* rests[3] = {nullptr, "controls.analog.axis.accel.<xmlattr>.rest", "controls.analog.axis.brake.<xmlattr>.rest"};
        for (int i = 0; i < 3; ++i) {
            const std::string rest = rests[i] ? xml.get_string(rests[i], "") : "";
            controls.has_rest[i] = !rest.empty();
            controls.rest[i] = controls.has_rest[i] ? profile_device::parseRest(rest) : 0;
        }
    }
    controls.asettings[0]  = xml.get_int("controls.analog.wheel.zone",  75);
    controls.asettings[1]  = xml.get_int("controls.analog.wheel.dead",  0);

    controls.haptic        = xml.get_int("controls.analog.haptic.<xmlattr>.enabled",    1);
    controls.force_device_guid = xml.get_string("controls.analog.haptic.device_guid", "");
    controls.max_force     = xml.get_int("controls.analog.haptic.max_force",            9000);
    controls.min_force     = xml.get_int("controls.analog.haptic.min_force",            8500);
    controls.force_duration= xml.get_int("controls.analog.haptic.force_duration",       20);
}
