# JZ-F407-MQTT-CAN

## Główny cel projektu

Głównym celem jest zdalna aktualizacja firmware’u docelowego MCU przez CAN (OTA), realizowana przez bramkę. Nowy projekt bramki rozwijamy wyłącznie dla STM32F407, w tym repozytorium `.STM32/JZ-F407-MQTT-CAN`.

Nowa funkcja musi zachować kompatybilność ze starszą komunikacją CAN: istniejące identyfikatory, formaty ramek, znaczenie payloadów i zachowanie dotychczasowych poleceń pozostają niezmienione. NUCLEO F207 jest punktem odniesienia dla tego protokołu, a nie miejscem implementacji OTA.

Mapa pinów i interfejsów płytki znajduje się w [JZ-F407.MD](./JZ-F407.MD), opracowanym na podstawie [schematu](./DOC/Schematic_STM32F407VET6.pdf).

## Stan implementacji

OTA przez CAN nie jest jeszcze zaimplementowane. Pliki `JZ-F407-MQTT-CAN.ioc` i `.cproject` wskazują już STM32F407VETx. Skonfigurowano USART1/RS232, I2C2, CAN1 50 kbit/s oraz RMII zgodnie z mapą płytki. USB CDC jest wyłączone, a `printf` korzysta z USART1. Zachowane pliki F207 pozostają w katalogu wyłącznie do porównania i są wykluczone z kompilacji. Nadal nie jest to gotowy firmware: przed budowaniem lub wgrywaniem otwórz `.ioc` w STM32CubeMX z pakietem STM32CubeF4, zweryfikuj zegar HSE 25 MHz, sterownik DP83848 oraz jego adres PHY zgodny ze strapami płytki, a następnie wygeneruj kod. Po generowaniu przejrzyj cały diff, szczególnie zmiany w plikach aplikacji i zachowanie Ethernetu. Sekcje poniżej opisują odziedziczoną bazę kodu F207, a nie gotową specyfikację sprzętową nowej bramki.

## Bieżąca baza kodu: NUCLEO F207 MQTT CAN

Firmware dla mikrokontrolera STM32F207ZGT6, który łączy sieć Ethernet i broker MQTT z magistralą CAN. Projekt zawiera również prosty serwer HTTP, wyświetlacz OLED oraz interfejs USB CDC do diagnostyki.

## Funkcje

- Odbieranie i wysyłanie ramek CAN.
- Publikowanie danych z CAN w MQTT oraz przekazywanie wybranych poleceń MQTT do CAN.
- Odczyt parametrów rekuperatora z ramek CAN i sterowanie poziomem wentylatora.
- Prosta strona WWW do sterowania diodami LED i podglądu logu.
- Wyświetlanie logu na OLED SSD1306.
- Zadania i kolejki komunikatów oparte na FreeRTOS.

## Bieżąca konfiguracja sprzętowa kodu bazowego

Poniższe wymagania i ustawienia dotyczą obecnej kopii firmware’u F207. Nie opisują docelowej płyty F407 dla projektu OTA.

- Płytka z mikrokontrolerem STM32F207ZGT6 i interfejsem Ethernet RMII.
- Połączenie Ethernet z siecią, w której dostępny jest broker MQTT.
- Transceiver CAN dopasowany do magistrali CAN; linie MCU nie zastępują transceivera.
- Opcjonalnie: wyświetlacz OLED SSD1306 podłączony do I2C2.
- Komputer z STM32CubeIDE i programator/debugger ST-LINK do wgrywania firmware’u.

Konfiguracja projektu używa CAN1 na pinach PD0 (RX) i PD1 (TX), z szybkością około 50 kbit/s. Magistrala musi mieć właściwe okablowanie i terminację zgodną z użytym transceiverem.

## Konfiguracja sieci

Firmware używa statycznej konfiguracji IPv4:

| Ustawienie | Wartość domyślna |
|---|---|
| Adres urządzenia | `192.168.1.249` |
| Maska | `255.255.255.0` |
| Brama | `192.168.1.1` |
| Broker MQTT | `192.168.1.2:1883` |
| Identyfikator klienta MQTT | `stm32` |

