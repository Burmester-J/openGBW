#include "scale.hpp"
#include <MathBuffer.h>
#include <Encoder.h>
#include <EEPROM.h>

HX711 loadcell;
SimpleKalmanFilter kalmanFilter(0.02, 0.02, 0.01);

// edit encoder files like in https://github.com/PaulStoffregen/Encoder/pull/85/files
Encoder rotaryEncoder(ROTARY_ENCODER_A_PIN, ROTARY_ENCODER_B_PIN); // ROTARY_ENCODER_BUTTON_PIN

#define ABS(a) (((a) > 0.0) ? (a) : ((a) * -1.0))

double scaleWeight = 0; //current weight
bool wakeDisp = false; //wake up display with rotary click
double setWeight = 0; //desired amount of coffee
double setCupWeight = 0; //cup weight set by user
double offset = 0; //stop x grams prios to set weight
bool scaleMode = false; //use as regular scale with timer if true
bool grindMode = true;  //false for impulse to start/stop grinding, true for continuous on while grinding
bool grinderActive = false; //needed for continuous mode
MathBuffer<double, 100> weightHistory;

unsigned long lastAction = 0;
unsigned long scaleLastUpdatedAt = 0;
unsigned long lastTareAt = 0; // if 0, should tare load cell, else represent when it was last tared
bool scaleReady = false;
int scaleStatus = STATUS_EMPTY;
double cupWeightEmpty = 0; //measured actual cup weight
unsigned long startedGrindingAt = 0;
unsigned long finishedGrindingAt = 0;
int encoderDir = -1;
bool greset = false;

bool newOffset = false;

int currentMenuItem = 0;
int currentSetting;
int encoderValue = 0;
int menuItemsCount = 9;
MenuItem menuItems[9] = {
    {1, false, "Manual Grind", 0},
    {2, false, "Cup weight", 1, &setCupWeight},
    {3, false, "Calibrate", 0},
    {4, false, "Offset", 0.1, &offset},
    {5, false, "Scale Mode", 0},
    {6, false, "Grinding Mode", 0},
    {7, false, "Exit", 0},
    {8, false, "Reset", 0},
    {9, false, "Tare", 0}}; // structure is mostly useless for now, plan on making menu easier to customize later

void writeSmallFloat(uint address, float float_value)
{
  if (float_value <= UINT8_MAX)
  {
    u_int8_t int_value = static_cast<u_int8_t>((float_value * 10) + 0.5f); // + 0.5f: round the value
    EEPROM.update(address, int_value);
  }
}

float readSmallFloat(uint address)
{
  uint int_value = EEPROM.read(address);
  return static_cast<float>(int_value) / 10.0;
}

void writeLargeFloat(uint address, float float_value)
{
  float_t value = static_cast<float_t>(float_value);
  EEPROM.put(address, value);
}

float readLargeFloat(uint address)
{
  float_t value;
  EEPROM.get(address, value);
  return value;
}

void writePresetValues()
{
  EEPROM.update(INIT_ADDRESS, 17);
  writeLargeFloat(CALIBRATION_ADDRESS, LOADCELL_SCALE_FACTOR);
  writeSmallFloat(DOSE_ADDRESS, COFFEE_DOSE_WEIGHT);
  writeSmallFloat(OFFSET_ADDRESS, COFFEE_DOSE_OFFSET);
  writeSmallFloat(CUP_ADDRESS, CUP_WEIGHT);
  // EEPROM.update(SCALE_ADDRESS, false);
  // EEPROM.update(GRIND_ADDRESS, true);
}


void grinderToggle()
{
  if(!scaleMode){
    if(grindMode){
      grinderActive = !grinderActive;
      digitalWrite(GRINDER_ACTIVE_PIN, grinderActive);
    }
    else{
      digitalWrite(GRINDER_ACTIVE_PIN, 1);
      delay(100);
      digitalWrite(GRINDER_ACTIVE_PIN, 0);
    }
  }
}


