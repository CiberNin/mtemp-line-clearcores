#include "config.h"

EthernetTcpClient client;

// Events
typedef std::map<String, String> strMap;
unsigned long event_count = 0;
unsigned long last_event_dispatch = 0; // When the last event was dispatched.

// Log Levels.
#undef DEBUG // IDK which import defines this.
enum LogLevel { NOTSET,
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    CRITICAL };
const String log_level_strings[] = { "NOTSET", "DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL" };

// Perform initialization of the Ethernet & Serial connections.
void setupConnectivity()
{
    // Ethernet
    EthernetMgr.Setup();
    EthernetMgr.NetmaskIp(netmask);
    EthernetMgr.GatewayIp(gateway);
    EthernetMgr.DnsIp(dns);
    EthernetMgr.LocalIp(local);
    // Serial
    Serial.begin(serial_baud);
}

// Setup IO pin modes
void setupIO()
{
    for (const auto& inputPair : IOmap) {
        pinMode(inputPair.second.pin, inputPair.second.mode);
        Serial.print("IO: ");
        Serial.print(inputPair.first.c_str());
        Serial.print(" set to: ");
        Serial.print(inputPair.second.mode);
        Serial.print(" on pin: ");
        Serial.println(inputPair.second.pin);
    }
}

// Keep track of the number of CCIO-8 boards attached to the system.
uint8_t ccioBoardCount;
// Setup CCIO connector, pin modes, & initialize board count
void setupCCIO()
{
    CcioPort.Mode(Connector::CCIO);
    CcioPort.PortOpen();
    ccioBoardCount = CcioMgr.CcioCount();

    // Setup pin modes
    for (const auto& inputPair : CCIOmap) {
        pinMode(inputPair.second.pin, inputPair.second.mode);
        Serial.print("CCIO: ");
        Serial.print(inputPair.first.c_str());
        Serial.print(" set to: ");
        Serial.print(inputPair.second.mode);
        Serial.print(" on pin: ");
        Serial.println(inputPair.second.pin);
    }
}

// Setup motor velocity & acceleration limits, and enable motors.
void setupMotors()
{
    // Sets the input clocking rate. This normal rate is ideal for ClearPath step and direction applications.
    MotorMgr.MotorInputClocking(MotorManager::CLOCK_RATE_NORMAL);

    // Sets all motor connectors into step and direction mode.
    MotorMgr.MotorModeSet(MotorManager::MOTOR_ALL, Connector::CPM_MODE_STEP_AND_DIR);

    // Sets the max velocity and acceleration for distance move and enable each motor.
    uint8_t i = 0;
    for (const auto& motorPair : motors) {
        motorPair.second->VelMax(velocities[i]);
        motorPair.second->AccelMax(accelerationLimit[i]);
        motorPair.second->EnableRequest(true);
        Serial.print("Motor: ");
        Serial.print(motorPair.first.c_str()); // Assuming you want to print the motor name instead of the index
        Serial.println(" is ready.");
        i++;
    }
}

// Convert a strMap to a JSON string.
String mapToJSON(strMap map)
{
    String json = "{";
    // Iterate through the map, adding each key-value pair to the json string.
    for (auto it = map.begin(); it != map.end(); ++it) {
        String key = it->first;
        key.toUpperCase();
        String value = it->second;
        value.toUpperCase();
        json += "\"" + key + "\":\"" + value + "\"";
        if (std::next(it) != map.end()) {
            json += ",";
        }
    }
    json += "}";
    return json;
}

// Convert a JSON string to a strMap.
// expects strings of the form {"key":"value","key2":"value2"}
// Does not handle nested objects or arrays.
strMap JSONtoMap(String json)
{
    strMap map;
    String key = "";
    String value = "";
    bool in_quotes = false;
    bool in_key = true;
    bool in_value = false;
    for (int i = 0; i < json.length(); i++) {
        char next = json[i];
        if (next == '"') {
            in_quotes = !in_quotes;
        } else if (next == ':' && !in_quotes) {
            in_key = false;
            in_value = true;
        } else if (next == ',' && !in_quotes) {
            in_key = true;
            in_value = false;
            key.toUpperCase();
            value.toUpperCase();
            map[key] = value;
            key = "";
            value = "";
        } else if (in_key && in_quotes) {
            key += next;
        } else if (in_value && in_quotes) {
            value += next;
        }
    }
    if (key.length() > 0 && value.length() > 0) {
        map[key] = value;
    }
    return map;
}