Przed uruchomieniem sprawdź, czy adres urządzenia nie jest zajęty i czy broker jest osiągalny z tej samej sieci. DHCP jest wyłączone. Adres IP urządzenia zmienia się w `LWIP/App/lwip.c`, a adres brokera w `Core/Src/my_mqtt.c`. Po zmianie konfiguracji przebuduj i wgraj firmware.

## Generowanie kodu F407 i budowanie

### Diody LED

- LED1 / PE13 zmienia stan co sekundę jako bicie serca głównego zadania.
  Miganie nie potwierdza stanu Ethernetu ani HTTP.
- LED2 / PE14 i LED3 / PE15 są sterowane wyłącznie przez formularz WWW.
  Pierwsze pole (`led=1`) steruje LED3, drugie (`led=2`) LED2.
  Zaznaczenie pola i wysłanie formularza zapala diodę, odznaczenie gasi.
- Zgodnie ze schematem płytki diody są aktywne stanem niskim:
  `GPIO_PIN_RESET` zapala, a `GPIO_PIN_SET` gasi.

Tymczasowa diagnostyka nasłuchu/łącza/SSI diodami została usunięta.
MQTT, LCD i zadanie przycisku pozostają wyłączone.

### CAN — etap 1: odbiór i logi

CAN1 działa w trybie normalnym przy 50 kbit/s, na PD0 (RX) i PD1 (TX).
Potwierdza odbiór ramek przez ACK, ale aplikacja nie wysyła ramek ani poleceń.
Filtr przyjmuje wszystkie identyfikatory do FIFO0. Callback odbioru przekazuje
ramkę do kolejki; logowanie odbywa się w zadaniu CAN, nie w przerwaniu.
Nie są wywoływane odziedziczone funkcje przekazujące CAN do MQTT.

Log WWW/USART1 zawiera wyniki konfiguracji filtra, startu i włączenia przerwań,
a następnie odebrane ramki, np. `CAN RX S:00000108 DATA L=4 08150001`.
`S` oznacza ID standardowe, `E` rozszerzone, `L` długość danych; ramki `RTR`
nie mają danych. Błędy inicjalizacji zatrzymują zadanie CAN, nie WWW.
Zmiany błędów HAL/ESR oraz liczników błędów odczytu i utraconych ramek kolejki
są raportowane najwyżej raz na sekundę. Wpisy otrzymują bieżący czas działania.

Kolejka RX mieści 32 ramki po 36 bajtów (nagłówek HAL i pełne 8 bajtów danych).
Rozmiar został zaktualizowany w `.ioc` i sprawdzony po regeneracji CubeMX
w osobnym katalogu; przeniesiono tylko zmianę kolejki, bez nadpisywania
działającej konfiguracji Ethernetu ani ponownego włączania innych zadań.
Log WWW pokazuje ostatnie 7 wpisów; przy dużym ruchu logowanie każdej ramki
może przepełnić kolejkę — licznik `queue_drop` ujawnia tę utratę.
Test startu CAN nie potwierdza jeszcze poprawnej komunikacji z innym węzłem.

### Konfiguracja buforów Ethernet dla WWW

W `LWIP/Target/lwipopts.h` ustawiono `LWIP_NETIF_TX_SINGLE_PBUF=1`:
LwIP składa dane TCP w pojedyncze bufory RAM przed przekazaniem do Ethernetu.
Sterownik odrzuca pakiety z więcej niż czterema buforami, a nagłówki HTTP
i treść strony mogą przekroczyć ten limit przy wysyłaniu bez kopiowania.
Opcja zwiększa koszt kopiowania i użycie RAM, ale nie zmienia protokołu ani
treści strony.
`LWIP_SUPPORT_CUSTOM_PBUF=1` zachowuje obsługę własnych buforów odbiorczych
sterownika Ethernet, niezależnie od ustawienia pojedynczego bufora TX.
Po wgraniu tej konfiguracji na JZ-F407 potwierdzono w dwóch kolejnych seriach
pełne odpowiedzi HTTP 200 dla `/` i `/index.shtml` oraz HTTP 404 dla `/404.html`.
Ping 4/4 również przeszedł. Testowy serwer TCP echo został usunięty;
firmware nie uruchamia nasłuchu na porcie 2323. MQTT pozostaje wyłączone;
CAN uruchamiany jest etapowo, zgodnie z opisem powyżej.

