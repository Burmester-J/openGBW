#pragma once

#include "scale.hpp"
#include <SPI.h>
#include <U8g2lib.h>

#define DISPLAY_SDA_PIN 4 //6
#define DISPLAY_SCL_PIN 5 //7

#define MENU_ITEM_NONE -1
#define MENU_ITEM_MANUAL_GRIND 0
#define MENU_ITEM_CUP_WEIGHT 1
#define MENU_ITEM_CALIBRATE 2
#define MENU_ITEM_OFFSET 3
#define MENU_ITEM_SCALE_MODE 4
#define MENU_ITEM_GRINDING_MODE 5
#define MENU_ITEM_RESET 6
#define MENU_ITEM_EXIT 7
#define MENU_ITEM_TARE 8

extern const unsigned int SLEEP_AFTER_MS;
extern bool dispAsleep;

void setupDisplay();
void updateDisplay();
