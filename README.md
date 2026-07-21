# MQTT-CAN-NUCLEO-F207

Firmware dla płytki STM32 NUCLEO-F207ZGTX, który realizuje bramkę pomiędzy magistralą CAN a siecią Ethernet z MQTT i prostym serwerem HTTP.

Projekt został zbudowany w oparciu o STM32CubeMX/CubeIDE i korzysta z FreeRTOS, LwIP, CAN, USB CDC, UART, I2C oraz niezależnego watchdog-a. W kodzie widać też obsługę wyświetlacza OLED SSD1306 i własne moduły logiki aplikacyjnej.

## Co robi projekt

- odbiera ramki CAN i publikuje ich treść do MQTT,
- odbiera wiadomości MQTT i zamienia je na komendy CAN,
- wystawia prosty interfejs HTTP z obsługą CGI/SSI,
- udostępnia logi przez USB CDC oraz ITM,
- działa na stałej konfiguracji sieciowej bez DHCP.

## Główna architektura

- [Core/Src/main.c](Core/Src/main.c) inicjuje peryferia i uruchamia scheduler FreeRTOS,
- [Core/Src/freertos.c](Core/Src/freertos.c) definiuje zadania, kolejki i semafory,
- [Core/Src/my_can.c](Core/Src/my_can.c) mapuje ramki CAN na tematy MQTT i obsługuje transmisję CAN,
- [Core/Src/my_mqtt.c](Core/Src/my_mqtt.c) realizuje połączenie z brokerem i publikację/subskrypcję,
- [Core/Src/my_mcu.c](Core/Src/my_mcu.c) interpretuje komendy z MQTT i przygotowuje wysyłkę do urządzeń CAN,
- [Core/Src/my_httpd.c](Core/Src/my_httpd.c) dodaje prosty serwer WWW z CGI/SSI,
- [LWIP/App/lwip.c](LWIP/App/lwip.c) konfiguruje interfejs Ethernet i adresację IP.

## Konfiguracja sprzętowa

- MCU: STM32F207ZGT6,
- CAN1: aktywne i skonfigurowane w trybie odbioru z przerwaniami,
- Ethernet: RMII z PHY LAN8742A,
- USB FS: urządzenie CDC,
- UART3: używany do komunikacji/debugowania,
- I2C2: używany m.in. przez OLED SSD1306,
- IWDG: włączony jako zabezpieczenie przed zawieszeniem.

## Sieć i MQTT

- adres IP urządzenia: `192.168.1.101`,
- brama: `192.168.1.1`,
- maska: `255.255.255.0`,
- broker MQTT w kodzie: `192.168.1.2`,
- projekt publikuje dane pod własnymi tematami i subskrybuje polecenia z gałęzi `.../set`.

## Logika aplikacji

Firmware rozpoznaje wybrane identyfikatory CAN i mapuje je na nazwy tematyczne MQTT, np. dane o temperaturze, wilgotności, wydajności, energii oraz stany wejść/wyjść. W drugą stronę wiadomości MQTT są zamieniane na komendy CAN dla urządzeń typu roleta, przełącznik i czujnik.

## Uwagi

- W kodzie znajdują się komentarze diagnostyczne dotyczące wcześniejszych zawieszeń i testów stabilności.
- Projekt jest mocno sprzętowy i najłatwiej uruchamia się go w środowisku STM32CubeIDE na docelowej płytce lub zbliżonej konfiguracji.
