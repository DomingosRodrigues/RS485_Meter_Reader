/*

  RS485 reader for Eastron SDM120M
  Reads the data via MODbus using TTL to modbus converter and sends via MQTT

*/

#include <ModbusMaster.h>
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)  \
  (byte & 0x80 ? '1' : '0'), \
  (byte & 0x40 ? '1' : '0'), \
  (byte & 0x20 ? '1' : '0'), \
  (byte & 0x10 ? '1' : '0'), \
  (byte & 0x08 ? '1' : '0'), \
  (byte & 0x04 ? '1' : '0'), \
  (byte & 0x02 ? '1' : '0'), \
  (byte & 0x01 ? '1' : '0') 

// Website
  #include <ESPAsyncTCP.h>
  #include <ESPAsyncWebServer.h>
  AsyncWebServer server(80);
  const char* PARAM_MESSAGE = "message";

// Variables
  float Frequency = 0;
  float Voltage   = 0;
  float Current   = 0;
  float Apower    = 0;
  float Rpower    = 0;
  float APpower   = 0;
  float Pfactor   = 0;
  float TApower   = 0;
  float TRpower   = 0;
  float ExportActive = 0;
  float ImportReactive = 0;
  float ExportReactive = 0;

// Modbus
ModbusMaster node;
SoftwareSerial AltSerial;

// WiFi
  const char *ssid = "404NotFound"; 
  const char *password = "get12125671out"; 

// MQTT Broker
  const char *mqtt_broker = "192.168.50.4";
  const char *topic = "power_readings/solar_energy/sdm120m";
  const char *mqtt_username = "mqtt_user";
  const char *mqtt_password = "scooby32";
  const int mqtt_port = 1883;

  void callback(char *topic, byte *payload, unsigned int length) {
    Serial.print("Message arrived in topic: ");
    Serial.println(topic);
    Serial.print("Message:");
    for (unsigned int i = 0; i < length; i++) {
        Serial.print((char) payload[i]);
    }
    Serial.println();
    Serial.println("-----------------------");
 }

// Wifi objects
  WiFiClient espClient;
  PubSubClient client(espClient);

// Meter DRIVER
// Choose the meter in use here 
// SDM120M
// or
// ORWE514
#define METER_NAME SDM120M

#define ORWE514                       1
#define ORWE514_BAUDRATE              9600
#define ORWE514_SERIAL_MODE           SWSERIAL_8E1
#define ORWE514_FREQUENCY             304
#define ORWE514_VOLTAGE               305
#define ORWE514_CURRENT               313
#define ORWE514_ACTIVE_POWER          320
#define ORWE514_REACTIVE_POWER        328
#define ORWE514_APPARENT_POWER        336
#define ORWE514_POWER_FACTOR          344
#define ORWE514_TOTAL_ACTIVE_POWER    40960
#define ORWE514_TOTAL_REACTIVE_POWER  40990


#define SDM120M                                     2
#define SDM120M_BAUDRATE                            2400
#define SDM120M_SERIAL_MODE                         SWSERIAL_8N2
#define SDM120M_VOLTAGE                             0x0000
#define SDM120M_CURRENT                             0x0006
#define SDM120M_ACTIVE_POWER                        0x000C
#define SDM120M_APPARENT_POWER                      0x0012
#define SDM120M_REACTIVE_POWER                      0x0018
#define SDM120M_POWER_FACTOR                        0x001E
#define SDM120M_FREQUENCY                           0x0046
#define SDM120M_IMPORT_ACTIVE_ENERGY                0x0048
#define SDM120M_EXPORT_ACTIVE_ENERGY                0x004A
#define SDM120M_IMPORT_REACTIVE_ENERGY              0x004C
#define SDM120M_EXPORT_REACTIVE_ENERGY              0x004E
#define SDM120M_TOTAL_SYSTEM_POWER_DEMAND           0x0054
#define SDM120M_MAXIMUM_TOTAL_SYSTEM_POWER_DEMAND   0x0056
#define SDM120M_IMPORT_SYSTEM_POWER_DEMAND          0x0058
#define SDM120M_MAXIMUM_IMPORT_SYSTEM_POWER_DEMAND  0x005A
#define SDM120M_EXPORT_SYSTEM_POWER_DEMAND          0x005C
#define SDM120M_MAXIMUM_EXPORT_SYSTEM_POWER_DEMAND  0x005E
#define SDM120M_CURRENT_DEMAND                      0x0102
#define SDM120M_MAXIMUM_CURRENT_DEMAND              0x0108
#define SDM120M_TOTAL_ACTIVE_POWER                  0x0156
#define SDM120M_TOTAL_REACTIVE_POWER                0x0158


