#include "scale.hpp"
#include <MathBuffer.h>
#include <Encoder.h>
#include <EEPROM.h>

HX711 loadcell;
SimpleKalmanFilter kalmanFilter(0.02, 0.02, 0.01);

// edit encoder files like in https://github.com/PaulStoffregen/Encoder/pull/85/files
Encoder* rotaryEncoder;

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
int menuItemsCount = 10;
MenuItem menuItems[10] = {
    {MENU_ITEM_MANUAL_GRIND, false, "Manual Grind", 0},
    {MENU_ITEM_CUP_WEIGHT, false, "Cup weight", 1, &setCupWeight},
    {MENU_ITEM_CALIBRATE, false, "Calibrate", 0},
    {MENU_ITEM_OFFSET, false, "Offset", 0.1, &offset},
    {MENU_ITEM_SCALE_MODE, false, "Scale Mode", 0},
    {MENU_ITEM_GRINDING_MODE, false, "Grinding Mode", 0},
    {MENU_ITEM_RESET, false, "Reset", 0},
    {MENU_ITEM_EXIT, false, "Exit", 0},
    {MENU_ITEM_TARE, false, "Tare", 0}}; // structure is mostly useless for now, plan on making menu easier to customize later

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
  EEPROM.update(SCALE_ADDRESS, false);
  EEPROM.update(GRIND_ADDRESS, true);
}

int encoderRead()
{
  int value = rotaryEncoder->read();
  if (value > 0) {
    return (value + 2) / 4;
  } else if (value < 0) {
    return (value - 2) / 4;
  } else {
    return 0;
  }
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
    currentMenuItem = MENU_ITEM_MANUAL_GRIND;
  }
  else if(scaleStatus == STATUS_IN_MENU){
    // only commit to EEPROM when exiting menu!
    if(currentMenuItem == MENU_ITEM_EXIT){
      scaleStatus = STATUS_EMPTY;

      EEPROM.commit();

      Serial.println("Exited Menu");
    }
    else if (currentMenuItem == MENU_ITEM_MANUAL_GRIND)
    {
      grinderToggle();
      currentSetting = MENU_ITEM_MANUAL_GRIND;
      Serial.println("Manual Grind Menu");
    }
    else if (currentMenuItem == MENU_ITEM_OFFSET){
      scaleStatus = STATUS_IN_SUBMENU;
      currentSetting = MENU_ITEM_OFFSET;
      Serial.println("Offset Menu");
    }
    else if (currentMenuItem == MENU_ITEM_CUP_WEIGHT)
    {
      scaleStatus = STATUS_IN_SUBMENU;
      currentSetting = MENU_ITEM_CUP_WEIGHT;
      Serial.println("Cup Menu");
    }
    else if (currentMenuItem == MENU_ITEM_CALIBRATE)
    {
      scaleStatus = STATUS_IN_SUBMENU;
      currentSetting = MENU_ITEM_CALIBRATE;
      Serial.println("Calibration Menu");
    }
    else if (currentMenuItem == MENU_ITEM_SCALE_MODE)
    {
      scaleStatus = STATUS_IN_SUBMENU;
      currentSetting = MENU_ITEM_SCALE_MODE;
      Serial.println("Scale Mode Menu");
    }
    else if (currentMenuItem == MENU_ITEM_GRINDING_MODE)
    {
      scaleStatus = STATUS_IN_SUBMENU;
      currentSetting = MENU_ITEM_GRINDING_MODE;
      Serial.println("Grind Mode Menu");
    }
    else if (currentMenuItem == MENU_ITEM_RESET)
    {
      scaleStatus = STATUS_IN_SUBMENU;
      currentSetting = MENU_ITEM_RESET;
      greset = false;
      Serial.println("Reset Menu");
    }
    else if (currentMenuItem == MENU_ITEM_TARE) 
    {
      scaleStatus = STATUS_TARING;
      currentSetting = MENU_ITEM_NONE;
      lastTareAt = 0;
      Serial.println("Taring");
    }
  }
  else if(scaleStatus == STATUS_IN_SUBMENU){
    if(currentSetting == MENU_ITEM_OFFSET){

      writeSmallFloat(OFFSET_ADDRESS, offset);
      
      scaleStatus = STATUS_IN_MENU;
      currentSetting = MENU_ITEM_NONE;
    }
    else if (currentSetting == MENU_ITEM_CUP_WEIGHT)
    {
      if(scaleWeight > 30){       //prevent accidental setting with no cup
        setCupWeight = scaleWeight;
        Serial.println(setCupWeight);
        
        writeSmallFloat(CUP_ADDRESS, setCupWeight);
        
        scaleStatus = STATUS_IN_MENU;
        currentSetting = MENU_ITEM_NONE;
      }
    }
    else if (currentSetting == MENU_ITEM_CALIBRATE)
    {
      double newCalibrationValue = readLargeFloat(CALIBRATION_ADDRESS) * (scaleWeight / 100);
      Serial.println(newCalibrationValue);

      writeLargeFloat(CALIBRATION_ADDRESS, newCalibrationValue);

      loadcell.set_scale(newCalibrationValue);
      scaleStatus = STATUS_IN_MENU;
      currentSetting = MENU_ITEM_NONE;
    }
    else if (currentSetting == MENU_ITEM_SCALE_MODE)
    {
      
      EEPROM.update(SCALE_ADDRESS, scaleMode);

      scaleStatus = STATUS_IN_MENU;
      currentSetting = MENU_ITEM_NONE;
    }
    else if (currentSetting == MENU_ITEM_GRINDING_MODE)
    {
      
      EEPROM.update(GRIND_ADDRESS, grindMode);

      scaleStatus = STATUS_IN_MENU;
      currentSetting = MENU_ITEM_NONE;
    }
    else if (currentSetting == MENU_ITEM_RESET)
    {
      if(greset){
        setWeight = (double)COFFEE_DOSE_WEIGHT;
        offset = (double)COFFEE_DOSE_OFFSET;
        setCupWeight = (double)CUP_WEIGHT;
        scaleMode = false;
        grindMode = true;

        writePresetValues();

        loadcell.set_scale((double)LOADCELL_SCALE_FACTOR);
      }
      
      scaleStatus = STATUS_IN_MENU;
      currentSetting = MENU_ITEM_NONE;
    }
  }
  delay(50);
}



