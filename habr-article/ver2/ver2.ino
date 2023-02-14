/* Подключаем библиотеки */
#include <ModbusRTU.h> // modbus-esp8266
#include <EEPROM.h> // работа с EEPROM

/* Объявления констант */
// Значение параметров связи по умолчанию
#define DEFAULT_MB_ADDRESS    1  // адрес нашего сервера
#define DEFAULT_MB_STOP_BITS  2  // количество стоповых битов
#define DEFAULT_MB_BAUDRATE   96 // скорость подключения/100

// Номера Modbus регистров
#define REG_MB_ADDRESS    100 // адрес устройства на шине
#define REG_MB_STOP_BITS  101 // количество стоповых битов
#define REG_MB_BAUDRATE   102 // скорость подключения

// Настройки EEPROM. Так как записывать в EEPROM будем числа типа uint16_t,
// которые на ESP8266/ESP32 занимают 4 байта, то каждое значение займёт две ячейки
#define EEPROM_SIZE           6 // мы займём 6 ячеек памяти: 3*2=6
#define EEPROM_MB_ADDRESS     0 // номер ячейки с адресом устройства
#define EEPROM_MB_STOP_BITS   2 // номер ячейки со стоп-битами
#define EEPROM_MB_BAUDRATE    4 // номер ячейки со скоростью

#define PIN_FLOW 4 // пин контроля направления приёма/передачи,
// если в вашем преобразователе UART-RS485 такого нет —
// закомментируйте строку

/*Объявляем переменные */
// Параметры связи
uint16_t mbAddress = DEFAULT_MB_ADDRESS; // modbus адрес устройства
uint16_t mbStopBits = DEFAULT_MB_STOP_BITS; // количество стоповых битов
uint16_t mbBaudrate = DEFAULT_MB_BAUDRATE; // скорость подключения modbus

ModbusRTU mb;

void setup() {
  eeprom_setup(); // настраиваем EEPROM
  modbus_setup();  // настраиваем Modbus
}

void loop() {
  mb.task();
}

// Иниализаця UART и конфигурирование Modbus
void modbus_setup() {
  Serial.begin(convert_baudrate(mbBaudrate), convert_stop_bits_to_config(mbStopBits)); // задаём парамеры связи
//    Serial.begin(9600, SERIAL_8N2); // задаём парамеры связи
  mb.begin(&Serial);
  mb.begin(&Serial, PIN_FLOW); // включаем контроль направления приёма/передачи
  mb.slave(mbAddress); // указываем адрес нашего сервера

  // описываем три holding регистра
  mb.addHreg(REG_MB_ADDRESS);   // адрес устройства на шине
  mb.addHreg(REG_MB_STOP_BITS); // стоповые биты
  mb.addHreg(REG_MB_BAUDRATE);  // скорость подключения

  // записываем в регистры значения адреса, стоповых битов и скорости
  mb.Hreg(REG_MB_ADDRESS, mbAddress);
  mb.Hreg(REG_MB_STOP_BITS, mbStopBits);
  mb.Hreg(REG_MB_BAUDRATE, mbBaudrate);

  // описываем колбек функцию, которая будет вызвана при записи регистров
  // параметров подключения
  mb.onSetHreg(REG_MB_ADDRESS, callback_set_mb_reg);
  mb.onSetHreg(REG_MB_STOP_BITS, callback_set_mb_reg);
  mb.onSetHreg(REG_MB_BAUDRATE, callback_set_mb_reg);
}

// Настройка параметров EEPROM
void eeprom_setup() {
  EEPROM.begin(EEPROM_SIZE);
}

// Чтение настроек Modbus: сперва читаем из EEPROM,
// а если там пусто, то берём значения по умолчанию
void read_modbus_settings() {
  EEPROM.get(EEPROM_MB_ADDRESS, mbAddress);
  if (mbAddress == 0xffff) {
    mbAddress = DEFAULT_MB_ADDRESS;
  }

  EEPROM.get(EEPROM_MB_STOP_BITS, mbStopBits);
  if (mbStopBits == 0xffff) {
    mbStopBits = DEFAULT_MB_STOP_BITS;
  }

  EEPROM.get(EEPROM_MB_BAUDRATE, mbBaudrate);
  if (mbBaudrate == 0xffff) {
    mbBaudrate = DEFAULT_MB_BAUDRATE;
  };
}

// Колбек функция в которой мы записываем полученные по Modbus регистры в EEPROMM.
// Не забываем проверять записываемые значения на корректность, иначе мы можем потерять
// связь с устройством
uint16_t callback_set_mb_reg(TRegister* reg, uint16_t val) {
  switch (reg->address.address) {
    case REG_MB_ADDRESS: // если записываем регистр с адресом
      if (val > 0 && val < 247) { // проверяем, что записываемое число корректно
        write_eeprom(EEPROM_MB_ADDRESS, val); // записываем значение в EEPROM
      } else {
        val = reg->value; // этот трюк сгенерирует ошибку записи, что нам и нужно, так как значение неверное
      }
      break;
    case REG_MB_STOP_BITS: // если регистр со стоповыми битами
      if (val == 1 || val == 2) {
        write_eeprom(EEPROM_MB_STOP_BITS, val);
      } else {
        val = reg->value;
      }
      break;
    case REG_MB_BAUDRATE: // если регистр со скоростью
      uint16_t correctBaudRates[] = {12, 24, 48, 96, 192, 384, 576, 1152};
      if (contains(val, correctBaudRates, 8)) {
        write_eeprom(EEPROM_MB_BAUDRATE, val);
      } else {
        val = reg->value;
      }
      break;
  }
  return val;
}

// Запись значения в EEPROM
void write_eeprom(uint8_t eepromm_address, uint16_t val) {
  EEPROM.put(eepromm_address, val);
  EEPROM.commit();
}

// Конвертер стоповых битов в настройки типа SerialConfig.
// Я не стал реализовывать все возможные варианты
// и давать настраивать чётность и количество битов данных, так как
// почти никто эти параметры при работе по протоколу Modbus не меняет
SerialConfig convert_stop_bits_to_config(uint16_t stopBits) {
  return (stopBits == 2) ? SERIAL_8N2 : SERIAL_8N1;
}

// Конвертер значения скорости. Для экономии места во флеше и регистрах, мы храним
// значение скорости, делённое на 100. То есть вместо 9600, мы храним 96.
// Эта функция умножает значение на 100.
uint32_t convert_baudrate(uint16_t baudrateValue) {
  return baudrateValue * 100;
}

// Функция, которая находит вхождение числа в массив
bool contains(uint16_t a, uint16_t arr[], uint8_t arr_size) {
  for (uint8_t i = 0; i < arr_size; i++) if (a == arr[i]) return true;
  return false;
}
