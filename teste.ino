/*
    Video: https://www.youtube.com/watch?v=oCMOYS71NIU
    Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleNotify.cpp
    Ported to Arduino ESP32 by Evandro Copercini

   Create a BLE server that, once we receive a connection, will send periodic notifications.
   The service advertises itself as: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
   Has a characteristic of: 6E400002-B5A3-F393-E0A9-E50E24DCCA9E - used for receiving data with "WRITE" 
   Has a characteristic of: 6E400003-B5A3-F393-E0A9-E50E24DCCA9E - used to send data with  "NOTIFY"

   The design of creating the BLE server is:
   1. Create a BLE Server
   2. Create a BLE Service
   3. Create a BLE Characteristic on the Service
   4. Create a BLE Descriptor on the characteristic
   5. Start the service.
   6. Start advertising.

   In this example rxValue is the data received (only accessible inside that function).
   And txValue is the data to be sent, in this example just a byte incremented every second. 
*/
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Arduino.h>

BLECharacteristic *pCharacteristic;
bool deviceConnected = false;
float txValue = 0;

//std::string rxValue; // Could also make this a global var to access it in loop()

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E" // UART service UUID
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"


int numberOfSamples = 2000;
double ICAL = 1.08;
 
// Pino do ESP32 - Pin 0 ADC
int inPinI1 = 36;
 
// CT: Voltage depends on current, burden resistor, and turns
#define CT_BURDEN_RESISTOR    62   // resistência interna do sensor
#define CT_TURNS              1800 //enrolamentos do sensor, bobina enrolada 1800 vezes
#define VOLTAGE               220  // saida da rede elétrica
 
// Initial gueses for ratios, modified by VCAL/ICAL tweaks
double I_RATIO = (long double)CT_TURNS / CT_BURDEN_RESISTOR * 3.3 / 4096 * ICAL;
 
//Filter variables 1
double lastFilteredI, filteredI;
double sqI,sumI;
//Sample variables
int lastSampleI,sampleI;
double Irms1;
unsigned long timer;

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();

      if (rxValue.length() > 0) {
        Serial.println("*********");
        Serial.print("Received Value: ");

        for (int i = 0; i < rxValue.length(); i++) {
          Serial.print(rxValue[i]);
        }
        Serial.println();
        Serial.println();
        Serial.println("*********");
      }
    }
};

void setup() {
  Serial.begin(115200);

  // Create the BLE Device
  BLEDevice::init("MONITORAMENTO DE CONSUMO"); // Give it a name

  // Create the BLE Server
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_TX,
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
                      
  pCharacteristic->addDescriptor(new BLE2902());

  BLECharacteristic *pCharacteristic = pService->createCharacteristic(
                                         CHARACTERISTIC_UUID_RX,
                                         BLECharacteristic::PROPERTY_WRITE
                                       );

  pCharacteristic->setCallbacks(new MyCallbacks());

  // Start the service
  pService->start();

  // Start advertising e espera o Client conectar para transmitir
  pServer->getAdvertising()->start();
 
  Serial.println("Waiting a client connection to notify...");
}

void loop() {
  if (deviceConnected) {
  timer = millis();
  for (int n=0; n<numberOfSamples; n++)
{
 
   //Used for offset removal
   lastSampleI=sampleI;
 
   //Lê a corrente com analogRead   
   sampleI = analogRead(inPinI1);
 
   //Used for offset removal
   lastFilteredI = filteredI;
 
   //Digital high pass filters to remove 1.6V DC offset.
   filteredI = 0.9989 * (lastFilteredI+sampleI-lastSampleI);
 
   //Root-mean-square method current
   //1) square current values
   sqI = filteredI * filteredI;
   //2) sum 
   sumI += sqI;
   delay(1);
}
 
//Calculation of the root of the mean of the voltage and current squared (rms)
//Calibration coeficients applied. 
  Irms1 = (I_RATIO * sqrt(sumI / numberOfSamples)) - 0.06;
  if (Irms1 < 0){ Irms1 = 0; }; //Set negative Current to zero
  sumI = 0;

    // Let's convert the value to a char array:
    char txString[8]; // Tamanho do Array Char de 8 Bits
    dtostrf(Irms1, 1, 2, txString); // float_val, min_width, digits_after_decimal, char_buffer - Função que transforma um Float em Char
    
//    pCharacteristic->setValue(&txValue, 1); // To send the integer value
//    pCharacteristic->setValue("Hello!"); // Sending a test message
    pCharacteristic->setValue(txString); //Pega o valor para enviar
    
    pCharacteristic->notify(); // Send the value to the app!
    Serial.print("*** Sent Value: ");
    Serial.print(txString);
    Serial.println(" ***");
    Serial.print("*** Tempo Percorrido ");
    Serial.print(timer);
    Serial.println(" ***");

  }
  delay(1);
}
