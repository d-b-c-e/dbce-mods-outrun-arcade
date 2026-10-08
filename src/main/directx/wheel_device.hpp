#pragma once
#include <string>
#include <vector>

namespace forcefeedback {
struct WheelDevice { std::string name, guid; };
// Read-only enumeration; never opens, acquires or creates a force effect.
std::vector<WheelDevice> enumerate_wheels();
// Startup-only selection; changing the menu choice requires restart.
void configure_guid(const char* saved);
}
