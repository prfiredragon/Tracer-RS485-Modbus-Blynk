#include <ModbusMaster.h>

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))
float battChargeCurrent, battDischargeCurrent, battOverallCurrent, battChargePower;
float bvoltage, ctemp, btemp, rbtemp, pctemp, bremaining, lvoltage, lpower, lcurrent, pvvoltage, pvcurrent, pvpower;
float stats_today_pv_volt_min, stats_today_pv_volt_max;
uint8_t result;
bool rs485DataReceived = true;
bool loadPoweredOn = true;
long last_sensor_reading = 0;
long sensor_reading_time = 600;

#define MAX485_DE D1
#define MAX485_RE_NEG D2

/*
   Virtual Pins - Base. (For Blynk)
*/
#define vPIN_PV_POWER                   V1
#define vPIN_PV_CURRENT                 V2
#define vPIN_PV_VOLTAGE                 V3
#define vPIN_LOAD_CURRENT               V4
#define vPIN_LOAD_POWER                 V5
#define vPIN_BATT_TEMP                  V6
#define vPIN_BATT_VOLTAGE               V7
#define vPIN_BATT_REMAIN                V8
#define vPIN_CONTROLLER_TEMP            V9
#define vPIN_BATTERY_CHARGE_CURRENT     V10
#define vPIN_BATTERY_CHARGE_POWER       V11
#define vPIN_BATTERY_OVERALL_CURRENT    V12
#define vPIN_LOAD_ENABLED               V14


/*
   ModBus Register Address List
*/
#define PV_ARRAY_VOLT                   0x00
#define PV_ARRAY_CURRENT                0x01
#define PV_ARRAY_POWER_L                0x02
#define PV_ARRAY_POWER_H                0x03
#define BATT_VOLT                       0x04
#define BATT_CURRENT                    0x05
#define BATT_POWER_L                    0x06
#define BATT_POWER_H                    0x07
#define LOAD_VOLT                       0x0C
#define LOAD_CURRENT                    0x0D
#define LOAD_POWER_L                    0x0E
#define LOAD_POWER_H                    0x0F
#define BATT_TEMP                       0x10
#define CONTROLLER_TEMP                 0x11
#define POWER_COMPONENTS_TEMP           0x12
#define BATT_SOC                        0x00
#define REMOTE_BATT_TEMP                0x01



void AddressRegistry_3100();
void AddressRegistry_3106();
void AddressRegistry_310D();
void AddressRegistry_311A();
void AddressRegistry_331B();

uint8_t checkLoadCoilState();
void uploadToBlynk();

ModbusMaster node;
//SimpleTimer timer;

void preTransmission() {
  digitalWrite(MAX485_RE_NEG, 1);
  digitalWrite(MAX485_DE, 1);
}

void postTransmission() {
  digitalWrite(MAX485_RE_NEG, 0);
  digitalWrite(MAX485_DE, 0);
}

// A list of the regisities to query in order
typedef void (*RegistryList[])();

RegistryList Registries = {
  AddressRegistry_3100,
  //AddressRegistry_3106,
  //AddressRegistry_310D,
  AddressRegistry_311A,
  AddressRegistry_331B,
};

// keep log of where we are
uint8_t currentRegistryNumber = 0;

// function to switch to next registry
void nextRegistryNumber() {
  // better not use modulo, because after overlow it will start reading in incorrect order
  currentRegistryNumber++;
  if (currentRegistryNumber >= ARRAY_SIZE(Registries)) {
    currentRegistryNumber = 0;
    uploadToBlynk();
  }
}