// Dispatch an event to the server.
void dispatchEvent(String event, String data = "", LogLevel log_level = INFO)
{
    strMap evt = {
        // { "id", String(event_count) }, // Will overflow after ~4 billion events.
        { "event", event },
        // { "timestamp", String(millis()) }, // Will overflow after 50 days.
        // { "log_level", log_level_strings[log_level] },
        { "data", data }
    };
    String json = mapToJSON(evt);
    Serial.print("SEND: ");
    Serial.println(json);
    json = json + "\n";
    client.Send(json.c_str());
    event_count++;
}
// Instead of a data atribute, will insert the map attributes.
void dispatchEvent(String event, strMap data, LogLevel log_level = INFO)
{
    strMap evt = {
        // { "id", String(event_count) }, // Will overflow after ~4 billion events.
        { "event", event },
        // { "timestamp", String(millis()) }, // Will overflow after 50 days.
        // { "log_level", log_level_strings[log_level] },
    };
    // Insert the data map into the event map.
    evt.insert(data.begin(), data.end());
    String json = mapToJSON(evt);
    Serial.print("SEND: ");
    Serial.println(json);
    json = json + "\n";
    client.Send(json.c_str());
    event_count++;
}

class Events {
private:
    // Trying out the class so that these variable are colocated instead of being global.
    String incoming_event = "";
    int incoming_event_depth = 0;

public:
    void processInput(void (*callback)(strMap))
    {
        while (client.BytesAvailable() > 0) {
            char next_char = char(client.Read());
            switch (next_char) {
            case '{':
                incoming_event_depth++;
                break;
            case '}':
                incoming_event_depth--;
                if (incoming_event_depth == 0) {
                    incoming_event += '}';
                    incoming_event.toUpperCase();
                    Serial.print("RECV: ");
                    Serial.println(incoming_event);
                    callback(JSONtoMap(incoming_event));
                    incoming_event = "";
                }
                break;
            }
            incoming_event += next_char;
        };
    }
};

Events events;

// For tracking if the board is having performance issues.
class Status {
private:
    unsigned long last_status_dispatch = 0;
    unsigned long status_interval = 5000;
    unsigned long iterations = 0;

public:
    void sendStatus()
    {
        if (millis() - last_status_dispatch > status_interval) {
            strMap status = {
                { "iterations", String(iterations) }
            };
            dispatchEvent("BoardStatus", status, DEBUG);
            last_status_dispatch = millis();
            iterations = 0;
        }
        iterations++;
    };
};

Status status;

// Recover if connection lost or cable unplugged.
void maintainEthernet()
{
    // Physical ethernet cable
    if (!EthernetMgr.PhyLinkActive()) {
        Serial.println("The Ethernet cable is unplugged...");
        client.Close(); // Just to be sure
        while (!EthernetMgr.PhyLinkActive()) { }
        Serial.println("Ethernet cable plugged in");
    }

    if (!client.Connected()) {
        uint32_t connection_lost_timestamp = millis();
        client.Close(); // Just to be sure

        // Immediately stop all motors.
        for (const auto& motorPair : motors) {
            motorPair.second->MoveStopAbrupt();
        }

        // Connect to server
        // TODO: Refactor to handle if ethernet (or anything else) is unplugged while waiting for connection.
        // TODO: Similar change to all other spots where we wait for something.
        Serial.println("Waiting for connection...");
        while (!client.Connected()) {
            client.Connect(remote_ip, remote_port);
            EthernetMgr.Refresh();
        }
        Serial.println("Connection established!");

        // Let the server know we're here.
        dispatchEvent("connection established", String(millis() - connection_lost_timestamp));
        // After connecting, we should send the current state of all IO and motors.
    }
    // Must call these regularly to keep the ethernet manager running.
    client.Flush();
    EthernetMgr.Refresh();
}