// Register address according to datasheet??? it sees the register i
// #define SDM120M_VOLTAGE                             30001
// #define SDM120M_CURRENT                             30007
// #define SDM120M_ACTIVE_POWER                        30013
// #define SDM120M_APPARENT_POWER                      30019
// #define SDM120M_REACTIVE_POWER                      30025
// #define SDM120M_POWER_FACTOR                        30031
// #define SDM120M_FREQUENCY                           30071
// #define SDM120M_IMPORT_ACTIVE_ENERGY                30073
// #define SDM120M_EXPORT_ACTIVE_ENERGY                30075
// #define SDM120M_IMPORT_REACTIVE_ENERGY              30077
// #define SDM120M_EXPORT_REACTIVE_ENERGY              30078
// #define SDM120M_TOTAL_SYSTEM_POWER_DEMAND           30085
// #define SDM120M_MAXIMUM_TOTAL_SYSTEM_POWER_DEMAND   30087
// #define SDM120M_IMPORT_SYSTEM_POWER_DEMAND          30089
// #define SDM120M_MAXIMUM_IMPORT_SYSTEM_POWER_DEMAND  30091
// #define SDM120M_EXPORT_SYSTEM_POWER_DEMAND          30093
// #define SDM120M_MAXIMUM_EXPORT_SYSTEM_POWER_DEMAND  30095
// #define SDM120M_CURRENT_DEMAND                      30259
// #define SDM120M_MAXIMUM_CURRENT_DEMAND              30265
// #define SDM120M_TOTAL_ACTIVE_POWER                  30343
// #define SDM120M_TOTAL_REACTIVE_POWER                30345

#if METER_NAME == ORWE514
  #define MODBUS_BAURATE ORWE514_BAUDRATE
  #define MODBUS_SERIAL_MODE ORWE514_SERIAL_MODE
  #define MODBUS_METER_NAME "Orno OR-WE-514"
#else
  #define MODBUS_BAURATE SDM120M_BAUDRATE
  #define MODBUS_SERIAL_MODE SDM120M_SERIAL_MODE
  #define MODBUS_METER_NAME "Eastron SDM120M"
#endif

uint16_t getData16(uint16_t read_register)
{
  node.readHoldingRegisters(read_register, 1);
  return node.getResponseBuffer(0);
}

uint32_t getData32(uint16_t read_register)
{
  #if METER_NAME == ORWE514
    node.readHoldingRegisters(read_register, 2);
    uint64_t result = (uint64_t)node.getResponseBuffer(0) << 16;
    result |= (uint64_t)node.getResponseBuffer(1);
    return result;
  #else
    node.readInputRegisters(read_register, 2);
    uint32_t result = (uint32_t)node.getResponseBuffer(0) << 16;
    result |= (uint32_t)node.getResponseBuffer(1);

    // // DEBUG 32bit data
    // Serial.printf("\nm: "BYTE_TO_BINARY_PATTERN" "BYTE_TO_BINARY_PATTERN"\n",
    // BYTE_TO_BINARY(node.getResponseBuffer(0)>>8), BYTE_TO_BINARY(node.getResponseBuffer(0)));
    // Serial.printf("\nm: "BYTE_TO_BINARY_PATTERN" "BYTE_TO_BINARY_PATTERN"\n",
    // BYTE_TO_BINARY(node.getResponseBuffer(1)>>8), BYTE_TO_BINARY(node.getResponseBuffer(1)));

    return result;
  #endif
}