// --------------------------------------------------------------------------------
  
  // upload values
  void uploadToBlynk() {
    Blynk.virtualWrite(vPIN_PV_POWER,                   pvpower);
    Blynk.virtualWrite(vPIN_PV_CURRENT,                 pvcurrent);
    Blynk.virtualWrite(vPIN_PV_VOLTAGE,                 pvvoltage);
    Blynk.virtualWrite(vPIN_LOAD_CURRENT,               lcurrent);
    Blynk.virtualWrite(vPIN_LOAD_POWER,                 lpower);
    Blynk.virtualWrite(vPIN_BATT_TEMP,                  btemp);
    Blynk.virtualWrite(vPIN_BATT_VOLTAGE,               bvoltage);
    Blynk.virtualWrite(vPIN_BATT_REMAIN,                bremaining);
    Blynk.virtualWrite(vPIN_CONTROLLER_TEMP,            ctemp);
    Blynk.virtualWrite(vPIN_BATTERY_CHARGE_CURRENT,     battChargeCurrent);
    Blynk.virtualWrite(vPIN_BATTERY_CHARGE_POWER,       battChargePower);
    Blynk.virtualWrite(vPIN_BATTERY_OVERALL_CURRENT,    battOverallCurrent);
    Blynk.virtualWrite(vPIN_LOAD_ENABLED,               loadPoweredOn);
  }
  
  // exec a function of registry read (cycles between different addresses)
  void executeCurrentRegistryFunction() {
    Registries[currentRegistryNumber]();
    nextRegistryNumber();  
    
  }
  
  uint8_t setOutputLoadPower(uint8_t state) {
    Serial.print("Writing coil 0x0006 value to: ");
    Serial.println(state);

    delay(10);
    // Set coil at address 0x0006 (Force the load on/off)
    result = node.writeSingleCoil(0x0006, state);

    if (result == node.ku8MBSuccess) {
      node.getResponseBuffer(0x00);
      Serial.println("Success.");
    }

    return result;
  }
  
  // callback to on/off button state changes from the Blynk app
  BLYNK_WRITE(vPIN_LOAD_ENABLED) {
    uint8_t newState = (uint8_t)param.asInt();
    
    Serial.print("Setting load state output coil to value: ");
    Serial.println(newState);

    result = setOutputLoadPower(newState);
    //readOutputLoadState();
    result &= checkLoadCoilState();
    
    if (result == node.ku8MBSuccess) {
      Serial.println("Write & Read suceeded.");
    } else {
      Serial.println("Write & Read failed.");
    }
    
    Serial.print("Output Load state value: ");
    Serial.println(loadPoweredOn);
    Serial.println();
    Serial.println("Uploading results to Blynk.");
        
    uploadToBlynk();
  }
  
  uint8_t readOutputLoadState() {
    delay(10);
    result = node.readHoldingRegisters(0x903D, 1);
    
    if (result == node.ku8MBSuccess) {
      loadPoweredOn = (node.getResponseBuffer(0x00) & 0x02) > 0;
  
      Serial.print("Set success. Load: ");
      Serial.println(loadPoweredOn);
    } else {
      // update of status failed
      Serial.println("readHoldingRegisters(0x903D, 1) failed!");
    }
    return result;
  }
  
  // reads Load Enable Override coil
  uint8_t checkLoadCoilState() {
    Serial.print("Reading coil 0x0006... ");

    delay(10);
    result = node.readCoils(0x0006, 1);
    
    Serial.print("Result: ");
    Serial.println(result);

    if (result == node.ku8MBSuccess) {
      loadPoweredOn = (node.getResponseBuffer(0x00) > 0);

      Serial.print(" Value: ");
      Serial.println(loadPoweredOn);
    } else {
      Serial.println("Failed to read coil 0x0006!");
    }

    return result;
 }