Konfiguracja projektu wskazuje F407, ale wygenerowany kod nie został jeszcze zweryfikowany. Nie wgrywaj dotychczasowych artefaktów z katalogu `Debug` na płytkę F407.

1. Otwórz `JZ-F407-MQTT-CAN.ioc` w STM32CubeMX i sprawdź, czy wybrany układ to STM32F407VET6.
2. Sprawdź mapę pinów i zegary: I2C2 musi używać PF0 (SDA) i PF1 (SCL), USART1 PA9/PA10, a źródłem PLL dla 168 MHz ma być HSE 25 MHz. Dla RMII potwierdź zgodność wygenerowanej obsługi PHY z DP83848 na płytce; nie zakładaj, że sterownik LAN8742 z bazy F207 jest zgodny.
3. Wygeneruj kod do katalogu projektu. Zachowaj istniejący kod aplikacji i przejrzyj cały diff, zwłaszcza pliki zawierające bloki `USER CODE`. Potwierdź, że powstał `Core/Startup/startup_stm32f407vetx.s`; zachowany plik startowy F207 nie może być użyty.
4. W **Project Manager → Toolchain / IDE** wybierz **CMake** oraz kompilator **GCC**, po czym wygeneruj kod. CubeMX utworzy `CMakeLists.txt`, `mx-generated.cmake` oraz `gcc-arm-none-eabi.cmake` w katalogu projektu.
5. Otwórz katalog projektu w VS Code, zainstaluj rekomendowane rozszerzenia, wybierz preset `debug` i wykonaj **CMake: Configure** oraz **CMake: Build**. Wymagane są STM32CubeCLT z CMake, Ninja i `arm-none-eabi-gcc` dostępnymi w `PATH`.
6. Przed wgrywaniem sprawdź wynik kompilacji oraz konfigurację pamięci Flash/linkera dla 512 KB. Test komunikacji Ethernet i CAN wykonaj na docelowej płytce.

STM32CubeMX pozostaje narzędziem do edycji `.ioc` i generowania kodu. STM32CubeIDE nie jest wymagane do konfiguracji ani budowania projektu.

## MQTT

Wszystkie tematy publikowane przez firmware mają prefiks `DataSoft/`. Publikacja używa QoS 0 i nie ustawia retained message. Po połączeniu urządzenie subskrybuje `DataSoft/can/#` oraz `DataSoft/rekuperator/set/#`.

### Dane z CAN

Przykładowe tematy publikowane dla standardowych ramek CAN:

| Temat | Znaczenie / wartość |
|---|---|
| `DataSoft/can/<ID>/roleta` | Stan rolety: `ON`, `OFF` albo `STOP` |
| `DataSoft/can/<ID>/roleta_pos` | Wartość pozycji rolety |
| `DataSoft/can/<ID>/switch1` | Stan przełącznika 1: `ON` albo `OFF` |
| `DataSoft/can/<ID>/switch2` | Stan przełącznika 2: `ON` albo `OFF` |
| `DataSoft/can/<ID>/sensor1` | Stan czujnika 1: `ON` albo `OFF` |
| `DataSoft/can/<ID>/sensor2` | Stan czujnika 2: `ON` albo `OFF` |
| `DataSoft/can/<ID>/prad` | Wartość pomiaru prądu |
| `DataSoft/can/<ID>/temp` | Wartość temperatury przekazana przez urządzenie CAN |

`<ID>` to identyfikator urządzenia w ramce CAN. Przykładowo `DataSoft/can/1/roleta`.

Aby wysłać polecenie do urządzenia CAN, użyj tematu z segmentem `set`, na przykład:

