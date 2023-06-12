// Has to be undeffed do to std library like map conflicting min/max vs arduino
#undef min
#undef max
#include <map>

#include "ClearCore.h"
#include "EthernetManager.h"
#include "EthernetTcpClient.h"


IpAddress local = IpAddress(192, 168, 84, 209);
IpAddress remote_ip = IpAddress(192, 168, 84, 208);
int remote_port = 8888;
IpAddress netmask = IpAddress(255, 255, 255, 0);
IpAddress gateway = IpAddress(192, 168, 84, 1);
IpAddress dns = IpAddress(192, 168, 84, 1);
int serial_baud = 9600;

// mapping motor names to their clearcore driver
std::map<String, MotorDriver*> motors = {
    {"60IN TRANSFER CONVEYOR", &ConnectorM0},
    {"UPPER LIFT CONVEYOR", &ConnectorM1},
    {"LOWER LIFT CONVEYOR", &ConnectorM2},
    {"54IN TRANSFER CONVEYOR", &ConnectorM3}
};

// mapping io names to their pin
// This is direct pin access. Clearcore provides a more abstracted interface,
// but it's not as good when you want to iterate over all the pins.
// Especially since they are covered by several different classes which makes them a pain to work with as a collection.
struct IOInfo {
    int pin;
    PinMode mode;
};

std::map<String, IOInfo> IOmap = {
    {"RAISE LIFT", {IO0, OUTPUT}},
    {"ENABLE BATCH WARNING LIGHT", {IO1, OUTPUT}},
    {"ASMLASTPART", {IO2, OUTPUT}},
    {"ASMMOVESDONE", {IO3, OUTPUT}},
    // {"", {IO4, INPUT}},
    // {"", {IO5, INPUT}},
    {"MTEMP MOTOR ON", {DI6, INPUT}},
    {"MTEMP DOOR OPEN", {DI7, INPUT}},
    {"ASM REQUESTS INDEX", {DI8, INPUT}},
    {"LIFT IS UP", {A9, INPUT}},
    {"LIFT IS DOWN", {A10, INPUT}},
    // {"", {A11, INPUT}},
    // {"", {A12, INPUT}}
};

// Specify which ClearCore serial COM port is connected to the "COM IN" port
// of the CCIO-8 board. COM-1 may also be used.
#define CcioPort ConnectorCOM0

// mapping ccio expansion board pins
std::map<String, IOInfo> CCIOmap = {
    // {"", {CCIOA0, INPUT}},
    // {"", {CCIOA1, INPUT}},
    // {"", {CCIOA2, INPUT}},
    // {"", {CCIOA3, INPUT}},
    // {"", {CCIOA4, INPUT}},
    // {"", {CCIOA5, INPUT}},
    // {"", {CCIOA6, INPUT}},
    // {"", {CCIOA7, INPUT}},

    // {"", {CCIOB0, INPUT}},
    // {"", {CCIOB1, INPUT}},
    // {"", {CCIOB2, INPUT}},
    // {"", {CCIOB3, INPUT}},
    // {"", {CCIOB4, INPUT}},
    // {"", {CCIOB5, INPUT}},
    // {"", {CCIOB6, INPUT}},
    // {"", {CCIOB7, INPUT}},

    // {"", {CCIOC0, INPUT}},
    // {"", {CCIOC1, INPUT}},
    // {"", {CCIOC2, INPUT}},
    // {"", {CCIOC3, INPUT}},
    // {"", {CCIOC4, INPUT}},
    // {"", {CCIOC5, INPUT}},
    // {"", {CCIOC6, INPUT}},
    // {"", {CCIOC7, INPUT}},

    // {"", {CCIOD0, INPUT}},
    // {"", {CCIOD1, INPUT}},
    // {"", {CCIOD2, INPUT}},
    // {"", {CCIOD3, INPUT}},
    // {"", {CCIOD4, INPUT}},
    // {"", {CCIOD5, INPUT}},
    // {"", {CCIOD6, INPUT}},
    // {"", {CCIOD7, INPUT}},

    // {"", {CCIOE0, INPUT}},
    // {"", {CCIOE1, INPUT}},
    // {"", {CCIOE2, INPUT}},
    // {"", {CCIOE3, INPUT}},
    // {"", {CCIOE4, INPUT}},
    // {"", {CCIOE5, INPUT}},
    // {"", {CCIOE6, INPUT}},
    // {"", {CCIOE7, INPUT}},

    // {"", {CCIOF0, INPUT}},
    // {"", {CCIOF1, INPUT}},
    // {"", {CCIOF2, INPUT}},
    // {"", {CCIOF3, INPUT}},
    // {"", {CCIOF4, INPUT}},
    // {"", {CCIOF5, INPUT}},
    // {"", {CCIOF6, INPUT}},
    // {"", {CCIOF7, INPUT}},

    // {"", {CCIOG0, INPUT}},
    // {"", {CCIOG1, INPUT}},
    // {"", {CCIOG2, INPUT}},
    // {"", {CCIOG3, INPUT}},
    // {"", {CCIOG4, INPUT}},
    // {"", {CCIOG5, INPUT}},
    // {"", {CCIOG6, INPUT}},
    // {"", {CCIOG7, INPUT}},

    // {"", {CCIOH0, INPUT}},
    // {"", {CCIOH1, INPUT}},
    // {"", {CCIOH2, INPUT}},
    // {"", {CCIOH3, INPUT}},
    // {"", {CCIOH4, INPUT}},
    // {"", {CCIOH5, INPUT}},
    // {"", {CCIOH6, INPUT}},
    // {"", {CCIOH7, INPUT}}
};