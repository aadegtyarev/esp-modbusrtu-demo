// Подключаем библиотеку modbus-esp8266
#include <ModbusRTU.h>

// Настройка Modbus
#define SLAVE_ID 1 // адрес нашего сервера
#define PIN_FLOW 4 // пин контроля направления приёма/передачи,
// если в вашем преобразователе UART-RS485 такого нет —
// закоменнтируйте строку

// Номера Modbus регистров
#define REG_TEST 0       // тестовый регистр с номером 0

ModbusRTU mb;

void setup() {
  modbus_setup();
}

void loop() {
  mb.task();
}

void modbus_setup() {
  Serial.begin(9600, SERIAL_8N2); // задаём парамеры связи
  mb.begin(&Serial);
  mb.begin(&Serial, PIN_FLOW); // включаем контроль направления приёма/передачи
  mb.slave(SLAVE_ID); // указываем адрес нашего сервера

  mb.addHreg(REG_TEST); // описываем регистр REG_TEST типа Holding
  mb.Hreg(REG_TEST, 3); // записываем в наш регистр REG_TEST число 3 —
  // его мы должны будем увидеть при опросе устройства
}