```text
Temat:   DataSoft/can/1/set/roleta
Payload: ON
```

Obsługiwane końcówki poleceń to `roleta`, `roleta_pos`, `switch1`, `switch2`, `sensor1` i `sensor2`. Dla rolety akceptowane są wartości `ON`, `OFF` i `STOP`; pozostałe wartości są interpretowane jako liczby. Polecenie jest wysyłane standardową ramką CAN o identyfikatorze `0x100 + <ID>`.

### Struktura ramki CAN

Projekt korzysta z klasycznych ramek CAN (CAN 2.0), które mogą przenosić do 8 bajtów danych. Ramka danych składa się z pól:

| Pole | Znaczenie |
|---|---|
| SOF | Początek ramki |
| Identyfikator i pola arbitrażu | Określają priorytet oraz typ ramki; identyfikator standardowy ma 11 bitów, a rozszerzony 29 bitów |
| Pole sterujące | Zawiera m.in. informację o formacie identyfikatora i długości danych (DLC: 0–8) |
| Dane | Od 0 do 8 bajtów, których znaczenie określa protokół urządzenia |
| CRC, ACK, EOF | Kontrola poprawności, potwierdzenie odbioru i koniec ramki |

W typowej ramce danych używanej przez projekt `RTR=0`. Ramka może mieć identyfikator standardowy (`IDE=0`) albo rozszerzony (`IDE=1`). Sam identyfikator CAN nie definiuje znaczenia bajtów danych — ich układ wynika z protokołu aplikacyjnego.

#### Wspólny format bramki i sterownika GW_MCU

Oba projekty używają standardowego identyfikatora 11-bitowego `StdId = 0x100 + id_device`. W sterowniku GW_MCU domyślne `id_device` wynosi `8`, więc jego identyfikator CAN to `0x108`. Sterownik filtruje ramki po tym identyfikatorze, a następnie interpretuje ich dane:

Konfiguracje obu mikrokontrolerów są ustawione na 50 kbit/s: NUCLEO F207 ma zegar CAN APB1 30 MHz, preskaler 40 i 15 kwantów czasu na bit; GW_MCU ma APB1 32 MHz, preskaler 40 i 16 kwantów na bit. Dzięki różnym podziałom zegara i segmentom czasowym obie strony uzyskują tę samą szybkość.

| Bajt | Pole | Znaczenie |
|---:|---|---|
| 0 | `to` | Kierunek/adres w protokole aplikacyjnym |
| 1 | `fun` | Kod funkcji |
| 2 | `val` | Wartość |
| 3 | `valL` | Młodszy bajt wartości; występuje w ramce pomiaru prądu |

Pole `to` ma różne wartości zależnie od kierunku. Polecenie z bramki do sterownika ma `to=0` (adres odbioru przez MCU). Sterownik GW_MCU ustawia `to=id_device` w swoich ramkach zwrotnych, a bramka używa tej wartości jako `<ID>` w temacie MQTT. Wartość 16-bitową składa się jako `256 * val + valL` (najpierw bajt starszy). Polecenia bramki mają `DLC=3`; raport pomiaru prądu ze sterownika ma `DLC=4`.

Kody funkcji wspólne lub rozpoznawane przez bramkę:

| `fun` | Znaczenie | Obsługa w GW_MCU |
|---:|---|---|
| `0` | Temperatura | Nie jest wysyłana przez ten sterownik |
| `1` | Sterowanie / stan rolety | Polecenie: `0` = zamykanie, `1` = otwieranie; raport: `0` = zamknięta, `1` = otwarta |
| `2`, `3` | Wyjście przełącznika 1 lub 2 | Odbiera polecenie i wysyła stan `0`/`1` |
| `4`, `5`, `6` | Czujniki binarne lub pozycja rolety | Bramka potrafi je mapować, ale GW_MCU ich nie obsługuje |
| `21` | Pomiar analogowy / prąd | Wysyła 16-bitową wartość w `val` i `valL` |
| `100` | Konfiguracja identyfikatora | Odbiera nowy, niezerowy identyfikator |
| `101` | Konfiguracja typu urządzenia | Zdefiniowana, ale odbiór jest obecnie błędny w GW_MCU |

