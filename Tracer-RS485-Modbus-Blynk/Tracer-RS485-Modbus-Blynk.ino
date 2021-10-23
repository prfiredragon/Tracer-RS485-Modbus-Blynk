
// Fill-in information from your Blynk Template here
#define BLYNK_TEMPLATE_ID           "TMPLYy0Kvjo6"
#define BLYNK_DEVICE_NAME           "Tracer 1"

#define BLYNK_FIRMWARE_VERSION        "1.0.4"

#define BLYNK_PRINT Serial
//#define BLYNK_DEBUG

#define APP_DEBUG

// Uncomment your board, or configure a custom board in Settings.h
//#define USE_SPARKFUN_BLYNK_BOARD
//#define USE_NODE_MCU_BOARD
//#define USE_WITTY_CLOUD_BOARD
#define USE_WEMOS_D1_MINI

#include "BlynkEdgent.h"
#include "ReadModbus.h"

void setup()
{
  Serial.begin(115200);
  delay(100);
  Read_ModBus.setup();
  BlynkEdgent.begin();
  Read_ModBus.startTimers();
}

void loop() {
  BlynkEdgent.run();
  Read_ModBus.run();
}
