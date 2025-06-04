#!/bin/bash

# Arduino CLI path from VS Code extension
ARDUINO_CLI="/c/Users/coverbeck/.vscode/extensions/vscode-arduino.vscode-arduino-community-0.7.2-win32-x64/assets/platform/win32-x64/arduino-cli/arduino-cli.exe"

# Board FQBN
BOARD="ClearCore:sam:clearcore"

echo "Building BeforeWM sketch..."
"$ARDUINO_CLI" compile --fqbn "$BOARD" BeforeWM/BeforeWM.ino

echo ""
echo "Building AfterWM sketch..."
"$ARDUINO_CLI" compile --fqbn "$BOARD" AfterWM/AfterWM.ino

echo ""
echo "Build complete!"