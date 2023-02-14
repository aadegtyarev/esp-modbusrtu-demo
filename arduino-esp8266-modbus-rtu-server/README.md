# arduino-esp8266-modbus-rtu-server
Пример Modbus RTU сервера на ESP8266, написано в Arduino.

Таблица регистров:

Адрес  | Тип			| Описание
------ | -------------	| ------------
0      | Holding		| Тестовый регистр
100    | Holding		| Modbus-адрес устройства
101    | Holding		| Стоп биты
102    | Holding		| Скорость обмена, делённая на 100

Дополнения:
- /adds/schematic.png — схема подключения;
- /adds/config-ad-demo.json — шаблон для драйвера wb-mqtt-serial, который используется в контроллерах Wiren Board.