uint64_t getData64(uint16_t read_register)
{
  node.readHoldingRegisters(read_register, 4);

  // DEBUG 64bit data
  // Serial.printf("\nm: "BYTE_TO_BINARY_PATTERN" "BYTE_TO_BINARY_PATTERN"\n",
  // BYTE_TO_BINARY(node.getResponseBuffer(0)>>8), BYTE_TO_BINARY(node.getResponseBuffer(0)));
  // Serial.printf("\nm: "BYTE_TO_BINARY_PATTERN" "BYTE_TO_BINARY_PATTERN"\n",
  // BYTE_TO_BINARY(node.getResponseBuffer(1)>>8), BYTE_TO_BINARY(node.getResponseBuffer(1)));
  // Serial.printf("\nm: "BYTE_TO_BINARY_PATTERN" "BYTE_TO_BINARY_PATTERN"\n",
  // BYTE_TO_BINARY(node.getResponseBuffer(2)>>8), BYTE_TO_BINARY(node.getResponseBuffer(2)));
  // Serial.printf("\nm: "BYTE_TO_BINARY_PATTERN" "BYTE_TO_BINARY_PATTERN"\n",
  // BYTE_TO_BINARY(node.getResponseBuffer(3)>>8), BYTE_TO_BINARY(node.getResponseBuffer(3)));

  uint64_t result = (uint64_t)node.getResponseBuffer(0) << 48;
  result |= (uint64_t)node.getResponseBuffer(1) << 32;
  result |= (uint64_t)node.getResponseBuffer(2) << 16;
  result |= (uint64_t)node.getResponseBuffer(3);
  return result;
}

#if METER_NAME == ORWE514
  uint16_t getFrequency(){
      return getData16(ORWE514_FREQUENCY);
  }

  uint16_t getVoltage(){
    return getData16(ORWE514_VOLTAGE);
  }

  uint32_t getCurrent(){
    return getData32(ORWE514_CURRENT);
  }

  int32_t getActivePower(){
    return getData32(ORWE514_ACTIVE_POWER);
  }

  int32_t getReactivePower(){
    return getData32(ORWE514_REACTIVE_POWER);
  }

  int32_t getApparentPower(){
    return getData32(ORWE514_APPARENT_POWER);
  }

  int16_t getPowerFactor(){
    return getData16(ORWE514_POWER_FACTOR);
  }

  uint64_t getTotalApparentPower(){
    return getData32(ORWE514_TOTAL_ACTIVE_POWER);
  }

  uint64_t getTotalReactivePower(){
    return getData32(ORWE514_TOTAL_REACTIVE_POWER);
  }
#else
  float getFrequency(){
    uint32_t read = getData32(SDM120M_FREQUENCY);
    return *(float*)&read;
  }

  float getVoltage(){
    uint32_t read = getData32(SDM120M_VOLTAGE);
    return *(float*)&read;
  }

  float getCurrent(){
    uint32_t read = getData32(SDM120M_CURRENT);
    return *(float*)&read;
  }

  float getActivePower(){
    uint32_t read = getData32(SDM120M_ACTIVE_POWER);
    return *(float*)&read;
  }

  float getReactivePower(){
    uint32_t read = getData32(SDM120M_REACTIVE_POWER);
    return *(float*)&read;
  }

  float getApparentPower(){
    uint32_t read = getData32(SDM120M_APPARENT_POWER);
    return *(float*)&read;
  }

  float getPowerFactor(){
    uint32_t read = getData32(SDM120M_POWER_FACTOR);
    return *(float*)&read;
  }

  float getTotalApparentPower(){
    uint32_t read = getData32(SDM120M_TOTAL_ACTIVE_POWER);
    return *(float*)&read;
  }

  float getTotalReactivePower(){
    uint32_t read = getData32(SDM120M_TOTAL_REACTIVE_POWER);
    return *(float*)&read;
  }

  float getExportActivePower(){
    uint32_t read = getData32(SDM120M_EXPORT_ACTIVE_ENERGY);
    return *(float*)&read;
  }

  float getImportReactivePower(){
    uint32_t read = getData32(SDM120M_IMPORT_REACTIVE_ENERGY);
    return *(float*)&read;
  }

  float getExportReactivePower(){
    uint32_t read = getData32(SDM120M_EXPORT_REACTIVE_ENERGY);
    return *(float*)&read;
  }
