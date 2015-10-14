/**
 * Copyright (C) 2015 Jeeva Kandasamy (jkandasa@gmail.com)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @author Jeeva Kandasamy (jkandasa)
 * @since 0.0.1
 */

/**
 * Water level sensor based on ultrasonic sensor.
 */

#include <SPI.h>
#include <MySensor.h>
#include <NewPing.h>

// Application details
#define APPLICATION_NAME    "Water Level Sensor Node"
#define APPLICATION_VERSION "0.0.1"

//My node Id
#define NODE_ID AUTO

//This node Sensors
#define S_WATER_DISTANCE_ID (int8_t) 1

//Tank water Level
#define LEVEL_TANK_FULL     (int8_t) 22    // Set your tank water FULL LEVEL
#define LEVEL_TANK_EMPTY    (int8_t) 165   // Set your tank water EMPTY LEVEL

//^^^^ which is calculated as 0.7 (165-22 -> 100/142) ~= 0.7

//EEPROM Address
#define E_ADR_LEVEL_CHECK_TIME (int8_t) 15 //in seconds

#define TRIGGER_PIN  5  // Arduino pin tied to trigger pin on the ultrasonic sensor.
#define ECHO_PIN     6  // Arduino pin tied to echo pin on the ultrasonic sensor.
#define MAX_DISTANCE 350 // Maximum distance we want to ping for (in centimeters). Maximum sensor distance is rated at 400-500cm.
#define SLEEP_TIME (unsigned long)1000l*15 // Sleep time between reads (in milliseconds)

void incomingMessage(const MyMessage &message);
void sendWaterLevel();

MySensor gw;
NewPing sonar(TRIGGER_PIN, ECHO_PIN, MAX_DISTANCE); // NewPing setup of pins and maximum distance.
MyMessage msg(S_WATER_DISTANCE_ID, V_VOLUME);
uint8_t lastDist = 0;
uint8_t sendLevelDelayTime = 120;
unsigned long lastSent;
bool isLevelIncreasing = false;

void setup() {
    gw.begin(incomingMessage, NODE_ID, true);
    // Send the sketch version information to the gateway and Controller
    gw.sendSketchInfo(APPLICATION_NAME, APPLICATION_VERSION);
    // Register all sensors to gw (they will be created as child devices)
    gw.present(S_WATER_DISTANCE_ID, S_WATER, "Tank");
    MyMessage commandMsg(S_WATER, V_VAR1);
    sendLevelDelayTime = gw.loadState(E_ADR_LEVEL_CHECK_TIME);
    if (sendLevelDelayTime <= 14) {
        sendLevelDelayTime = 15;
    }

    gw.send(commandMsg.set(sendLevelDelayTime)); //Send current state of delay time
    lastSent = millis();
}

void loop() {
    gw.process();
    //This is to avoid infinite loop, This number will overflow (go back to zero), after approximately 50 days.
    //https://www.arduino.cc/en/reference/millis
    if (millis() <= 3 * 1000l) {
        lastSent = millis();
    } else if ((lastSent + (sendLevelDelayTime * 1000l)) <= millis()) {
        lastSent = millis();
        sendWaterLevel();
    }
}

void sendWaterLevel() {
    uint8_t dist = sonar.ping_cm();
    if (lastDist != 0) {
        if (lastDist > dist) {
            if (!isLevelIncreasing) {
                dist = lastDist;
                isLevelIncreasing = true;
            }
        } else if (lastDist < dist) {
            if (isLevelIncreasing) {
                dist = lastDist;
                isLevelIncreasing = false;
            }
        }
    }

    lastDist = dist;
    float percentage;
    //convert it in to percentage 0~100 %
    float level = 0.7f * (dist - LEVEL_TANK_FULL );
    if (level < 0) {
        percentage = 100.0f + (level * -1);
    } else if (level > 100.0f) {
        percentage = 0.0f;
    } else {
        percentage = 100.0f - level;
    }
    gw.send(msg.set(percentage, 2));
}

void incomingMessage(const MyMessage &message) {
    if (message.type == V_VAR1) {
        if (message.getInt() == 0) {
            sendWaterLevel();
        } else if ((message.getInt() >= 15) && (message.getInt() <= 250)) {
            gw.saveState(E_ADR_LEVEL_CHECK_TIME, (uint8_t) message.getInt());
            sendLevelDelayTime = gw.loadState(E_ADR_LEVEL_CHECK_TIME);
        }
    }
}