// Recover if CCIO-8 link is broken. Also logs a warning if the number of CCIO-8 boards changes.
void maintainCCIO()
{
    // Reestablish the CCIO-8 link if it is broken.
    if (CcioMgr.LinkBroken()) {
        uint32_t connection_lost_timestamp = millis();

        // Immediately stop all motors.
        for (const auto& motorPair : motors) {
            motorPair.second->MoveStopAbrupt();
        }

        dispatchEvent("CCIO expansion board disconnected");
        Serial.println("The CCIO-8 link is broken!");
        // Make sure the CCIO-8 link is established.
        while (CcioMgr.LinkBroken()) { }
        Serial.println("The CCIO-8 link is online again!");
        dispatchEvent("CCIO expansion board reconnected", String(millis() - connection_lost_timestamp));
    }
    // Log a warning if the number of CCIO-8 boards changes.
    uint8_t newBoardCount = CcioMgr.CcioCount();
    if (ccioBoardCount != newBoardCount) {
        dispatchEvent("Warning", "CCIO-8 board count changed at runtime!");
        ccioBoardCount = newBoardCount;
    }
}

std::map<uint32_t, String> motorStatusStrings = {
    { 1 << 0, "AtTargetPosition" },
    { 1 << 1, "StepsActive" },
    { 1 << 2, "AtTargetVelocity" },
    { 1 << 3, "MoveDirection" },
    { 1 << 4, "MotorInFault" },
    { 1 << 5, "Enabled" },
    { 1 << 6, "PositionalMove" },
    { 1 << 9, "AlertsPresent" },
    { 1 << 13, "Triggering" },
    { 1 << 14, "InPositiveLimit" },
    { 1 << 15, "InNegativeLimit" },
    { 1 << 16, "InEStopSensor" },
};

std::map<uint32_t, String> hlfbStateStrings = {
    { 0, "HLFB_DEASSERTED" },
    { 1, "HLFB_ASSERTED" },
    { 2, "HLFB_HAS_MEASUREMENT" },
    { 3, "HLFB_UNKNOWN" },
};

std::map<uint32_t, String> readyStateStrings = {
    { 0, "MOTOR_DISABLED" }, // The motor is not enabled.
    { 1, "MOTOR_ENABLING" }, // The motor is in the process of enabling.
    { 2, "MOTOR_FAULTED" }, // The motor is enabled and not moving, but HLFB is not asserted.
    { 3, "MOTOR_READY" }, // The motor is enabled and HLFB is asserted.
    { 4, "MOTOR_MOVING" }, // The motor is enabled and moving.
};

std::map<uint32_t, String> alertStrings = {
    { 0, "MotionCanceledInAlert" },
    { 1, "MotionCanceledPositiveLimit" },
    { 2, "MotionCanceledNegativeLimit" },
    { 3, "MotionCanceledSensorEStop" },
    { 4, "MotionCanceledMotorDisabled" },
    { 5, "MotorFaulted" },
};

// Dispatch MotorStatus events for any changes to StatusReg, HlfbState, or ReadyState.
// Maybe need to monitor alert register?
void motorStatus()
{
    uint32_t statusRisen;
    uint32_t statusFallen;
    strMap status;

    // Iterate through all motors
    for (const auto& motorPair : motors) {
        status["motor"] = motorPair.first;
        MotorDriver* motor = motorPair.second;
        statusRisen = motor->StatusRegRisen().reg;
        statusFallen = motor->StatusRegFallen().reg;

        // Check for changes in status bits and dispatch events
        for (const auto& field : motorStatusStrings) {
            bool risenBit = statusRisen & field.first;
            bool fallenBit = statusFallen & field.first;

            if (risenBit || fallenBit) {
                status["attribute"] = field.second;
                status["value"] = String(risenBit);
                dispatchEvent("MotorStatus", status);
            }
        }

        // Handle HlfbState attribute separately
        uint32_t hlfbMask = 3 << 7;
        uint32_t readyMask = 7 << 10;

        if ((statusRisen & hlfbMask) || (statusFallen & hlfbMask)) {
            status["attribute"] = "HlfbState";
            uint32_t value = (statusRisen & hlfbMask) >> 7;
            status["value"] = hlfbStateStrings[value];
            dispatchEvent("MotorStatus", status);
        }

        // Handle ReadyState attribute separately
        if ((statusRisen & readyMask) || (statusFallen & readyMask)) {
            status["attribute"] = "ReadyState";
            uint32_t value = (statusRisen & readyMask) >> 10;
            status["value"] = readyStateStrings[value];
            dispatchEvent("MotorStatus", status);

            // Dispatch position when ReadyState changes to any state that's not moving
            if (value != 4) {
                status["attribute"] = "Position";
                status["value"] = String(motor->PositionRefCommanded());
                dispatchEvent("MotorStatus", status);
            }
        }

        // Dispatch velocity when AtTargetVelocity risen
        if (statusRisen & (1 << 2)) {
            status["attribute"] = "Velocity";
            status["value"] = String(motor->VelocityRefCommanded());
            dispatchEvent("MotorStatus", status);
        }
    }
}

