//===================================================================================================
// подключение к MQTT 
/*
const char *mqtt_server = "mqtt.dioty.co";          // Имя сервера MQTT
const int   mqtt_port = 1883;                       // Порт для подключения к серверу MQTT
const char *mqtt_user = "sv.lipatnikov@gmail.com";  // Логин от сервера
const char *mqtt_pass = "83eb5858";                 // Пароль от сервера
*/

const char *mqtt_server = "srv2.clusterfly.ru";  // Имя сервера MQTT
const int   mqtt_port = 9991;                    // Порт для подключения к серверу MQTT
const char *mqtt_user = "user_1502445e";         // Логин от сервера
const char *mqtt_pass = "pass_943f7409";         // Пароль от сервера

void Connect_mqtt(const char* client_name) {
  if (WiFi.status() == WL_CONNECTED) {
    client.setServer(mqtt_server, mqtt_port);            
    if (client.connect(client_name, mqtt_user, mqtt_pass)) 
      client.setCallback(mqtt_get);
  }
}


// Функция отправки int на сревер mqtt
void MQTT_publish_int(const char* topic , int data){
  char msg[5];
  sprintf (msg, "%u", data);    
  client.publish(topic, msg, true);
}


// Функция отправки float на сревер mqtt
void MQTT_publish_float(const char* topic , float data){
  char msg[4];
  sprintf (msg, "%2.1f", data);    
  client.publish(topic, msg, true);
}


// Функция отправки str на сревер mqtt
void MQTT_publish_str(const char* topic , char* data){    
  client.publish(topic, data, true);
}



//=========================================================================================
//функции MQTT

// функция подписки на топики !!!
void MQTT_subscribe(void) {
  if (client.connected()){
    client.subscribe(topic_led_ctrl);
  }
}

// получение данных от сервера
void mqtt_get(char* topic, byte* payload, unsigned int length) {
  // создаем копию полученных данных
  char localPayload[length + 1];
  for (int i=0;i<length;i++) { localPayload[i] = (char)payload[i];  }
  localPayload[length] = 0;

  if (strcmp(topic, topic_led_ctrl) == 0) {  
    int ivalue = 0; sscanf(localPayload, "%d", &ivalue);
    Manual_mode = (byte)ivalue; 
    if (Manual_mode) Manual_mode_time = millis();
  }
}
