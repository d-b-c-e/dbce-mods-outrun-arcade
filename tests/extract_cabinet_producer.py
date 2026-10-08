"""Compile the original producer, not a rewritten force approximation.

The generated include contains unchanged constructor/init and motor methods.
Only engine globals and the physical sink are supplied by the offline fixture.
"""
from pathlib import Path
import sys

source = Path(sys.argv[1]).read_text(encoding="utf-8")
start = source.index("OOutputs::OOutputs(void)")
end = source.index("void OOutputs::tick(int16_t input_motor)")
motor_start = source.index("const static uint8_t MOTOR_VALUES[]")
motor_end = source.index("// Deluxe Upright: Steering Wheel Movement", motor_start)
# Stop before the divider preceding the upright (unrelated rumble) code.
motor_end = source.rfind("// ----", motor_start, motor_end)
state = source[start:end]
motor = source[motor_start:motor_end]
assert state.count("void OOutputs::init()") == 1
assert motor.count("void OOutputs::do_motors(") == 1
assert motor.count("void OOutputs::motor_output(") == 1
Path(sys.argv[2]).write_text(state + "\n" + motor, encoding="utf-8")
