#include "generic.h"

void handleEvent(strMap event) {
    // Add a command for enabling / disabling a blinky output?
    // Add command to dump list of defined io/motors?
    // Way to set the velocity and acceleration of a motor?

    // Iterate through event and print all key/value pairs
    // for (strMap::iterator it = event.begin(); it != event.end(); ++it) {
    //     Serial.print(it->first);
    //     Serial.print(" ");
    //     Serial.println(it->second);
    // }
    String command = event["COMMAND"];
    if (command == "RESET") {
        SysMgr.ResetBoard();

    } else if (command.indexOf("IO") == 0) {
        String IOname = event["IO"];
        if (IOmap.find(IOname) == IOmap.end()) {
            dispatchEvent("WARNING", IOname + " not found");
            return;
        }

        if (command == "IO_SET") {
            IOInfo info = IOmap[IOname];
            if (info.mode == INPUT) {
                dispatchEvent("WARNING", IOname + " is not an output");
                return;
            }
            // Serial.print("IO_SET ");
            // Serial.print(IOname);
            // Serial.print(" ");
            // Serial.println(event["VALUE"]);
            digitalWrite(info.pin, (event["VALUE"] == "1"));

        } else if (command == "IO_GET") {
            strMap data = {
                {"IO", IOname},
                {"VALUE", String(digitalRead(IOmap[IOname].pin))}
            };
            dispatchEvent("IO_STATUS", data);
        }

    } else if (command.indexOf("MOTOR") == 0) {
        String motorName = event["MOTOR"];
        // check if valid motor name
        if (motors.find(motorName) == motors.end()) {
            dispatchEvent("WARNING", motorName + " not found");
            return;
        }
        ClearCore::MotorDriver* motor = motors[motorName];

        if (command == "MOTOR_RESET_ALERTS") {
            motor->ClearAlerts();
        } else if (command == "MOTOR_STATUS") {
            getMotorStatus(motorName);
        } else if (command.indexOf("MOVE") != -1) {
            // check if motor is enabled
            if (!motor->StatusReg().bit.Enabled) {
                dispatchEvent("WARNING", motorName + " is not enabled");
                return;
            }
            // check that a VELOCITY is provided
            if (event.count("VELOCITY") == 0) {
                dispatchEvent("WARNING", "VELOCITY not provided");
                return;
            }
            // check that a ACCELERATION is provided
            if (event.count("ACCELERATION") == 0) {
                dispatchEvent("WARNING", "ACCELERATION not provided");
                return;
            }
            int velocity = event["VELOCITY"].toInt();
            int acceleration = event["ACCELERATION"].toInt();
            motor->VelMax(velocity);
            motor->AccelMax(acceleration);

            if (command == "MOTOR_MOVE_ABSOLUTE") {
                int position = event["POSITION"].toInt();
                // If ACCELERATION is specified, use it, otherwise use the default
                motor->Move(position, MotorDriver::MOVE_TARGET_ABSOLUTE);

            } else if (command == "MOTOR_MOVE_RELATIVE") {
                int distance = event["DISTANCE"].toInt();
                motor->Move(distance, MotorDriver::MOVE_TARGET_REL_END_POSN);

            } else if (command == "MOTOR_MOVE_VELOCITY") {
                int velocity = event["VELOCITY"].toInt();
                motor->MoveVelocity(velocity);
            }
        } else if (command == "MOTOR_STOP_DECEL") {
            motor->MoveStopDecel();
        } else if (command == "MOTOR_STOP_ABRUPT") {
            motor->MoveStopAbrupt();
        }
    }
}


void setup() {
    // Wait for serial to initialize or timeout after 5 seconds.
    setupConnectivity();
    Serial.println("Setup starting");
    setupIO();
    // setupCCIO();
    setupMotors();
    Serial.println("Setup complete");
}

void loop() {
    maintainEthernet();
    // maintainCCIO();
    monitorMotorStatus();
    monitorIO();
    // monitorCCIO();
    events.processInput(handleEvent);
    // status.sendStatus();
}