// -----------------------------------------------------------------
/*
  void AddressRegistry_3100() {
    result = node.readInputRegisters(0x3100, 6);
  
    if (result == node.ku8MBSuccess) {
      
      pvvoltage = node.getResponseBuffer(0x00) / 100.0f;
      //Serial.print("PV Voltage: ");
      //Serial.println(pvvoltage);
  
      pvcurrent = node.getResponseBuffer(0x01) / 100.0f;
      //Serial.print("PV Current: ");
      //Serial.println(pvcurrent);
  
      pvpower = (node.getResponseBuffer(0x02) | node.getResponseBuffer(0x03) << 16) / 100.0f;
      //Serial.print("PV Power: ");
      //Serial.println(pvpower);
      
      bvoltage = node.getResponseBuffer(0x04) / 100.0f;
      //Serial.print("Battery Voltage: ");
      //Serial.println(bvoltage);
      
      battChargeCurrent = node.getResponseBuffer(0x05) / 100.0f;
      //Serial.print("Battery Charge Current: ");
      //Serial.println(battChargeCurrent);
    }
  }
*/
/*
  void AddressRegistry_3106()
  {
    result = node.readInputRegisters(0x3106, 2);

    if (result == node.ku8MBSuccess) {
      battChargePower = (node.getResponseBuffer(0x00) | node.getResponseBuffer(0x01) << 16)  / 100.0f;
      //Serial.print("Battery Charge Power: ");
      //Serial.println(battChargePower);
    }
  }
*/
/*
  void AddressRegistry_310D() 
  {
    result = node.readInputRegisters(0x310D, 3);

    if (result == node.ku8MBSuccess) {
      lcurrent = node.getResponseBuffer(0x00) / 100.0f;
      //Serial.print("Load Current: ");
      //Serial.println(lcurrent);
  
      lpower = (node.getResponseBuffer(0x01) | node.getResponseBuffer(0x02) << 16) / 100.0f;
      //Serial.print("Load Power: ");
      //Serial.println(lpower);
    } else {
      rs485DataReceived = false;
      Serial.println("Read register 0x310D failed!");
    }    
  } 
*/
  void AddressRegistry_311A() {
    result = node.readInputRegisters(0x311A, 2);
   
    if (result == node.ku8MBSuccess) {    
      bremaining = node.getResponseBuffer(0x00) / 1.0f;
      //Serial.print("Battery Remaining %: ");
      //Serial.println(bremaining);
      
      btemp = node.getResponseBuffer(0x01) / 100.0f;
      //Serial.print("Battery Temperature: ");
      //Serial.println(btemp);
    } else {
      rs485DataReceived = false;
      Serial.println("Read register 0x311A failed!");
      Serial.println();
      Serial.print("FAIL TO READ, CODE: 0x");
      Serial.println(result, HEX);
      Serial.println();

      // Result codes:
      // 0x00 Success
      // 0x01 Illegal function Error
      // 0x02 Illegal Data Address Error
      // 0x03 Illegal Data Value Error
      // 0xE0 Invalid Response Slave ID Error
      // 0xE1 Invalid Response Function Error
      // 0xE2 Response Timed-Out Error
      // 0xE3 Invalid Response CRC Error
    }
  }

  void AddressRegistry_331B() {
    result = node.readInputRegisters(0x331B, 2);
    
    if (result == node.ku8MBSuccess) {
      battOverallCurrent = (node.getResponseBuffer(0x00) | node.getResponseBuffer(0x01) << 16) / 100.0f;
      //Serial.print("Battery Discharge Current: ");
      //Serial.println(battOverallCurrent);
    } else {
      rs485DataReceived = false;
      Serial.println("Read register 0x331B failed!");
      Serial.println();
      Serial.print("FAIL TO READ, CODE: 0x");
      Serial.println(result, HEX);
      Serial.println();

      // Result codes:
      // 0x00 Success
      // 0x01 Illegal function Error
      // 0x02 Illegal Data Address Error
      // 0x03 Illegal Data Value Error
      // 0xE0 Invalid Response Slave ID Error
      // 0xE1 Invalid Response Function Error
      // 0xE2 Response Timed-Out Error
      // 0xE3 Invalid Response CRC Error
    }
  }


  
  void AddressRegistry_3100()
{

  result = node.readInputRegisters(0x3100, 20); 

  if (result == node.ku8MBSuccess)
  {
    pvvoltage = node.getResponseBuffer(PV_ARRAY_VOLT)/100.0f;
    pvcurrent = node.getResponseBuffer(PV_ARRAY_CURRENT)/100.0f;
    pvpower = (node.getResponseBuffer(PV_ARRAY_POWER_L) |
                    (node.getResponseBuffer(PV_ARRAY_POWER_H) << 16))/100.0f;

    bvoltage = node.getResponseBuffer(BATT_VOLT)/100.0f;
    battChargeCurrent = node.getResponseBuffer(BATT_CURRENT)/100.0f;
    battChargePower = (node.getResponseBuffer(BATT_POWER_L) |
                    (node.getResponseBuffer(BATT_POWER_H) << 16))/100.0f;

    lvoltage = node.getResponseBuffer(LOAD_VOLT)/100.0f;
    lcurrent = node.getResponseBuffer(LOAD_CURRENT)/100.0f;
    lpower = (node.getResponseBuffer(LOAD_POWER_L) |
                    (node.getResponseBuffer(LOAD_POWER_H) << 16))/100.0f;

    btemp = node.getResponseBuffer(BATT_TEMP)/100.0f;
    ctemp = node.getResponseBuffer(CONTROLLER_TEMP)/100.0f; 
    pctemp = node.getResponseBuffer(POWER_COMPONENTS_TEMP)/100.0f; 
                    
                    
                    
      
      /*Serial.print("PV Voltage: ");
      Serial.println(pvvoltage);
  
      Serial.print("PV Current: ");
      Serial.println(pvcurrent);
  
      Serial.print("PV Power: ");
      Serial.println(pvpower);
      Serial.println();
      
      Serial.print("Battery Voltage: ");
      Serial.println(bvoltage);
      
      Serial.print("Battery Charge Current: ");
      Serial.println(battChargeCurrent);
      
      Serial.print("Battery Charge Power: ");
      Serial.println(battChargePower);
                    
      Serial.print("Battery Temperature: ");
      Serial.println(btemp);     
      Serial.println();
      
      Serial.print("Load Voltage: ");
      Serial.println(lvoltage);
      Serial.print("Load Current: ");
      Serial.println(lcurrent);
      Serial.print("Load Power: ");
      Serial.println(lpower);
      Serial.println();
      
      Serial.print("Controller Temp: ");
      Serial.println(ctemp);
      Serial.println();
      Serial.print("Power Components Temp: ");
      Serial.println(pctemp);
      Serial.println();   
    
      Serial.println();
      Serial.println();*/

            
    
  } else {
    
    Serial.println("Read register 0x3100 failed!");
    Serial.println();
    Serial.print("FAIL TO READ, CODE: 0x");
    Serial.println(result, HEX);
    Serial.println();

    // Result codes:
    // 0x00 Success
    // 0x01 Illegal function Error
    // 0x02 Illegal Data Address Error
    // 0x03 Illegal Data Value Error
    // 0xE0 Invalid Response Slave ID Error
    // 0xE1 Invalid Response Function Error
    // 0xE2 Response Timed-Out Error
    // 0xE3 Invalid Response CRC Error
   }
}



//class ReadModBus {

//public:

  void ReadModBus_setup() {
  pinMode(MAX485_RE_NEG, OUTPUT);
  pinMode(MAX485_DE, OUTPUT);
  
  digitalWrite(MAX485_RE_NEG, 0);
  digitalWrite(MAX485_DE, 0);

  // Modbus slave ID 1
  node.begin(1, Serial);

  // callbacks to toggle DE + RE on MAX485
  node.preTransmission(preTransmission);
  node.postTransmission(postTransmission);
    
  }

  void startTimers() {
      //timerTask1 = timer.setInterval(3000L, executeCurrentRegistryFunction);  
      //timerTask2 = timer.setInterval(3000L, nextRegistryNumber);  
      //timerTask3 = timer.setInterval(3000L, uploadToBlynk);
  }

  void ReadModBus_run() {
    if(millis() - last_sensor_reading >= sensor_reading_time) // if it’s 5000ms or more since we last took a reading
      {
          executeCurrentRegistryFunction();
          last_sensor_reading = millis();
      }
    //timer.run();
  }

//};

//ReadModBus Read_ModBus;