Wiadomości MQTT do funkcji `4`, `5` i `6` nie są więc poleceniami obsługiwanymi przez firmware GW_MCU, mimo że bramka zna te kody dla integracji z innymi urządzeniami.

#### Urządzenia roletowe i wejścia/wyjścia

Dla polecenia MQTT, np. `DataSoft/can/8/set/roleta`, bramka wysyła ramkę z `StdId=0x100+8`, `DLC=3` i bajtami `00 01 01` dla polecenia `ON`. Przykładowe dane polecenia roletowego: `to=0`, `fun=1`, `val=1`. Dla `OFF` wartość `val` wynosi `0`, a dla `STOP` `255`. Polecenia przełączników używają odpowiednio `fun=2` albo `fun=3` i wartości `0`/`1`.

Sterownik odpowiada ramką o tym samym identyfikatorze standardowym; jej `to` zawiera identyfikator sterownika. Dla wyjścia przełącznika odpowiedź zawiera kod funkcji `2` lub `3` i jego stan. Bramka wykorzystuje `to` do publikacji pod `DataSoft/can/<ID>/...`. Sterownik GW_MCU obsługuje tryb włącznika i rolety manualnej; pozostałe kody wymienione w tabeli nie oznaczają, że ta wersja urządzenia je realizuje.

W trybie rolety manualnej raport funkcji `1` jest wysyłany po zakończeniu ruchu i informuje o osiągniętym stanie: `val=1` oznacza roletę otwartą, a `val=0` zamkniętą. Choć bramka przyjmuje payload `STOP` i zamienia go na `val=255`, obecny firmware GW_MCU nie obsługuje zatrzymania z CAN — każda wartość inna niż `1`, w tym `255`, uruchamia zamykanie.

#### Rekuperator

Polecenie poziomu wentylatora korzysta z osobnej ramki z identyfikatorem rozszerzonym `ExtId = 0x1F015051` i `DLC = 8`. Dane mają postać `01 00 20 1C 00 00 <poziom> 00`, gdzie `<poziom>` (0–3) znajduje się w bajcie 6.

### Rekuperator

Odczytane parametry rekuperatora są publikowane pod `DataSoft/rekuperator/<parametr>`. Przykładowe parametry to `wywiew_temp`, `nawiew_temp`, `czerpnia_wilg`, `wydajnosc_wyw`, `bypass_status` i `energia_odzyskana_total`.

Sterowanie poziomem wentylatora:

```text
Temat:   DataSoft/rekuperator/set/wentylator_poziom
Payload: 2
```

Obsługiwane wartości payloadu to `0`–`3`.

## Interfejs WWW i diagnostyka

Po podłączeniu do tej samej sieci otwórz `http://192.168.1.249/`. Strona pozwala sterować dwiema diodami LED i wyświetla wpisy logu. Log jest również dostępny przez USB CDC oraz wyjście debuggera.

## Struktura projektu

| Katalog / plik | Zawartość |
|---|---|
| `Core/Src`, `Core/Inc` | Aplikacja, sterowniki peryferiów i moduły MQTT, CAN, HTTP, logowania oraz rekuperatora |
| `LWIP` | Konfiguracja sieci Ethernet i stosu LwIP |
| `USB_DEVICE` | Interfejs USB CDC |
| `Middlewares` | LwIP i FreeRTOS |
| `Drivers` | Obecnie pozostałości HAL STM32F2/CMSIS; pliki dla STM32F4 muszą zostać wygenerowane przed kompilacją |
| `mkfs` | Pliki strony WWW osadzone w firmware |
| `JZ-F407-MQTT-CAN.ioc` | Konfiguracja STM32CubeMX dla STM32F407VET6 |

## Licencje

Projekt zawiera komponenty STM32, FreeRTOS i LwIP. Warunki licencyjne komponentów zewnętrznych znajdują się w odpowiadających im katalogach `Drivers` i `Middlewares`.