void rotary_onButtonClick()
{
  wakeDisp = 1;
  lastAction = millis();

  if(dispAsleep)
    return;

  if(scaleStatus == STATUS_EMPTY){
    scaleStatus = STATUS_IN_MENU;
    currentMenuItem = 0;
  }
  else if(scaleStatus == STATUS_IN_MENU){
    if(currentMenuItem == 6){
      scaleStatus = STATUS_EMPTY;
      Serial.println("Exited Menu");
    }
    else if (currentMenuItem == 0)
    {
      grinderToggle();
      currentSetting = 0;
      Serial.println("Manual Grind Menu");
    }
    else if (currentMenuItem == 3){
      scaleStatus = STATUS_IN_SUBMENU;
      currentSetting = 3;
      Serial.println("Offset Menu");
    }
    else if (currentMenuItem == 1)
    {
      scaleStatus = STATUS_IN_SUBMENU;
      currentSetting = 1;
      Serial.println("Cup Menu");
    }
    else if (currentMenuItem == 2)
    {
      scaleStatus = STATUS_IN_SUBMENU;
      currentSetting = 2;
      Serial.println("Calibration Menu");
    }
    else if (currentMenuItem == 4)
    {
      scaleStatus = STATUS_IN_SUBMENU;
      currentSetting = 4;
      Serial.println("Scale Mode Menu");
    }
    else if (currentMenuItem == 5)
    {
      scaleStatus = STATUS_IN_SUBMENU;
      currentSetting = 5;
      Serial.println("Grind Mode Menu");
    }
    else if (currentMenuItem == 7)
    {
      scaleStatus = STATUS_IN_SUBMENU;
      currentSetting = 7;
      greset = false;
      Serial.println("Reset Menu");
    }
    else if (currentMenuItem == 8)
    {
      scaleStatus = STATUS_TARING;
      currentSetting = -1;
      lastTareAt = 0;
      Serial.println("Taring");
    }
  }
  else if(scaleStatus == STATUS_IN_SUBMENU){
    if(currentSetting == 3){

      writeSmallFloat(OFFSET_ADDRESS, offset);
      EEPROM.commit();
      
      scaleStatus = STATUS_IN_MENU;
      currentSetting = -1;
    }
    else if (currentSetting == 1)
    {
      if(scaleWeight > 30){       //prevent accidental setting with no cup
        setCupWeight = scaleWeight;
        Serial.println(setCupWeight);
        
        writeSmallFloat(CUP_ADDRESS, setCupWeight);
        EEPROM.commit();
        
        scaleStatus = STATUS_IN_MENU;
        currentSetting = -1;
      }
    }
    else if (currentSetting == 2)
    {
      double newCalibrationValue = readLargeFloat(CALIBRATION_ADDRESS) * (scaleWeight / 100);
      Serial.println(newCalibrationValue);

      writeLargeFloat(CALIBRATION_ADDRESS, newCalibrationValue);
      EEPROM.commit();

      loadcell.set_scale(newCalibrationValue);
      scaleStatus = STATUS_IN_MENU;
      currentSetting = -1;
    }
    else if (currentSetting == 4)
    {
      
      // EEPROM.update(SCALE_ADDRESS, scaleMode);

      scaleStatus = STATUS_IN_MENU;
      currentSetting = -1;
    }
    else if (currentSetting == 5)
    {
      
      // EEPROM.update(GRIND_ADDRESS, grindMode);

      scaleStatus = STATUS_IN_MENU;
      currentSetting = -1;
    }
    else if (currentSetting == 7)
    {
      if(greset){
        setWeight = (double)COFFEE_DOSE_WEIGHT;
        offset = (double)COFFEE_DOSE_OFFSET;
        setCupWeight = (double)CUP_WEIGHT;
        scaleMode = false;
        grindMode = true;

        writePresetValues();
        EEPROM.commit();

        loadcell.set_scale((double)LOADCELL_SCALE_FACTOR);
      }
      
      scaleStatus = STATUS_IN_MENU;
      currentSetting = -1;
    }
  }
}