#endif

void getPrintAll(){
  #if METER_NAME == ORWE514
    Serial.print("\nOR-WE-514 reading...\n");
    // Read all data
    Frequency =(float)getFrequency()/100;
    Voltage   =(float)getVoltage()/100;
    Current   =(float)getCurrent()/1000;
    Apower    =(float)getActivePower()/1000;
    Rpower    =(float)getReactivePower()/1000;
    APpower   =(float)getApparentPower()/1000;
    Pfactor   =(float)getPowerFactor()/1000;
    TApower   =(double)getTotalApparentPower()/1000;
    TRpower   =(double)getTotalReactivePower()/1000;
    Serial.print("OR-WE-514 output:\n");

    // Print all data
    Serial.printf("Frequency:%.2fHz\n"              ,Frequency);
    Serial.printf("Voltage:%.2fV\n"                 ,Voltage  );
    Serial.printf("Current:%.2fA\n"                 ,Current  );
    Serial.printf("Active power:%.2fkW\n"           ,Apower   );
    Serial.printf("Reactive power:%.2fkvar\n"       ,Rpower   );
    Serial.printf("Apparent power:%.2fkva\n"        ,APpower  );
    Serial.printf("Power factor:%.2f\n"             ,Pfactor  );
    Serial.printf("Total Active power:%.2fkWh\n"    ,TApower  );
    Serial.printf("Total Reactive power:%.2fkvarh\n",TRpower  );
  #else
    Serial.print("\nEastron SDM120M reading...\n");

    // Modbus reads can be very slow so we need to check at each read if the buffer changed
    // Read all data
    float buffer = 0;
    float last_buffer = 0;

    // Frequency
    while (last_buffer == buffer)
    {
      Serial.printf("Freq\n");
      buffer = getFrequency();
    }
    last_buffer = buffer;
    Frequency = buffer;

    // Voltage
    while (last_buffer == buffer)
    {
      Serial.printf("Volt\n");
      buffer = getVoltage();
    }
    last_buffer = buffer;
    Voltage = buffer;

    // Current
    while (last_buffer == buffer)
    {
      Serial.printf("Curr\n");
      buffer = getCurrent();
    }
    last_buffer = buffer;
    Current = buffer;

    // ActivePower
    while (last_buffer == buffer)
    {
      Serial.printf("Ap\n");
      buffer = getActivePower();
    }
    last_buffer = buffer;
    Apower = buffer;

    // ReactivePower
    while (last_buffer == buffer)
    {
      Serial.printf("ReacP\n");
      buffer = getReactivePower();
    }
    last_buffer = buffer;
    Rpower = buffer;

    // ApparentPower
    while (last_buffer == buffer)
    {
      Serial.printf("AppP\n");
      buffer = getApparentPower();
    }
    last_buffer = buffer;
    APpower = buffer;

    // PowerFactor
    while (last_buffer == buffer)
    {
      Serial.printf("PF\n");
      buffer = getPowerFactor();
    }
    last_buffer = buffer;
    Pfactor = buffer;

    // TotalApparentPower
    while (last_buffer == buffer)
    {
      Serial.printf("TApP\n");
      buffer = getTotalApparentPower();
    }
    last_buffer = buffer;
    TApower = buffer;

    // TotalReactivePower
    while (last_buffer == buffer)
    {
      Serial.printf("TRepP\n");
      buffer = getTotalReactivePower();
    }
    last_buffer = buffer;
    TRpower = buffer;

    // ExportActivePower
    //while (last_buffer == buffer)
    //{
    //  Serial.printf("EAct\n");
    //  buffer = getExportActivePower();
    //}
    //last_buffer = buffer;
    //ExportActive = buffer;

    // ImportReactivePower
    //while (last_buffer == buffer)
    //{
    //  Serial.printf("IReact\n");
    //  buffer = getImportReactivePower();
    //}
    //last_buffer = buffer;
    //ImportReactive = buffer;

    // ExportReactivePower
    //while (last_buffer == buffer)
    //{
    //  Serial.printf("EReact\n");
    //  buffer = getExportReactivePower();
    //}
    //last_buffer = buffer;
    //ExportReactive = buffer;

    Serial.print("Eastron SDM120M output:\n");

    // Print all data
    Serial.printf("Voltage:%.2fV\n"                 ,Voltage          );
    Serial.printf("Frequency:%.2fHz\n"              ,Frequency        );
    Serial.printf("Current:%.2fA\n"                 ,Current          );
    Serial.printf("Active power:%.2fW\n"            ,Apower           );
    Serial.printf("Reactive power:%.2fvar\n"        ,Rpower           );
    Serial.printf("Apparent power:%.2fva\n"         ,APpower          );
    Serial.printf("Power factor:%.2f\n"             ,Pfactor          );
    Serial.printf("Total Active power:%.2fkWh\n"    ,TApower          );
    Serial.printf("Export active:%.2fkWh\n"         ,ExportActive     );
    Serial.printf("Import reactive:%.2fkWh\n"       ,ImportReactive   );
    Serial.printf("Export reactive:%.2fkWh\n"       ,ExportReactive   );
  #endif

  
}