// Dispatch event for any IO that has changed since last call.
void monitorIO()
{
    uint32_t statusRisen = InputMgr.InputsRisen().reg;
    uint32_t statusFallen = InputMgr.InputsFallen().reg;
    strMap status;

    for (const auto& field : IOmap) {
        bool risenBit = statusRisen & (1 << field.second.pin);
        bool fallenBit = statusFallen & (1 << field.second.pin);

        if (risenBit || fallenBit) {
            status["attribute"] = field.first;
            status["value"] = String(risenBit);
            dispatchEvent("IOStatus", status);
        }
    }
}

// Dispatch event for any CCIO that has changed since last call.
void monitorCCIO()
{
    uint32_t statusRisen = CcioMgr.InputsRisen();
    uint32_t statusFallen = CcioMgr.InputsFallen();
    strMap status;

    for (const auto& field : CCIOmap) {
        bool risenBit = statusRisen & (1 << field.second.pin);
        bool fallenBit = statusFallen & (1 << field.second.pin);

        if (risenBit || fallenBit) {
            status["attribute"] = field.first;
            status["value"] = String(risenBit);
            dispatchEvent("IOStatus", status);
        }
    }
}

// Dispatch single event with all info on a motor's current status.
void getMotorStatus(String motorName)
{
    if (motors.find(motorName) == motors.end()) {
        dispatchEvent("WARNING", motorName + " not found");
        return;
    }
    ClearCore::MotorDriver* motor = motors[motorName];
    // Would be nice to have a nested structure here?
    // Also maybe just send this full thing every time anything changes? Maybe perf implications.
    // server can dedupe. // IO can be very fast, but motor changes are slower.
    strMap status = {
        { "Motor", motorName },
        { "Position", String(motor->PositionRefCommanded()) },
        { "Velocity", String(motor->VelocityRefCommanded()) },
        { "HlfbState", hlfbStateStrings[(motor->StatusReg().reg & (3 << 7)) >> 7] },
        { "ReadyState", readyStateStrings[(motor->StatusReg().reg & (7 << 10)) >> 10] },

        { "AtTargetPosition", String(motor->StatusReg().bit.AtTargetPosition) },
        { "StepsActive", String(motor->StatusReg().bit.StepsActive) },
        { "AtTargetVelocity", String(motor->StatusReg().bit.AtTargetVelocity) },
        { "MoveDirection", String(motor->StatusReg().bit.MoveDirection) },
        { "MotorInFault", String(motor->StatusReg().bit.MotorInFault) },
        { "Enabled", String(motor->StatusReg().bit.Enabled) },
        { "PositionalMove", String(motor->StatusReg().bit.PositionalMove) },
        { "AlertsPresent", String(motor->StatusReg().bit.AlertsPresent) },
        { "Triggering", String(motor->StatusReg().bit.Triggering) },
        { "InPositiveLimit", String(motor->StatusReg().bit.InPositiveLimit) },
        { "InNegativeLimit", String(motor->StatusReg().bit.InNegativeLimit) },
        { "InEStopSensor", String(motor->StatusReg().bit.InEStopSensor) },

        { "MotionCanceledInAlert", String(motor->AlertReg().bit.MotionCanceledInAlert) },
        { "MotionCanceledPositiveLimit", String(motor->AlertReg().bit.MotionCanceledPositiveLimit) },
        { "MotionCanceledNegativeLimit", String(motor->AlertReg().bit.MotionCanceledNegativeLimit) },
        { "MotionCanceledSensorEStop", String(motor->AlertReg().bit.MotionCanceledSensorEStop) },
        { "MotionCanceledMotorDisabled", String(motor->AlertReg().bit.MotionCanceledMotorDisabled) },
        { "MotorFaulted", String(motor->AlertReg().bit.MotorFaulted) }
    };

    dispatchEvent("MotorStatus", status);
}