/* Прошивка для датчика протечки
 * 
 * Подключение:
 * GPIO0 - выход на светодиодную ленту
 * GPIO2 - DO сенсора протечки
 * VCC   - пиание +3.3V
 * CH_PD - питание +3.3V через резистор 10к (без него модуль не работает)
 * GND   - питание GND
 * 
 * v01 
 * функции Blynk
 * 
 * v02 - 25/10/19
 * добавлена паредача пакетов по UDP 
 * добавлены функции OTA
 * 
 * v03 - 26/10/19
 * добавлено управленеие адресной лентой
 * добавлено получение пакетов по UDP для управления лентой: m0 - движения нет, m1 - есть движение
 * 
 * v04 - 31/10/19
 * изменен алгоритм подключения к wi-fi в функции Connect
 * 
 * v05 - 08/11/19
 * заменена библиотека управления адресной лентой на FastLED
 * 
 * v6 - 12/11/19
 * добавлены эффекты для ленты и управление эффектами через blynk
 * 
 * v7 - 14/11/19
 * добавлен эффект SEA_MODE
 * 
 * v8 - 28/11/19
 * Устранена проблема с переподключением модуля к wi-fi
 * 
 * v9 - 17/12/19
 * Добавлена функция переподключения к Blynk серверу
 * 
 * v10 - 30/12/19
 * функции подключения к wi-fi и blynk вынесены в отдельный файл.
 * OTA-апдейт заменен на WEB-апдейт
 * 
 * v11 - 14/01/20
 * функция выбора режима ленты вынесена в effects()
 * доработана функция определения движения
 * 
 * v12 - 07/02/2020
 * Добавлена функция отправки E-mail при протечке
 * 
 * v20 - 25/03/2020
 * отказ от Blynk, переход на mqtt
 * переход на новые версии модулей connect_wi-fi_v102, effects_v102
 * упрощен алгоритм чтения сигнала с датчика протечки
 * 
 * v201 - 27/03/2020
 * исправлены ошибки подключения к mqtt
 * отлажена работа в УКБП
 * 
 * v202 - 05/04/2020
 * добавлен топик синхронизации с MQTT
 * 
 * v22 - 09/05/2020
 * добавлено отключение по времени MAX_MANUAL_PERIOD
 * 
 * v23 - 23/05/2020
 * топики mqtt разделены на топики управления ctrl и топики статуса state
 * добавлены флаги retain вместо топиков синхронизации (connect_mqtt_v03)
 * 
 * v30 - 26/05/2021
 * переход mqtt на clusterfly
 */

 

#define PIN_LED          1  // встроенный светодиод
#define PIN_water_sensor 2  // вход датчика влажности
#define PIN_led_strip    0  // адресаня лента

#define OFF      0
#define RAINBOW  1
#define HELLO    4
#define ALARM    10


#include <FastLED.h>
#define LEDS_count 30       // число пикселей в адресной ленте (в ванной)
CRGB Strip[LEDS_count];


#include <ESP8266WiFi.h>
const char ssid[] = "welcome's wi-fi";
const char pass[] = "27101988";
const bool NEED_STATIC_IP = true;
IPAddress IP_Node_MCU          (192, 168, 1, 71);
IPAddress IP_Fan_controller    (192, 168, 1, 41);
IPAddress IP_Water_sensor_bath (192, 168, 1, 135); 
IPAddress IP_Toilet_controller (192, 168, 1, 54);


#include <WiFiUdp.h>
WiFiUDP Udp;
unsigned char ip_toilet_realy[] = {192,168,43,88};
unsigned int localPort = 8888;
char Buffer[UDP_TX_PACKET_MAX_SIZE]; 


#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;


#include <PubSubClient.h>
WiFiClient Bath_esp8266;
PubSubClient client(Bath_esp8266);
const char* mqtt_client_name = "Bath_esp8266";    // Имя клиента


const int CHECK_PERIOD = 2*60*1000;       // периодичность проверки на подключение к сервисам
const int RESTART_PERIOD = 30*60*1000;    // время до ребута, если не удается подключиться к wi-fi
const int MAX_MANUAL_PERIOD = 20*60*1000; // максимальное время работы в ручном режиме 
const int DELAY_ALARM_TIME = 500;         // время до срабатывания сигнализации протечки (защита от дребезга)