void getPrintPublishAll(){
  // Read MODbus and print via serial
  getPrintAll();

  char temp_data[260];

  // Publish via MQTT
  #if METER_NAME == ORWE514
    sprintf(temp_data,"{\"%s\":{\"Frequency\":%.2f,\"Voltage\":%.2f,\"Current\":%.3f,\"Active power\":%.3f,\"Reactive power\":%.3f,\"Apparent power\":%.3f,\"Power factor\":%.3f,\"Total Active power\":%.3f,\"Total Reactive power\":%.3f}}",
    MODBUS_METER_NAME,Frequency,Voltage,Current,Apower,Rpower,APpower,Pfactor,TApower,TRpower);
  #else
    sprintf(temp_data,"{\"%s\":{\"Voltage\":%.2f,\"Frequency\":%.2f,\"Current\":%.3f,\"Active power\":%.3f,\"Apparent power\":%.3f,\"Reactive power\":%.3f,\"Power factor\":%.3f,\"Energy total\":%.3f,\"Export active\":%.3f,\"Import reactive\":%.3f,\"Export reactive\":%.3f}}",
    MODBUS_METER_NAME,Voltage,Frequency,Current,Apower,APpower,Rpower,Pfactor,TApower,ExportActive,ImportReactive,ExportReactive);
  #endif
  // Serial.print("Data to send MQTT");
  // Serial.println(temp_data);

  // client.publish(topic, "Before");

  client.publish(topic, temp_data);

  // client.publish(topic, "After");
}

void PublishAll(){
  char temp_data[280];

  // Publish via MQTT
  #if METER_NAME == ORWE514
    sprintf(temp_data,"{\"%s\":{\"Frequency\":%.2f,\"Voltage\":%.2f,\"Current\":%.3f,\"Active power\":%.3f,\"Reactive power\":%.3f,\"Apparent power\":%.3f,\"Power factor\":%.3f,\"Total Active power\":%.3f,\"Total Reactive power\":%.3f}}",
    MODBUS_METER_NAME,Frequency,Voltage,Current,Apower,Rpower,APpower,Pfactor,TApower,TRpower);
  #else
    sprintf(temp_data,"{\"%s\":{\"Voltage\":%.2f,\"Frequency\":%.2f,\"Current\":%.3f,\"Active power\":%.3f,\"Apparent power\":%.3f,\"Reactive power\":%.3f,\"Power factor\":%.3f,\"Energy total\":%.3f,\"Export active\":%.3f,\"Import reactive\":%.3f,\"Export reactive\":%.3f}}",
    MODBUS_METER_NAME,Voltage,Frequency,Current,Apower,APpower,Rpower,Pfactor,TApower,ExportActive,ImportReactive,ExportReactive);
  #endif

  client.publish(topic, temp_data);
}


