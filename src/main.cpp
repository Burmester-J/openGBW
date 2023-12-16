#include <Arduino.h>

#include "display.hpp"
#include "scale.hpp"

void setup() {
  Serial.begin(115200);
  while(!Serial){delay(100);}

  
  setupDisplay();
  setupScale();

  Serial.println();
  Serial.println("******************************************************");
}

void loop() {
  //rotary_loop();
  delay(1000);
}