void rotary_loop()
{
  if (encoderValue != rotaryEncoder.read()) // encoder changed
  {
    wakeDisp = 1;
    lastAction = millis();

    if(dispAsleep)
      return;
    
    if(scaleStatus == STATUS_EMPTY){
        int newValue = rotaryEncoder.read();
        Serial.print("Value: ");

        setWeight += ((float)newValue - (float)encoderValue) / 10 * encoderDir;

        encoderValue = newValue;
        Serial.println(newValue);

        writeSmallFloat(DOSE_ADDRESS, setWeight);
        EEPROM.commit();

      }
    else if(scaleStatus == STATUS_IN_MENU){
      int newValue = rotaryEncoder.read();
      currentMenuItem = (currentMenuItem + (newValue - encoderValue) * encoderDir) % menuItemsCount;
      currentMenuItem = currentMenuItem < 0 ? menuItemsCount + currentMenuItem : currentMenuItem;
      encoderValue = newValue;
      Serial.println(currentMenuItem);
    }
    else if(scaleStatus == STATUS_IN_SUBMENU){
      if(currentSetting == 3){ //offset menu
        int newValue = rotaryEncoder.read();
        Serial.print("Value: ");

        offset += ((float)newValue - (float)encoderValue) * encoderDir / 100;
        encoderValue = newValue;

        if(abs(offset) >= setWeight){
          offset = setWeight;     //prevent nonsensical offsets
        }
      }
      else if(currentSetting == 4){
        scaleMode = !scaleMode;
      }
      else if (currentSetting == 5)
      {
        grindMode = !grindMode;
      }
      else if (currentSetting == 7)
      {
        greset = !greset;
      }
    }
  }
  if (digitalRead(ROTARY_ENCODER_BUTTON_PIN) == LOW) // button pressed
  {
    rotary_onButtonClick();
  }
  if (wakeDisp && ((millis() - lastAction) > 1000))
    wakeDisp = 0;
}

void tareScale() {
  long min=0;
  long max=0;
  long sum=0;
  Serial.println("Taring scale");
  for (int i=0; i<TARE_MEASURES; i++){
    long result = loadcell.read();
    if (i == 0){
      min = result;
      max = result;
    }
    else {
      if (result < min)
        min = result;
      if (result > max)
        max = result;
    }
    sum += result;
  }
  // Serial.println(min);
  // Serial.println(max);
  long range = max-min;
  // Serial.println(range);
  // Serial.println(sum/loadcell.get_scale());
  if (range < TARE_THRESHOLD_COUNTS)
    loadcell.set_offset(sum/TARE_MEASURES);
  lastTareAt = millis();
}

void updateScale() {
  float lastEstimate=0;

  if (lastTareAt == 0) {
    Serial.println("retaring scale");
    Serial.println("current offset");
    Serial.println(offset);
    tareScale();
  }
  if (loadcell.wait_ready_timeout(300)) {
    lastEstimate = kalmanFilter.updateEstimate(loadcell.get_units(5));
    scaleWeight = lastEstimate;
    scaleLastUpdatedAt = millis();
    weightHistory.push(scaleWeight);
    scaleReady = true;
  } else {
    Serial.println("HX711 not found.");
    scaleReady = false;
  }
}



