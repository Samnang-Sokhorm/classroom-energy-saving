#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

// Timing constants
const unsigned long emptyDelay = 15000;        // 15 seconds
const unsigned long relayDuration = 30000;      // 60 seconds
const int DETECTION_DELAY = 300;                // Debounce delay

// Pin definitions
const int IR_IN_PIN = 13;    // Entrance sensor
const int IR_OUT_PIN = 4;     // Exit sensor
const int PIR1_PIN = A0;      // First PIR sensor
const int PIR2_PIN = A1;      // Second PIR sensor

// Relay pins
const int relay_LED_1 = 5;
const int relay_LED_2 = 6;
const int relay_LED_3 = 7;
const int relay_LED_4 = 8;
const int relay_FAN_1 = 9;
const int relay_FAN_2 = 10;
const int relay_FAN_3 = 12;
const int relay_FAN_4 = 11;
const int relay_control = 3;  // Switch 9 relay

// State variables
int peopleCount = 0;
bool irInState = false;
bool irOutState = false;
bool irInPrevious = false;
bool irOutPrevious = false;
unsigned long lastDetectionTime = 0;

// PIR states
bool pir1State = false;
bool pir2State = false;
bool motionDetected = false;
unsigned long lastMotionTime = 0;

// Timer variables
unsigned long roomEmptyTimer = 0;
bool emptyTimerRunning = false;

// Relay timer variables
unsigned long fan1StartTime = 0;
unsigned long fan3StartTime = 0;
bool fan1Active = false;
bool fan3Active = false;
bool pirFanControl = true;     // For fan3 PIR control

// Bluetooth command variables
int data = 0;

// LED states (true = ON, false = OFF)
bool led1State = true;
bool led2State = true;
bool led3State = true;
bool led4State = true;

void setup() {
  Serial.begin(9600);
  
  // Initialize pins
  pinMode(IR_IN_PIN, INPUT);
  pinMode(IR_OUT_PIN, INPUT);
  pinMode(PIR1_PIN, INPUT);
  pinMode(PIR2_PIN, INPUT);
  
  // Initialize relay pins
  pinMode(relay_LED_1, OUTPUT);
  pinMode(relay_LED_2, OUTPUT);
  pinMode(relay_LED_3, OUTPUT);
  pinMode(relay_LED_4, OUTPUT);
  pinMode(relay_FAN_1, OUTPUT);
  pinMode(relay_FAN_2, OUTPUT);
  pinMode(relay_FAN_3, OUTPUT);
  pinMode(relay_FAN_4, OUTPUT);
  pinMode(relay_control, OUTPUT);
  
  // Initialize LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Smart Class");
  lcd.setCursor(0, 1);
  lcd.print("System Ready");
  delay(2000);
  lcd.clear();
  
  // Set all relays to OFF (assuming LOW = OFF)
  digitalWrite(relay_LED_1, LOW);
  digitalWrite(relay_LED_2, LOW);
  digitalWrite(relay_LED_3, LOW);
  digitalWrite(relay_LED_4, LOW);
  digitalWrite(relay_FAN_1, LOW);
  digitalWrite(relay_FAN_2, LOW);
  digitalWrite(relay_FAN_3, LOW);
  digitalWrite(relay_FAN_4, LOW);
  digitalWrite(relay_control, HIGH);  // Control relay starts ON
}

void loop() {
  // Read all sensors
  readSensors();
  
  // Handle Bluetooth commands
  handleBluetoothCommands();
  
  // Update people count based on IR sensors
  updatePeopleCount();
  
  // Update motion detection
  updateMotionDetection();
   Serial.print("Data: ");
  Serial.print(data);
  Serial.print("  PIR1: ");
  Serial.print(pir1State);
  Serial.print("  PIR2: ");
  Serial.println(pir2State);
  
  // Control room based on occupancy
  if (peopleCount > 0) {
    handleOccupiedRoom();
  } else {
    handleEmptyRoom();
  }
  
  // Update LCD display
  updateLCD();
  
  // Update previous states
  irInPrevious = irInState;
  irOutPrevious = irOutState;
}

void readSensors() {
  irInState = digitalRead(IR_IN_PIN);
  irOutState = digitalRead(IR_OUT_PIN);
  pir1State = digitalRead(PIR1_PIN);  // Fixed: using digitalRead on analog pins is okay
  pir2State = digitalRead(PIR2_PIN);
}

void handleBluetoothCommands() {
  while (Serial.available() > 0) {
    data = Serial.read();
    
    // LED controls (A-D for ON, a-d for OFF)
    if (data == 65) led1State = true;   // 'A' - LED1 ON
    if (data == 97) led1State = false;   // 'a' - LED1 OFF
    
    if (data == 66) led2State = true;   // 'B' - LED2 ON
    if (data == 98) led2State = false;   // 'b' - LED2 OFF
    
    if (data == 67) led3State = true;   // 'C' - LED3 ON
    if (data == 99) led3State = false;   // 'c' - LED3 OFF
    
    if (data == 68) led4State = true;   // 'D' - LED4 ON
    if (data == 100) led4State = false;  // 'd' - LED4 OFF
    
    // Fan controls (E-H for ON, e-h for OFF)
    if (data == 69) {  // 'E' - Fan1 ON
      digitalWrite(relay_FAN_1, HIGH);
      fan1Active = true;
      fan1StartTime = millis();
    }
    if (data == 101) { // 'e' - Fan1 OFF
      digitalWrite(relay_FAN_1, LOW);
      fan1Active = false;
    }
    
    if (data == 70) {  // 'F' - Fan2 ON
      digitalWrite(relay_FAN_2, HIGH);
    }
    if (data == 102) { // 'f' - Fan2 OFF
      digitalWrite(relay_FAN_2, LOW);
    }
    
    if (data == 71) {  // 'G' - Fan3 ON
      digitalWrite(relay_FAN_3, HIGH);
      fan3Active = true;
      fan3StartTime = millis();
      pirFanControl = true;  // Re-enable PIR control
    }
    if (data == 103) { // 'g' - Fan3 OFF
      digitalWrite(relay_FAN_3, LOW);
      fan3Active = false;
      pirFanControl = false; // Disable PIR control when manually turned off
    }
    
    if (data == 72) {  // 'H' - Fan4 ON
      digitalWrite(relay_FAN_4, HIGH);
    }
    if (data == 104) { // 'h' - Fan4 OFF
      digitalWrite(relay_FAN_4, LOW);
    }
    
    // Control relay (Switch 9)
    if (data == 73) {  // 'I' - Control relay ON
      digitalWrite(relay_control, HIGH);
    }
    if (data == 105) { // 'i' - Control relay OFF
      digitalWrite(relay_control, LOW);
    }
    
    // Debug output
    Serial.print("Command: ");
    Serial.println(data);
  }
}

