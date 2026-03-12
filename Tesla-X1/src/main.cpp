#include <Arduino.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>
#include <Wire.h>
#include "config.h"

struct DrivingData { float angle; int power; };
struct FeedbackData { bool emergencyBrakeActive; float distance; };

// Global Variables
char currentGear = 'D';
bool isSafetyActive = false;  
bool isBuzzerActive = false;
DrivingData driveCmd = {0, 0};
float currentSmoothPower = 0;
volatile uint32_t globalDistance = 999;

void TaskMotorControl(void *pvParameters);
void onI2CRecv(int bytes);

void onDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
    // 0 = Mode, 1 = Safety, 2 = Driving
    uint8_t type = incomingData[0];

    if (type == 0) {
        currentGear = (char)incomingData[1];
        Serial.printf("Gear Changed to: %c\n", currentGear);
    } 
    else if (type == 1) {
        isSafetyActive = (bool)incomingData[1];
        Serial.printf("Safety Mode: %s\n", isSafetyActive ? "ON" : "OFF");
    }
    else if (type == 2) {
        memcpy(&driveCmd, &incomingData[1], sizeof(DrivingData));
        // Serial.printf("[RX] Type 2 | Angle: %.2f | Power: %d\n", driveCmd.angle, driveCmd.power);
    }else if (type == 3) {
        // For Buzzer on car, not implemented yet
        isBuzzerActive = (bool)incomingData[1];
        Serial.printf("Buzzer: %s\n", isBuzzerActive ? "ON" : "OFF");
    }
}

void BuzzerControl(bool isBuzzerActive) {
    if (isBuzzerActive) {
        digitalWrite(BUZZER_PIN, HIGH);
    } else {
        digitalWrite(BUZZER_PIN, LOW);
    }
}

void setup() {

    Serial.begin(115200);

    Serial.printf("Before Wire.begin -> SDA: %d, SCL: %d\n", digitalRead(PIN_SDA), digitalRead(PIN_SCL));
    bool success = Wire.begin(0x08);  //? First Parameter is only I2C Address
    if (!success) {
        Serial.println("I2C Slave Init Failed - Check Pins/Pull-ups");
    }

    Wire.onReceive(onI2CRecv);
    WiFi.mode(WIFI_STA);
	
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);

	Serial.print("ESP32 Board MAC Address:  ");
  	Serial.println(WiFi.macAddress());

    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
	if (esp_now_init() != ESP_OK) return;
    esp_now_register_recv_cb(onDataRecv);

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, REMOTE_ADDR, 6);
    peerInfo.channel = 1;
    peerInfo.encrypt = false; // For simplicity, no encryption
    esp_now_add_peer(&peerInfo);

    xTaskCreatePinnedToCore(TaskMotorControl, "MotorTask", 10000, NULL, 1, NULL, 1);
}

void TaskMotorControl(void *pvParameters) {
    ledcSetup(0, PWM_FREQ, PWM_RES); ledcAttachPin(M1_PWM, 0);
    ledcSetup(1, PWM_FREQ, PWM_RES); ledcAttachPin(M2_PWM, 1);
    pinMode(M1_IN1, OUTPUT); pinMode(M1_IN2, OUTPUT);
    pinMode(M2_IN1, OUTPUT); pinMode(M2_IN2, OUTPUT);

    for (;;) {

        int distance = globalDistance;
        
        BuzzerControl(isBuzzerActive);
        if (distance > 0 && distance <= LOG_DISTANCE) {
            Serial.printf("[LOG] Object at: %d cm\n", distance);
        }

        bool obstacle = (distance > 1 && distance < STOP_DISTANCE);
        bool emergency = (isSafetyActive && obstacle && (currentGear == 'D' || currentGear == 'S'));
        
        //! Please Debug for case that can't back for when car in safy mode
        
        if (emergency) {
            digitalWrite(M1_IN1, HIGH); digitalWrite(M1_IN2, HIGH);
            digitalWrite(M2_IN1, HIGH); digitalWrite(M2_IN2, HIGH);
            ledcWrite(0, 0); ledcWrite(1, 0);
            currentSmoothPower = 0;

            FeedbackData fb = {true, distance};
            esp_now_send(REMOTE_ADDR, (uint8_t *) &fb, sizeof(fb));
        }
        else if (currentGear != 'N') {
            float targetPWM = map(driveCmd.power, 0, 100, 0, 255); // Map power to target range
            
            if (currentGear == 'S') {
                currentSmoothPower = targetPWM;
            } else {
                if (currentSmoothPower < targetPWM) currentSmoothPower += ACCEL_STEP;
                else if (currentSmoothPower > targetPWM) currentSmoothPower -= BRAKE_STEP;
                currentSmoothPower = constrain(currentSmoothPower, 0, 255);
            }

            float leftSpeed = currentSmoothPower, rightSpeed = currentSmoothPower;
            if (driveCmd.angle > 0) {
				rightSpeed *= (1.0 - (driveCmd.angle / 90.0));
			}
            else if (driveCmd.angle < 0) {
				leftSpeed *= (1.0 - (abs(driveCmd.angle) / 90.0));
			}

            if (currentGear == 'R') {
                digitalWrite(M1_IN1, LOW); digitalWrite(M1_IN2, HIGH);
                digitalWrite(M2_IN1, LOW); digitalWrite(M2_IN2, HIGH);
            } else {
                digitalWrite(M1_IN1, HIGH); digitalWrite(M1_IN2, LOW);
                digitalWrite(M2_IN1, HIGH); digitalWrite(M2_IN2, LOW);
            }

			Serial.printf("[SPEED] L = %.2f; R = %.2f \n", leftSpeed, rightSpeed);
            ledcWrite(0, (int)leftSpeed); ledcWrite(1, (int)rightSpeed);
        }
        else {
            ledcWrite(0, 0); ledcWrite(1, 0);
            digitalWrite(M1_IN1, LOW); digitalWrite(M1_IN2, LOW);
            digitalWrite(M2_IN1, LOW); digitalWrite(M2_IN2, LOW);
            currentSmoothPower = 0;
        }
        vTaskDelay(5 / portTICK_PERIOD_MS);
    }
}

void onI2CRecv(int bytes) {
    if (bytes == 4) {
        uint8_t buf[4];
        for (int i = 0; i < 4; i++) buf[i] = Wire.read();
        
        uint32_t tempDist;
        memcpy(&tempDist, buf, 4); 
        globalDistance = tempDist;
        
    }
}

void loop() { vTaskDelay(1000 / portTICK_PERIOD_MS); }