// функции UDP 

//=========================================================================================
// функция отправки по UDP

unsigned long Last_UDP_send_time;             // время крайней отправки по udp
const int     UDP_SEND_PERIOD = 1000;         // частота отправки пакетов UDP

void Send_UDP (char data[UDP_TX_PACKET_MAX_SIZE]) {
  if ((long)millis() - Last_UDP_send_time > UDP_SEND_PERIOD) {
    Last_UDP_send_time = millis();  
    Udp.beginPacket(IP_Toilet_controller, localPort);
    Udp.write(data);
    Udp.endPacket();
  }
}


//=========================================================================================
// прием пакетов по UDP

unsigned long Last_UDP_receive_time;             // время крайнего приема по udp
const int     UDP_RECEIVE_WAIT_TIME = 10 * 1000; // время устаревания данных, принятых по UDP

void Receive_UDP (void) {
  int packetSize = Udp.parsePacket();
  if (packetSize)  {
    Last_UDP_receive_time = millis(); 
    int len = Udp.read(Buffer, UDP_TX_PACKET_MAX_SIZE); 
    if ((len == 2) && (Buffer[0] == 'm') && (Buffer[1] == '1')) Motion_flag = true;         // есть движение
    if ((len == 2) && (Buffer[0] == 'm') && (Buffer[1] == '0')) Motion_flag = false;        // движения нет
    if ((len == 2) && (Buffer[0] == 'h') && (Buffer[1] == '1')) High_humidity_flag = true;  // высокая влажность
    if ((len == 2) && (Buffer[0] == 'h') && (Buffer[1] == '0')) High_humidity_flag = false; // низакя влажность
  }
  // если нет приема по UDP - сбрасываем флаги
  if ((long)millis() - Last_UDP_receive_time > UDP_RECEIVE_WAIT_TIME) {
    Motion_flag = false;
    High_humidity_flag = false;
  }                                                               
}
