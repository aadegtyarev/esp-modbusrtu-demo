/* Подключаем библиотеки */
#include <ModbusRTU.h> // modbus-esp8266
#include <EEPROM.h> // работа с EEPROM
#include <SimpleTimer.h>     // простой таймер

#include <Wire.h>            // Wire
#include <SparkFunCCS811.h>  // SparkFunCCS811

/* Объявления констант */
// Значение параметров связи по умолчанию
#define DEFAULT_MB_ADDRESS    1  // адрес нашего сервера
#define DEFAULT_MB_STOP_BITS  2  // количество стоповых битов
#define DEFAULT_MB_BAUDRATE   96 // скорость подключения/100

// Номера Modbus регистров
#define REG_SENSOR_ERROR  0 // ошибка инициализации сенсора CCS811
#define REG_SENSOR_ECO2   1 // eCO2
#define REG_SENSOR_TVOC   2 // TVOC
#define REG_SENSOR_BL     3 // Baseline

#define REG_MB_ADDRESS    100 // адрес устройства на шине
#define REG_MB_STOP_BITS  101 // количество стоповых битов
#define REG_MB_BAUDRATE   102 // скорость подключения

// Настройки EEPROM. Так как записывать в EEPROM будем числа типа uint16_t,
// которые на ESP8266/ESP32 занимают 4 байта, то каждое значение займёт две ячейки
#define EEPROM_SIZE           6 // мы займём 6 ячеек памяти: 3*2=6
#define EEPROM_MB_ADDRESS     0 // номер ячейки с адресом устройства
#define EEPROM_MB_STOP_BITS   2 // номер ячейки со стоп-битами
#define EEPROM_MB_BAUDRATE    4 // номер ячейки со скоростью

// Описание входов-выходов
#define PIN_FLOW 12 // D6 пин контроля направления приёма/передачи,
// если в вашем преобразователе UART-RS485 такого нет —
// закомментируйте строку
#define BTN_SAFE_MODE 14 // D5 на NodeMCU. Кнопка сброса настроек подключения

#define CCS811_ADDR 0x5A  // Указываем адрес устройства I2C, по умолчанию 0x5A, второй адрес устройства 0x5B
/* Переменные */
// Связь
uint16_t mbAddress = DEFAULT_MB_ADDRESS; // modbus адрес устройства
uint16_t mbStopBits = DEFAULT_MB_STOP_BITS; // количество стоповых битов
uint16_t mbBaudrate = DEFAULT_MB_BAUDRATE; // скорость подключения modbus

/*Прочие настройки */
ModbusRTU mb;
CCS811 ccs811(CCS811_ADDR);  // создаем объект для работы с сенсором CCS811
SimpleTimer sysTimer(200);      // запускаем таймер с интервалом 2c (200мс)

/* Функции инициализации  и главный цикл */
// Настройка устройства
void setup() {
  io_setup();         // настраиваем входы/выходы
  eeprom_setup();     // настраиваем EEPROM
  check_safe_mode();  // проверяем, не надо ли нам в безопасный режим с дефолтными настройками
  modbus_setup();     // настраиваем Modbus
  i2c_setup();          // инициализация i2c
}

// Главный цикл
void loop() {
  mb.task();
  check_timer();
}

void check_timer() {
  if (sysTimer.isReady()) { // выполняется раз в 2c
    read_sensor(); // опрашиваем сенсор
    sysTimer.reset(); // сбрасываем таймер
  }
}

// Настройка входов-выходов
void io_setup() {
  pinMode(BTN_SAFE_MODE, INPUT_PULLUP);
}

// Иниализаця UART и конфигурирование Modbus
void modbus_setup() {
  Serial.begin(convert_baudrate(mbBaudrate), convert_stop_bits_to_config(mbStopBits)); // задаём парамеры связи
  mb.begin(&Serial);
  mb.begin(&Serial, PIN_FLOW); // включаем контроль направления приёма/передачи
  mb.slave(mbAddress); // указываем адрес нашего сервера

  /* Описываем регистры */
  // настройки связи
  mb.addHreg(REG_MB_ADDRESS);   // адрес устройства на шине
  mb.addHreg(REG_MB_STOP_BITS); // стоповые биты
  mb.addHreg(REG_MB_BAUDRATE);  // скорость подключения
  mb.addIsts(REG_SENSOR_ERROR); // ошибка инициализации сенсора
  mb.addIreg(REG_SENSOR_ECO2);  // eCO2
  mb.addIreg(REG_SENSOR_TVOC);  // TVOC
  mb.addIreg(REG_SENSOR_BL);    // Baseline

  /*Инициализация регистров*/
  // записываем в регистры текущие значения адреса, стоповых битов и скорости
  mb.Hreg(REG_MB_ADDRESS, mbAddress);
  mb.Hreg(REG_MB_STOP_BITS, mbStopBits);
  mb.Hreg(REG_MB_BAUDRATE, mbBaudrate);
  mb.Ists(REG_SENSOR_ERROR, 0); // ошибка
  mb.Ireg(REG_SENSOR_ECO2, 0);  // eCO2
  mb.Ireg(REG_SENSOR_TVOC, 0);  // TVOC
  mb.Ireg(REG_SENSOR_BL, 0);    // Baseline

  /* Назначение колбек функций на изменение регистров*/
  // параметры связи
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

void i2c_setup() {
  Wire.begin();  // инициализация i2c

  if (!ccs811.begin()) {  // инициализация CCS811
    mb.Ists(REG_SENSOR_ERROR, 1); // если не получилось — ошибка, пишем в регистр ошибок
  } else {
    mb.Ists(REG_SENSOR_ERROR, 0);
  };
}

/* Функции работы с периферией */
// Проверка состояние кнопки безопасного режима
void check_safe_mode() {
  // Проверка кнопки безопасного режима
  if (digitalRead(BTN_SAFE_MODE)) { // если не нажата, то читаем настройки из EEPROM
    read_modbus_settings();
  }
}

// Опрос сенсора
void read_sensor() {
  unsigned int baseLine;
  
  if (ccs811.dataAvailable()) {                 // проверяем, есть ли нвоые данные
    ccs811.readAlgorithmResults();              // считываем
    mb.Ireg(REG_SENSOR_ECO2, ccs811.getCO2());  // записываем в регистр eCO2
    mb.Ireg(REG_SENSOR_TVOC, ccs811.getTVOC()); // записываем в регистр TVOC

    baseLine = ccs811.getBaseline();
    mb.Ireg(REG_SENSOR_BL, baseLine);    // Baseline
  }
}

/* Вспомогательные функции */
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

// запись значения в EEPROM
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
