#include "display.hpp"

// U8G2_R0 is no rotation, U8G2_R2 is 180° flipped
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0); // DISPLAY_SCL_PIN, DISPLAY_SDA_PIN are not needed, because we use hardware I2C

const unsigned int SLEEP_AFTER_MS = 60 * 1000; // sleep after 60 seconds
bool dispAsleep = false;

void CenterPrintToScreen(char const *str, u8g2_uint_t y) {
  u8g2_uint_t width = u8g2.getStrWidth(str);
  u8g2.setCursor(128 / 2 - width / 2, y);
  u8g2.print(str);
}

void LeftPrintToScreen(char const *str, u8g2_uint_t y)
{
  u8g2_uint_t width = u8g2.getStrWidth(str);
  u8g2.setCursor(5, y);
  u8g2.print(str);
}

void LeftPrintActiveToScreen(char const *str, u8g2_uint_t y)
{
  u8g2_uint_t width = u8g2.getStrWidth(str);
  u8g2.setDrawColor(1);
  u8g2.drawBox(3, y - 1, 122, 14);
  u8g2.setDrawColor(0);
  u8g2.setCursor(5, y);
  u8g2.print(str);
  u8g2.setDrawColor(1);
}

void RightPrintToScreen(char const *str, u8g2_uint_t y)
{
  u8g2_uint_t width = u8g2.getStrWidth(str);
  u8g2.setCursor(123-width, y);
  u8g2.print(str);
}

void showMenu(){
  int prevIndex = (currentMenuItem - 1) % menuItemsCount;
  int nextIndex = (currentMenuItem + 1) % menuItemsCount;

  prevIndex = prevIndex < 0 ? prevIndex + menuItemsCount : prevIndex;
  MenuItem prev = menuItems[prevIndex];
  MenuItem current = menuItems[currentMenuItem];
  MenuItem next = menuItems[nextIndex];
  char buf[3];
  u8g2.clearBuffer();
  u8g2.setFontPosTop();
  u8g2.setFont(u8g2_font_7x14B_tf);
  CenterPrintToScreen("Menu", 0);
  u8g2.setFont(u8g2_font_7x13_tr);
  LeftPrintToScreen(prev.menuName, 19);
  LeftPrintActiveToScreen(current.menuName, 35);
  LeftPrintToScreen(next.menuName, 51);

  u8g2.sendBuffer();
}

void showOffsetMenu(){
  char buf[16];
  u8g2.clearBuffer();
  u8g2.setFontPosTop();
  u8g2.setFont(u8g2_font_7x14B_tf);
  CenterPrintToScreen("Adjust offset", 0);
  u8g2.setFont(u8g2_font_7x13_tr);
  snprintf(buf, sizeof(buf), "%3.2fg", offset);
  CenterPrintToScreen(buf, 28);
  u8g2.sendBuffer();
}


void showCalibrationMenu(){
  u8g2.clearBuffer();
  u8g2.setFontPosTop();
  u8g2.setFont(u8g2_font_7x14B_tf);
  CenterPrintToScreen("Calibration", 0);
  u8g2.setFont(u8g2_font_7x13_tr);
  CenterPrintToScreen("Place 100g weight", 19);
  CenterPrintToScreen("on scale and", 35);
  CenterPrintToScreen("press button", 51);
  u8g2.sendBuffer();
}

void showResetMenu()
{
  char buf[16];
  u8g2.clearBuffer();
  u8g2.setFontPosTop();
  u8g2.setFont(u8g2_font_7x14B_tf);
  CenterPrintToScreen("Reset to defaults?", 0);
  u8g2.setFont(u8g2_font_7x13_tr);
  if (greset)
  {
    LeftPrintActiveToScreen("Confirm", 19);
    LeftPrintToScreen("Cancel", 35);
  }
  else
  {
    LeftPrintToScreen("Confirm", 19);
    LeftPrintActiveToScreen("Cancel", 35);
  }
  u8g2.sendBuffer();
}

void showSetting(){
  if(currentSetting == MENU_ITEM_OFFSET){
    showOffsetMenu();
  }
  else if (currentSetting == MENU_ITEM_CALIBRATE)
  {
    showCalibrationMenu();
  }
  else if (currentSetting == MENU_ITEM_RESET)
  {
    showResetMenu();
  }
}