void scaleStatusLoop() {
  double tenSecAvg;
  
  tenSecAvg = weightHistory.averageSince((int64_t)millis() - 10000);
  

  if (ABS(tenSecAvg - scaleWeight) > SIGNIFICANT_WEIGHT_CHANGE) {
    lastAction = millis();
  }

  if (scaleStatus == STATUS_EMPTY) {
    if (((millis() - lastTareAt) > TARE_MIN_INTERVAL)
        && (ABS(scaleWeight) > 0.0) 
        && (tenSecAvg < 3.0)
        && (scaleWeight < 3.0)) {
      // tare if: not tared recently, more than 0.2 away from 0, less than 3 grams total (also works for negative weight)
      lastTareAt = 0;
      scaleStatus = STATUS_TARING;
    }

    if (ABS(weightHistory.minSince((int64_t)millis() - 1000) - setCupWeight) < CUP_DETECTION_TOLERANCE 
        && ABS(weightHistory.maxSince((int64_t)millis() - 1000) - setCupWeight) < CUP_DETECTION_TOLERANCE
        && (lastTareAt != 0)
        && scaleReady)
    {
      // using average over last 500ms as empty cup weight
      Serial.println("Starting grinding");
      cupWeightEmpty = weightHistory.averageSince((int64_t)millis() - 500);
      scaleStatus = STATUS_GRINDING_IN_PROGRESS;
      
      if(!scaleMode){
        newOffset = true;
        startedGrindingAt = millis();
      }
      
      grinderToggle();
      return;
    }
  } else if (scaleStatus == STATUS_GRINDING_IN_PROGRESS) {
    if (!scaleReady) {
      
      grinderToggle();
      scaleStatus = STATUS_GRINDING_FAILED;
    }
    //Serial.printf("Scale mode: %d\n", scaleMode);
    //Serial.printf("Started grinding at: %d\n", startedGrindingAt);
    //Serial.printf("Weight: %f\n", cupWeightEmpty - scaleWeight);
    if (scaleMode && (startedGrindingAt == 0) && ((scaleWeight - cupWeightEmpty) >= 0.1))
    {
      Serial.printf("Started grinding at: %d\n", millis());
      startedGrindingAt = millis();
      return;
    }

    if (((millis() - startedGrindingAt) > MAX_GRINDING_TIME) && !scaleMode) {
      Serial.println("Failed because grinding took too long");
      
      grinderToggle();
      scaleStatus = STATUS_GRINDING_FAILED;
      return;
    }

    if ( ((millis() - startedGrindingAt) > GRINDING_DELAY_TOLERANCE) // started grinding at least 3s ago
          && ((scaleWeight - weightHistory.firstValueOlderThan(millis() - 2000)) < 0.5) // less than a gram has been grinded in the last 2 second
          && !scaleMode) {
      Serial.println("Failed because no change in weight was detected");
      
      grinderToggle();
      scaleStatus = STATUS_GRINDING_FAILED;
      return;
    }

    // if (weightHistory.minSince((int64_t)millis() - 200) < (cupWeightEmpty - CUP_DETECTION_TOLERANCE) 
    //       && !scaleMode) {
    //   Serial.printf("Failed because weight too low, min: %f, min value: %f\n", weightHistory.minSince((int64_t)millis() - 200), CUP_WEIGHT + CUP_DETECTION_TOLERANCE);
      
    //   grinderToggle();
    //   scaleStatus = STATUS_GRINDING_FAILED;
    //   continue;
    // }
    double currentOffset = offset;
    if(scaleMode){
      currentOffset = 0;
    }

    if (weightHistory.maxSince((int64_t)millis() - 200) >= (cupWeightEmpty + setWeight + currentOffset)) {
      Serial.println("Finished grinding");
      finishedGrindingAt = millis();
      
      grinderToggle();
      scaleStatus = STATUS_GRINDING_FINISHED;
      return;
    }
  } else if (scaleStatus == STATUS_GRINDING_FINISHED) {
    double currentWeight = weightHistory.averageSince((int64_t)millis() - 500);
    if (scaleWeight < 5) {
      Serial.println("Going back to empty");
      startedGrindingAt = 0;
      scaleStatus = STATUS_EMPTY;
      return;
    }
    else if ((currentWeight != (setWeight + cupWeightEmpty)) 
              && ((millis() - finishedGrindingAt) > 1500)
              && newOffset)
    {
      offset += (setWeight + cupWeightEmpty - currentWeight);
      if(ABS(offset) >= setWeight){
        offset = COFFEE_DOSE_OFFSET;
      }
      // preferences.begin("scale", false);
      // preferences.putDouble("offset", offset);
      // preferences.end();
      newOffset = false;
    }
  } else if (scaleStatus == STATUS_GRINDING_FAILED) {
    if (scaleWeight >= GRINDING_FAILED_WEIGHT_TO_RESET) {
      Serial.println("Going back to empty");
      scaleStatus = STATUS_EMPTY;
      return;
    }
  }
  else if (scaleStatus == STATUS_TARING) {
    if (lastTareAt != 0) {
      scaleStatus = STATUS_EMPTY;
    }
  }
  rotary_loop();
  delay(50);
}



void setupScale() {
  loadcell.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);

  pinMode(ROTARY_ENCODER_BUTTON_PIN, INPUT); //INPUT_PULLDOWN ?
  pinMode(GRINDER_ACTIVE_PIN, OUTPUT);
  digitalWrite(GRINDER_ACTIVE_PIN, 0);

  EEPROM.begin(256);

  // uninitialized EEPROM
  if (EEPROM.read(INIT_ADDRESS) != 17)
  {
    writePresetValues();
    EEPROM.commit();
  }
  
  double scaleFactor = readLargeFloat(CALIBRATION_ADDRESS); // (double)LOADCELL_SCALE_FACTOR
  setWeight = readSmallFloat(DOSE_ADDRESS); // (double)COFFEE_DOSE_WEIGHT
  offset = readSmallFloat(OFFSET_ADDRESS); // (double)COFFEE_DOSE_OFFSET
  setCupWeight = readSmallFloat(CUP_ADDRESS); // (double)CUP_WEIGHT
  scaleMode = (bool)EEPROM.read(SCALE_ADDRESS); // false
  grindMode = (bool)EEPROM.read(GRIND_ADDRESS); // true
  
  loadcell.set_scale(scaleFactor);

  Serial.println("Scale Setup");
}
