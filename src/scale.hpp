#pragma once

#include <SimpleKalmanFilter.h>
#include "HX711.h"
#include "display.hpp"

class MenuItem
{
    public:
        int id;
        bool selected;
        char menuName[16];
        double increment;
        double *value;
};

#define STATUS_EMPTY 0
#define STATUS_GRINDING_IN_PROGRESS 1
#define STATUS_GRINDING_FINISHED 2
#define STATUS_GRINDING_FAILED 3
#define STATUS_IN_MENU 4
#define STATUS_IN_SUBMENU 5
#define STATUS_IN_SUBSUBMENU 6
#define STATUS_TARING 7

#define LOADCELL_DOUT_PIN 18 //GPIO18
#define LOADCELL_SCK_PIN 19 //GPIO19

#define LOADCELL_SCALE_FACTOR 1079 // 7351
#define LOADCELL_OFFSET 4700

#define TARE_MEASURES 40 // use the average of measure for taring
#define TARE_THRESHOLD_COUNTS 2 * 1680 // this value is specific to the loadcell and HX711
#define SIGNIFICANT_WEIGHT_CHANGE 5 // 5 grams changes are used to detect a significant change
#define COFFEE_DOSE_WEIGHT 16.5
#define COFFEE_DOSE_OFFSET -0.25
#define MAX_GRINDING_TIME 60000 // 60 seconds diff
#define GRINDING_DELAY_TOLERANCE 5000 // 5 seconds
#define GRINDING_FAILED_WEIGHT_TO_RESET 150 // force on balance need to be measured to reset grinding

#define GRINDER_ACTIVE_PIN 22 //GPIO22
#define GRIND_BUTTON_PIN 14 //GPIO14

#define TARE_MIN_INTERVAL 60 * 1000 // auto-tare at most once every 60 seconds

#define ROTARY_ENCODER_A_PIN 8 //GPIO8
#define ROTARY_ENCODER_B_PIN 9 //GPIO9
#define ROTARY_ENCODER_BUTTON_PIN 10 //GPIO10
#define ROTARY_ENCODER_VCC_PIN -1
#define ROTARY_ENCODER_STEPS 4

#define INIT_ADDRESS 0
#define DOSE_ADDRESS 1
#define OFFSET_ADDRESS 2
#define CALIBRATION_ADDRESS 6

extern double scaleWeight;
extern bool wakeDisp;
extern unsigned long scaleLastUpdatedAt;
extern unsigned long lastAction;
extern unsigned long lastTareAt;
extern bool scaleReady;
extern int scaleStatus;
extern double cupWeightEmpty;
extern unsigned long startedGrindingAt;
extern unsigned long finishedGrindingAt;
extern double setWeight;
extern double offset;
extern bool greset;
extern int menuItemsCount;

extern MenuItem menuItems[];
extern int currentMenuItem;
extern int currentSetting;

void setupScale();
void updateScale();
void scaleStatusLoop();