bool Water_sensor_flag = false;      // флаг сигнала от датчика протечки
bool Water_alarm_flag = false;       // флаг тревоги протечки воды
bool Motion_flag = false;            // флаг наличия движения
bool High_humidity_flag = false;     // флаг высокой влажности
byte Manual_mode = OFF;              // режим ленты, управляемый через Blynk
bool Send_Email_flag = false;        // флаг отправленного сообщения по данной протечке

unsigned long Last_online_time;      // время когда модуль был онлайн
unsigned long Last_check_time;       // время проверки подключения к wi-fi и сервисам
unsigned long Manual_mode_time;      // время управления лентой в ручном режиме
unsigned long Water_alarm_flag_time; // время срабатывания сенсора протечки

byte LED_effect = OFF;                        // текущий эфффект светодиодной ленты
byte last_LED_effect = OFF;                   // эфффект светодиодной ленты на предыдущем такте 

// топик управления лентой
const char topic_led_ctrl[] = "/sv.lipatnikov@gmail.com/bath/led_ctrl";

// топики состояний
const char topic_water_alarm[] = "/sv.lipatnikov@gmail.com/bath/alarm_state";
const char topic_led_state[] = "/sv.lipatnikov@gmail.com/bath/led_state";


//=========================================================================================

void setup() {
  FastLED.addLeds<WS2811, PIN_led_strip, GRB>(Strip, LEDS_count).setCorrection( TypicalLEDStrip );
  FastLED.setBrightness(200); //0-255
  FastLED.clear();
  
  pinMode(PIN_LED, OUTPUT); digitalWrite(PIN_LED, LOW); 
  pinMode(PIN_water_sensor, INPUT);

  Connect_WiFi(IP_Water_sensor_bath, NEED_STATIC_IP); 
  Connect_mqtt(mqtt_client_name);
  MQTT_subscribe();
}

//=========================================================================================

void loop() {  
  // функции работы с датчиком протечки
  bool Water_alarm_flag = Read_water_sensor();
   
  // управление светодиодной лентой и встроенным светодиодом
  if ((long)millis() - Manual_mode_time > MAX_MANUAL_PERIOD) 
    Manual_mode = OFF;
    
  if (Water_alarm_flag)        LED_effect = ALARM; 
  else if (Manual_mode)        LED_effect = Manual_mode;  
  else if (Motion_flag)        LED_effect = HELLO;  
  else if (High_humidity_flag) LED_effect = RAINBOW;
  else                         LED_effect = OFF;
  LED_strip(LED_effect);
  
  // отправка режима на сервер mqtt
  if (LED_effect != last_LED_effect) {
    MQTT_publish_int(topic_led_state, LED_effect);
    last_LED_effect = LED_effect;      
  }

  // сетевые функции  
  httpServer.handleClient();                 // для обновления по воздуху  
  client.loop();                             // для функций MQTT 
  if (Water_alarm_flag) Send_UDP("w0");      // отправляем сообщение по UDP w0=протечка воды! 
  Receive_UDP();                             // прием UDP для определения движения

  
  // проверка подключений к wifi и серверам
  if ((long)millis() - Last_check_time > CHECK_PERIOD) {
    Last_check_time = millis(); 
    
    // wi-fi  
    if (WiFi.status() != WL_CONNECTED) { 
      Connect_WiFi(IP_Water_sensor_bath, NEED_STATIC_IP);
      Restart(Last_online_time, RESTART_PERIOD);
    }
    else 
      Last_online_time = millis();     
      
    // mqtt
    if (!client.connected()) {
      Connect_mqtt(mqtt_client_name);
      MQTT_subscribe();
    }      
  }  
}


//=========================================================================================
// чтение датчика протечки

bool Read_water_sensor() {
  if (digitalRead(PIN_water_sensor) == HIGH) {      // если НЕТ сигнала от датчика протечки
    if (Water_sensor_flag)
      MQTT_publish_int(topic_water_alarm, 0);       // если протечка прекратилась отправляем 0
    Water_sensor_flag = false;                      // сбрасываем флаг протечки
  }  
  else if (Water_sensor_flag == false) {            // если сигнал от датчика есть но флага нет (начало сигнала)             
    Water_sensor_flag = true;
    Water_alarm_flag_time = millis();
    MQTT_publish_int(topic_water_alarm, 1);         // если протечка есть отправляем 1
  } 
  
  if (Water_sensor_flag && ((long)millis() - Water_alarm_flag_time >= DELAY_ALARM_TIME) )  return true;
  else return false; 
}
