#pragma once

#include "scale.hpp"
#include <SPI.h>
#include <U8g2lib.h>

#define DISPLAY_SDA_PIN 4 //6
#define DISPLAY_SCL_PIN 5 //7

#define MENU_ITEM_NONE -1
#define MENU_ITEM_TARE 0
#define MENU_ITEM_CALIBRATE 1
#define MENU_ITEM_OFFSET 2
#define MENU_ITEM_RESET 3
#define MENU_ITEM_EXIT 4

extern const unsigned int SLEEP_AFTER_MS;
extern bool dispAsleep;

void setupDisplay();
void updateDisplay();