void setup()
{
  // Enable builtin led
  digitalWrite(LED_BUILTIN,0);

  // use Serial (port 0) for USB communication
  Serial.begin(115200);

  // Initialize Modbus Ports
  AltSerial.begin(MODBUS_BAURATE,MODBUS_SERIAL_MODE,D1,D2);

  // communicate with Modbus slave ID 1 over Serial (port 0)
  node.begin(1, AltSerial);

  Serial.println("\n\nBegin:");

  // connecting to a WiFi network
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.println("Connecting to WiFi..");
  }

  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Connect MQTT
  client.setServer(mqtt_broker, mqtt_port);
  client.setCallback(callback);
  while (!client.connected()) {
      String client_id = "esp8266-client-";
      client_id += String(WiFi.macAddress());
      Serial.printf("%s is connecting to the public mqtt broker\n", client_id.c_str());
      if (client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
        Serial.print("MQTT connected\n");
      } else {
        Serial.print("failed with state ");
        Serial.print(client.state());
        Serial.print("\n");
        delay(2000);
    }
  }
  client.setBufferSize(350);

  // publish and subscribe
  client.publish(topic, MODBUS_METER_NAME" Online");
  client.subscribe(topic);

  // Website
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    char temp[1000];
    sprintf(temp,"<html><head><script>function refresh(refreshPeriod){setTimeout('location.reload(true)', refreshPeriod);}window.onload = refresh(6000);</script></head>><body><style>textarea {	resize: vertical;	width: 200px;	height: 318px;	padding: 5px;	overflow: auto;	background: #1f1f1f;	color: #65c115;}body {	text-align: center;	font-family: verdana, sans-serif;	background: #252525;}	</style><div style='text-align:left;display:inline-block;color:#eaeaea;min-width:340px;'><h1>%s</h1><b>Frequency:</b> %.2f Hz<br><b>Voltage:</b> %.2f V<br><b>Current:</b> %.2f A<br><b>Active power:</b> %.2f kW<br><b>Reactive power:</b> %.2f kvar<br><b>Apparent power:</b> %.2f kva<br><b>Power factor:</b> %.2f<br><b>Total Active power:</b> %.2f kWh<br><b>Total Reactive power:</b> %.2f kvarh<br><b><br>MQTT Broker IP:</b> %s<br><b>MQTT Topic:</b> %s<br><b>MQTT User:</b> %s<br><b>MQTT Pass:</b> %s </div></body></html>",
    MODBUS_METER_NAME,Frequency,Voltage,Current,Apower,Rpower,APpower,Pfactor,TApower,TRpower,mqtt_broker,topic,mqtt_username,mqtt_password);
        request->send(200, "text/html", temp);
    });


  server.begin();

  // Disable builtin led
  digitalWrite(LED_BUILTIN,1);
}

#define POST_EVERY_MS 6000
uint32_t print_time = millis();
#define READ_EVERY_MS 2000
uint32_t read_time = millis();

void loop()
{
  // Post data each POST_EVERY_MS
  if (millis()- print_time > POST_EVERY_MS){
    print_time = millis();
    PublishAll();
  }

  // Read data each READ_EVERY_MS
  if (millis()- read_time > READ_EVERY_MS){
    read_time = millis();
    getPrintAll();
  }

  // Handle MQTT disconnects
  while (!client.connected()) {
      String client_id = "esp8266-client-";
      client_id += String(WiFi.macAddress());
      Serial.printf("%s is connecting to the public mqtt broker\n", client_id.c_str());
      if (client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
        Serial.print("MQTT connected\n");
      } else {
        Serial.print("failed with state ");
        Serial.print(client.state());
        Serial.print("\n");
        delay(500);
    }
  }

  // Handle Wifi disconnects
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Reconnecting to WIFI network");
    WiFi.disconnect();
    WiFi.reconnect();
    delay(3000);
  }
 
}