void updatePeopleCount() {
  // Person exited
  if (irOutState == HIGH && irOutPrevious == LOW) {
    if (millis() - lastDetectionTime > DETECTION_DELAY) {
      if (peopleCount > 0) {
        peopleCount--;
      }
      lastDetectionTime = millis();
      Serial.print("Person EXITED - Count: ");
      Serial.println(peopleCount);
    }
  }
  
  // Person entered
  if (irInState == HIGH && irInPrevious == LOW) {
    if (millis() - lastDetectionTime > DETECTION_DELAY) {
      peopleCount++;
      lastDetectionTime = millis();
      Serial.print("Person ENTERED - Count: ");
      Serial.println(peopleCount);
    }
  }
}

void updateMotionDetection() {
  // Check for motion from either PIR sensor
  if (pir1State == HIGH || pir2State == HIGH) {
    motionDetected = true;
    lastMotionTime = millis();
    emptyTimerRunning = false;  // Cancel empty room timer
  } else {
    // No motion currently, check if enough time has passed
    if (millis() - lastMotionTime > 1000) {  // 1 second debounce
      motionDetected = false;
    }
  }
}

void handleOccupiedRoom() {
  // Turn on LEDs (unless manually turned off)
  digitalWrite(relay_LED_1, led1State ? HIGH : LOW);
  digitalWrite(relay_LED_2, led2State ? HIGH : LOW);
  digitalWrite(relay_LED_3, led3State ? HIGH : LOW);
  digitalWrite(relay_LED_4, led4State ? HIGH : LOW);
  
  // Handle PIR-controlled fans
  handlePIRFans();
  
  // Handle automatic fan timers
  handleFanTimers();
  
  // Check for room empty condition
  if (!motionDetected) {
    if (!emptyTimerRunning) {
      roomEmptyTimer = millis();
      emptyTimerRunning = true;
    }
    
    // If no motion for emptyDelay, reset room
    if (emptyTimerRunning && (millis() - roomEmptyTimer > emptyDelay)) {
      resetRoom();
    }
  }
}

void handlePIRFans() {
  // Fan 3 controlled by PIR1
  if (pir1State == HIGH && pirFanControl && peopleCount > 0) {
    digitalWrite(relay_FAN_3, HIGH);
    fan3Active = true;
    fan3StartTime = millis();
  }
  
  // Fan 1 controlled by PIR2
  if (pir2State == HIGH && peopleCount > 0) {
    digitalWrite(relay_FAN_1, HIGH);
    fan1Active = true;
    fan1StartTime = millis();
  }
}

void handleFanTimers() {
  // Check Fan 1 timer
  if (fan1Active && (millis() - fan1StartTime >= relayDuration)) {
    digitalWrite(relay_FAN_1, LOW);
    fan1Active = false;
    Serial.println("Fan 1 auto OFF");
  }
  
  // Check Fan 3 timer
  if (fan3Active && (millis() - fan3StartTime >= relayDuration)) {
    digitalWrite(relay_FAN_3, LOW);
    fan3Active = false;
    Serial.println("Fan 3 auto OFF");
  }
}

void handleEmptyRoom() {
  // Turn everything OFF
  digitalWrite(relay_LED_1, LOW);
  digitalWrite(relay_LED_2, LOW);
  digitalWrite(relay_LED_3, LOW);
  digitalWrite(relay_LED_4, LOW);
  digitalWrite(relay_FAN_1, LOW);
  digitalWrite(relay_FAN_2, LOW);
  digitalWrite(relay_FAN_3, LOW);
  digitalWrite(relay_FAN_4, LOW);
  
  // Reset states
  fan1Active = false;
  fan3Active = false;
  emptyTimerRunning = false;
  
  // Keep control relay on if needed
  digitalWrite(relay_control, HIGH);
}

void resetRoom() {
  Serial.println("Room empty - Resetting system");
  peopleCount = 0;
  
  digitalWrite(relay_LED_1, LOW);
  digitalWrite(relay_LED_2, LOW);
  digitalWrite(relay_LED_3, LOW);
  digitalWrite(relay_LED_4, LOW);
  digitalWrite(relay_FAN_1, LOW);
  digitalWrite(relay_FAN_2, LOW);
  digitalWrite(relay_FAN_3, LOW);
  digitalWrite(relay_FAN_4, LOW);
  
  fan1Active = false;
  fan3Active = false;
  emptyTimerRunning = false;
}

void updateLCD() {
  lcd.setCursor(0, 0);
  lcd.print("People Inside:  ");
  
  lcd.setCursor(0, 1);
  lcd.print("Count: ");
  lcd.print(peopleCount);
  lcd.print("   ");  // Clear any leftover digits
}