void updateDisplay() {
  char buf[64];
  char buf2[64];

  u8g2.clearBuffer();

  if (dispAsleep && !wakeDisp)
    return;

  if (!wakeDisp
      && (millis() - lastAction) > SLEEP_AFTER_MS) {
    u8g2.sendBuffer();
    delay(100);
    dispAsleep = true;
    return;
  }

  dispAsleep = false;

  if (scaleLastUpdatedAt == 0) {
    u8g2.setFontPosTop();
    u8g2.drawStr(0, 20, "Initializing...");
    Serial.println("Initializing");
  } else if (!scaleReady) {
    u8g2.setFontPosTop();
    u8g2.drawStr(0, 20, "SCALE ERROR");
  } else {
    if (scaleStatus == STATUS_GRINDING_IN_PROGRESS) {
      u8g2.setFontPosTop();
      u8g2.setFont(u8g2_font_7x13_tr);
      CenterPrintToScreen("Grinding...", 0);


      u8g2.setFontPosCenter();
      u8g2.setFont(u8g2_font_7x14B_tf);
      u8g2.setCursor(3, 32);
      snprintf(buf, sizeof(buf), "%3.1fg", scaleWeight - cupWeightEmpty);
      u8g2.print(buf);

      u8g2.setFontPosCenter();
      u8g2.setFont(u8g2_font_unifont_t_symbols);
      u8g2.drawGlyph(64, 32, 0x2794);

      u8g2.setFontPosCenter();
      u8g2.setFont(u8g2_font_7x14B_tf);
      u8g2.setCursor(84, 32);
      snprintf(buf, sizeof(buf), "%3.1fg", setWeight);
      u8g2.print(buf);

      u8g2.setFontPosBottom();
      u8g2.setFont(u8g2_font_7x13_tr);
      snprintf(buf, sizeof(buf), "%3.1fs", startedGrindingAt > 0 ? (double)(millis() - startedGrindingAt) / 1000 : 0);
      CenterPrintToScreen(buf, 64);
    } else if (scaleStatus == STATUS_EMPTY) {
      u8g2.setFontPosTop();
      u8g2.setFont(u8g2_font_7x13_tr);
      CenterPrintToScreen("Weight:", 0);

      u8g2.setFont(u8g2_font_7x14B_tf);
      u8g2.setFontPosCenter();
      u8g2.setCursor(0, 28);
      snprintf(buf, sizeof(buf), "%3.1fg", abs(scaleWeight));
      CenterPrintToScreen(buf, 32);

      u8g2.setFont(u8g2_font_7x13_tf);
      u8g2.setFontPosCenter();
      u8g2.setCursor(5, 50);
      snprintf(buf2, sizeof(buf2), "Set: %3.1fg", setWeight);
      LeftPrintToScreen(buf2, 50);

      
    } else if (scaleStatus == STATUS_GRINDING_FAILED) {

      u8g2.setFontPosTop();
      u8g2.setFont(u8g2_font_7x14B_tf);
      CenterPrintToScreen("Grinding failed", 0);

      u8g2.setFontPosTop();
      u8g2.setFont(u8g2_font_7x13_tr);
      CenterPrintToScreen("Press the balance", 32);
      CenterPrintToScreen("to reset", 42);
    } else if (scaleStatus == STATUS_GRINDING_FINISHED) {

      u8g2.setFontPosTop();
      u8g2.setFont(u8g2_font_7x13_tr);
      u8g2.setCursor(0, 0);
      CenterPrintToScreen("Grinding finished", 0);

      u8g2.setFontPosCenter();
      u8g2.setFont(u8g2_font_7x14B_tf);
      u8g2.setCursor(3, 32);
      snprintf(buf, sizeof(buf), "%3.1fg", scaleWeight - cupWeightEmpty);
      u8g2.print(buf);

      u8g2.setFontPosCenter();
      u8g2.setFont(u8g2_font_unifont_t_symbols);
      u8g2.drawGlyph(64, 32, 0x2794);

      u8g2.setFontPosCenter();
      u8g2.setFont(u8g2_font_7x14B_tf);
      u8g2.setCursor(84, 32);
      snprintf(buf, sizeof(buf), "%3.1fg", setWeight);
      u8g2.print(buf);

      u8g2.setFontPosBottom();
      u8g2.setFont(u8g2_font_7x13_tr);
      u8g2.setCursor(64, 64);
      snprintf(buf, sizeof(buf), "%3.1fs", (double)(finishedGrindingAt - startedGrindingAt) / 1000);
      CenterPrintToScreen(buf, 64);
    }
    else if (scaleStatus == STATUS_IN_MENU)
    {
      showMenu();
    }
    else if (scaleStatus == STATUS_IN_SUBMENU)
    {
      showSetting();
    }
    else if (scaleStatus == STATUS_TARING)
    {
      char buf[16];
      u8g2.clearBuffer();
      u8g2.setFontPosTop();
      u8g2.setFont(u8g2_font_7x13_tr);
      CenterPrintToScreen("Taring...", 20);
      u8g2.setFont(u8g2_font_7x13_tr);
      CenterPrintToScreen("Remove weight", 38);
      u8g2.sendBuffer();
    }
  }
  u8g2.sendBuffer();
  // delay(100);
}

void setupDisplay() {
  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_7x13_tr);
  u8g2.setFontPosTop();
  u8g2.drawStr(0, 20, "Hello");
  u8g2.sendBuffer();

  Serial.println("Display Setup");
}
