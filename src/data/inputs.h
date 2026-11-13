// What is plugged into the TV's inputs: the names the devices announce over HDMI (HDMI-CEC), by port (Hearth's lib/inputs.ts,
// its pull request 16 by Obednal97).
#pragma once

#include <map>
#include <string>

/** The TV service's device list (com.webos.service.tv.externaldevice/getExternalInputStatusList), asked as root. */
extern const char *CEC_LIST_URI;
/** Port (1: HDMI 1) to the name of the first named device on it, from that call's reply; empty when the reply is not such a list.
 *  A device's port is its portId, or the first digit of its physical address (0x2000: HDMI 2). */
std::map<int, std::string> device_names (const std::string &reply);