void rotary_loop()
{
  if (encoderValue != encoderRead()) // encoder changed
  {
    wakeDisp = 1;
    lastAction = millis();

    if(dispAsleep)
      return;
    
    if(scaleStatus == STATUS_EMPTY){
        int newValue = encoderRead();
        Serial.print("Value: ");

        setWeight += ((float)newValue - (float)encoderValue) / 10 * encoderDir;

        encoderValue = newValue;
        Serial.println(newValue);

        writeSmallFloat(DOSE_ADDRESS, setWeight);
      }
    else if(scaleStatus == STATUS_IN_MENU){
      int newValue = encoderRead();
      currentMenuItem = (currentMenuItem + (newValue - encoderValue) * encoderDir) % menuItemsCount;
      currentMenuItem = currentMenuItem < 0 ? menuItemsCount + currentMenuItem : currentMenuItem;
      encoderValue = newValue;
      Serial.println(currentMenuItem);
    }
    else if(scaleStatus == STATUS_IN_SUBMENU){
      if(currentSetting == MENU_ITEM_OFFSET){ //offset menu
        int newValue = encoderRead();
        Serial.print("Value: ");

        offset += ((float)newValue - (float)encoderValue) * encoderDir / 100;
        encoderValue = newValue;

        if(abs(offset) >= setWeight){
          offset = setWeight;     //prevent nonsensical offsets
        }
      }
      else if(currentSetting == MENU_ITEM_SCALE_MODE){
        int newValue = encoderRead();

        scaleMode = newValue % 2;

        encoderValue = newValue;
      }
      else if (currentSetting == MENU_ITEM_GRINDING_MODE)
      {
        int newValue = encoderRead();

        grindMode = newValue % 2;

        encoderValue = newValue;
      }
      else if (currentSetting == MENU_ITEM_RESET)
      {
        int newValue = encoderRead();

        greset = newValue % 2;

        encoderValue = newValue;
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
  Serial.println(range);
  Serial.println(sum/loadcell.get_scale());
  if (range < TARE_THRESHOLD_COUNTS){
    loadcell.set_offset(sum/TARE_MEASURES);
    lastTareAt = millis();
  }
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

    // if (ABS(weightHistory.minSince((int64_t)millis() - 1000) - setCupWeight) < CUP_DETECTION_TOLERANCE 
    //     && ABS(weightHistory.maxSince((int64_t)millis() - 1000) - setCupWeight) < CUP_DETECTION_TOLERANCE
    //     && (lastTareAt != 0)
    //     && scaleReady)
    if (digitalRead(GRIND_BUTTON_PIN)
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
    if (lastTareAt == 0) {
      tareScale();
    }
    scaleStatus = STATUS_EMPTY;
  }
  rotary_loop();
  delay(50);
}



void setupScale() {
  loadcell.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);

  rotaryEncoder = new Encoder(ROTARY_ENCODER_A_PIN, ROTARY_ENCODER_B_PIN);

  pinMode(ROTARY_ENCODER_BUTTON_PIN, INPUT_PULLUP); 
  pinMode(GRINDER_ACTIVE_PIN, OUTPUT);
  pinMode(GRIND_BUTTON_PIN, INPUT);
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
  loadcell.set_offset(LOADCELL_OFFSET);
  lastTareAt = 0;

  Serial.println("Scale Setup");
}
