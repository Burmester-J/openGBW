#include <Arduino.h>

#include "display.hpp"
#include "scale.hpp"

void setup() {
  Serial.begin(9600);
  while(!Serial){delay(100);}
  
  setupDisplay();

  Serial.println();
  Serial.println("******************************************************");
}

void loop() {
  updateDisplay();
}

void setup1(){
  Serial.begin(9600);
  while(!Serial){delay(100);}

  setupScale();
}

void loop1(){
  updateScale();
  scaleStatusLoop();
}

