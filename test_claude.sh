#!/bin/bash

# Test Claude integration by running Surge XT and sending a test prompt
echo "Starting Surge XT with Claude integration test..."

# Run Surge XT and capture output
./build/surge_xt_products/Surge\ XT.app/Contents/MacOS/Surge\ XT 2>&1 | tee surge_test.log &
SURGE_PID=$!

echo "Surge XT started with PID $SURGE_PID"
echo "Check the GUI and test the Claude integration"
echo "Press Enter to stop and view the log..."
read

kill $SURGE_PID
echo "Surge XT stopped. Checking for parameter mapping in log..."

echo -e "\n=== Parameter Mapping Results ==="
grep -E "(Mapped alias|Could not find parameter|Found in alias map|Applied:|Warning:)" surge_test.log | tail -50

echo -e "\n=== Claude API Response ==="
grep -E "(DEBUG: Response text:|PARAMETERS:)" surge_test.log | tail -20

echo -e "\n=== Parameter Application ==="
grep -E "(Trying to set parameter:|Successfully applied|Parameter changed from)" surge_test.log | tail -20