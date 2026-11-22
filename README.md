# JZ-F407-MQTT-CAN

## Główny cel projektu

Głównym celem jest zdalna aktualizacja firmware’u docelowego MCU przez CAN (OTA), realizowana przez bramkę. Nowy projekt bramki rozwijamy wyłącznie dla STM32F407, w tym repozytorium `.STM32/JZ-F407-MQTT-CAN`.

Nowa funkcja musi zachować kompatybilność ze starszą komunikacją CAN: istniejące identyfikatory, formaty ramek, znaczenie payloadów i zachowanie dotychczasowych poleceń pozostają niezmienione. NUCLEO F207 jest punktem odniesienia dla tego protokołu, a nie miejscem implementacji OTA.

Mapa pinów i interfejsów płytki znajduje się w [JZ-F407.MD](./JZ-F407.MD), opracowanym na podstawie [schematu](./DOC/Schematic_STM32F407VET6.pdf).

## Stan implementacji

OTA przez CAN nie jest jeszcze zaimplementowane. Pliki `JZ-F407-MQTT-CAN.ioc` i `.cproject` wskazują STM32F407VETx. Skonfigurowano USART1/RS232, CAN1 50 kbit/s, SDIO 4-bit z DMA/FatFs oraz RMII zgodnie z mapą płytki. I2C2/OLED jest obecnie wyłączone; zachowane źródła I2C2 i SSD1306 nie są kompilowane. USB CDC jest wyłączone, a `printf` korzysta z USART1. Kod kompiluje się przez CMake, ale nie został jeszcze przetestowany na płytce. Przed wgrywaniem zweryfikuj zegar HSE 25 MHz, sterownik DP83848 i jego adres PHY zgodny ze strapami płytki. Przy każdej zmianie `.ioc` wygeneruj kod ponownie i przejrzyj cały diff, szczególnie zmiany w plikach aplikacji i obsłudze Ethernetu. Zachowane pliki F207 pozostają w katalogu wyłącznie do porównania i są wykluczone z kompilacji. Sekcje poniżej opisują odziedziczoną bazę kodu F207, a nie gotową specyfikację sprzętową nowej bramki.

## Karta SD (JZ-F407)

SDIO używa PC8–PC11 jako D0–D3, PC12 jako CLK, PD2 jako CMD oraz PD3 jako wykrywanie karty (stan niski oznacza kartę włożoną). Transfer wykorzystuje DMA2 Stream 3 i FatFs. Osobne zadanie czeka na kartę, montuje istniejący wolumin i obsługuje logi; nie wykonuje formatowania.

Wpisy `log_put` trafiają do dziennych plików `logRRRRMMDD.txt` w katalogu głównym, np. `log20261116.txt`. Datę pliku oraz znacznik `RRRR-MM-DD GG:MM:SS` ustala się w chwili powstania wpisu według polskiego czasu CET/CEST. Wpisy oczekujące w kolejce zachowują swój dzień, również po północy lub korekcie NTP. Przed pierwszym ustawieniem RTC logi mają czas od uruchomienia i trafiają do `log_unsynced.txt`; nie są później przenoszone do pliku z datą.

Pliki są tworzone, jeśli ich brakuje; istniejące są otwierane do dopisywania i nie są obcinane. Przy zmianie dnia zadanie zamyka poprzedni plik i otwiera plik odpowiadający kolejnemu wpisowi. FatFs ma włączone długie nazwy (`_USE_LFN=2`). Buforowana kolejka 32 wpisów i osobne zadanie niskiego priorytetu oddzielają zapis SD od zadań CAN/MQTT. Zmiany są synchronizowane na kartę mniej więcej co sekundę i przy zamknięciu pliku. Przy przepełnieniu kolejki informacja o liczbie pominiętych wpisów trafia do pliku, gdy kolejka się opróżni. Stare logi nie są automatycznie usuwane; zapis zakończy się błędem po zapełnieniu karty. Wyjmowanie karty podczas zapisu nie jest bezpieczne — przed wyjęciem wyłącz zasilanie.

Test pliku pozostaje nieinwazyjny: jeśli `SDTEST.TXT` już istnieje, firmware odczytuje do 32 bajtów i go nie zmienia. Jeśli pliku nie ma, tworzy go wyłącznie w trybie `CREATE_NEW` i zapisuje krótki znacznik testowy. Błędy są zapisywane w logu; karta nie jest formatowana.

## RTC i synchronizacja NTP (JZ-F407)

RTC korzysta z LSE 32,768 kHz na PC14/PC15, formatu 24 h i zasilania VBAT z baterii. Kalendarz przechowuje UTC; przeliczenie na polski czas CET/CEST odbywa się przy odczycie, według obecnych reguł UE (ostatnia niedziela marca i października, 01:00 UTC). Po poprawnej synchronizacji znacznik w rejestrze backup pozwala zachować czas po restarcie i odłączeniu głównego zasilania, jeśli bateria i LSE działają. Przy braku poprawnego czasu używany jest jawnie czas działania od uruchomienia, a metadane FAT otrzymują datę zastępczą 1980-01-01.

Po inicjalizacji sieci klient SNTP odpytuje serwery Cloudflare `162.159.200.1` i `162.159.200.123`, ponawia nieudane próby i synchronizuje czas co godzinę. Wymaga dostępu do Internetu przez skonfigurowaną bramę oraz ruchu UDP na port 123; połączenie z lokalnym MQTT nie oznacza dostępu do NTP. Brak odpowiedzi nie blokuje CAN/MQTT ani podtrzymywanego RTC. Akceptowany zakres czasu NTP to lata 2024–2099. Odpowiedzi są sprawdzane pod kątem adresu i portu serwera, ale SNTP nie zapewnia uwierzytelnienia czasu.

Po każdej regeneracji CubeMX zachowaj sekcję `Check_RTC_BKUP` w `Core/Src/rtc.c`, konfigurację SNTP w sekcji użytkownika `LWIP/Target/lwipopts.h` oraz inicjalizację kolejki SD i mutexu RTC w sekcjach użytkownika `Core/Src/freertos.c`. RTC/NTP i zmiana dziennych plików wymagają jeszcze testu na płytce, w tym odłączenia głównego zasilania i ponownego startu bez sieci.

## Bieżąca baza kodu: NUCLEO F207 MQTT CAN

Firmware dla mikrokontrolera STM32F207ZGT6, który łączy sieć Ethernet i broker MQTT z magistralą CAN. Projekt zawiera również prosty serwer HTTP, wyświetlacz OLED oraz interfejs USB CDC do diagnostyki.

## Funkcje

- Odbieranie i wysyłanie ramek CAN.
- Publikowanie danych z CAN w MQTT oraz przekazywanie wybranych poleceń MQTT do CAN.
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
| Identyfikator klienta MQTT | `dscan-jz-f407` |

Przed uruchomieniem sprawdź, czy adres urządzenia nie jest zajęty i czy broker jest osiągalny z tej samej sieci. DHCP jest wyłączone. Adres IP urządzenia zmienia się w `LWIP/App/lwip.c`, a adres brokera w `Core/Src/my_mqtt.c`. Po zmianie konfiguracji przebuduj i wgraj firmware.

## Generowanie kodu F407 i budowanie

### Diody LED

- LED1 / PE13 zmienia stan co sekundę jako bicie serca głównego zadania.
  Miganie nie potwierdza stanu Ethernetu ani HTTP.
- LED2 / PE14 i LED3 / PE15 są sterowane wyłącznie przez formularz WWW.
  Pierwsze pole (`led=1`) steruje LED3, drugie (`led=2`) LED2. LED3 jest też
  przełączana przez test CAN po naciśnięciu przycisku S3.
  Zaznaczenie pola i wysłanie formularza zapala diodę, odznaczenie gasi.
- Zgodnie ze schematem płytki diody są aktywne stanem niskim:
  `GPIO_PIN_RESET` zapala, a `GPIO_PIN_SET` gasi.

Tymczasowa diagnostyka nasłuchu/łącza/SSI diodami została usunięta.
LCD pozostaje wyłączony. Zadanie przycisku obsługuje S3 (PE12) z eliminacją
drgań styków; naciśnięcie przełącza LED3 (PE15) i kolejkuje pojedynczą ramkę
testową CAN.

### CAN — odbiór, diagnostyka i most MQTT

CAN1 działa w trybie normalnym przy 50 kbit/s, na PD0 (RX) i PD1 (TX).
Potwierdza odbiór ramek przez ACK. Nadawanie obejmuje polecenia przychodzące
z MQTT oraz ramkę testową z S3.
Filtr przyjmuje wszystkie identyfikatory do FIFO0. Callback odbioru przekazuje
ramkę do kolejki; logowanie odbywa się w zadaniu CAN, nie w przerwaniu.
Do MQTT trafiają prawidłowe ramki danych zgodne z protokołem sterownika.

Po naciśnięciu S3 firmware wysyła standardową ramkę z ID `0x108`, `DLC=3`
i danymi `00 64 00`: zapytanie o identyfikator DS-MCU-CAN o ID `8`.
Wartość `0` nie zmienia ID ani nie uruchamia wyjść. OLED DS-MCU powinien pokazać
`C108 F100 00:00` poprzedzone czasem od uruchomienia. Log JZ pokazuje
`can_tx>0 0 108 0 3 00640000`; ostatni bajt jest tylko częścią logu, nie ramki.
Odziedziczony kod DS-MCU odpowiada funkcją `1` i wartością swojego ID (`8`),
a nie kodem `100`, więc odpowiedzi nie należy traktować jako stanu rolety.
Na magistrali 50 kbit/s pojedyncza ramka powinna być widoczna na oscyloskopie.
Jeśli nie ma innego aktywnego węzła, możliwy jest błąd ACK — sprawdź wtedy
przebieg różnicowy CANH–CANL oraz status błędów CAN.

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
Log WWW pokazuje ostatnie 50 wpisów; przy dużym ruchu logowanie każdej ramki
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
`MEM_SIZE=10*1024` zapewnia pamięć dla równoczesnego MQTT i WWW z 50 wpisami.
Przy poprzednim limicie 5 KB po uruchomieniu MQTT zaobserwowano nieudane
alokacje LwIP i zanikanie odpowiedzi HTTP.
`TCPIP_THREAD_STACKSIZE=4096` zapewnia stos dla równoczesnej obsługi HTTP,
MQTT i logowania. Po włączeniu MQTT potwierdzono nadpisanie początku
poprzedniego stosu o wielkości 1024 bajtów.
Po wgraniu tej konfiguracji na JZ-F407 potwierdzono w dwóch kolejnych seriach
pełne odpowiedzi HTTP 200 dla `/` i `/index.shtml` oraz HTTP 404 dla `/404.html`.
Ping 4/4 również przeszedł. Testowy serwer TCP echo został usunięty;
firmware nie uruchamia nasłuchu na porcie 2323. CAN i MQTT uruchamiane są
etapowo, zgodnie z opisami w tym dokumencie.

Konfiguracja projektu wskazuje F407, ale wygenerowany kod nie został jeszcze zweryfikowany. Nie wgrywaj dotychczasowych artefaktów z katalogu `Debug` na płytkę F407.

1. Otwórz `JZ-F407-MQTT-CAN.ioc` w STM32CubeMX i sprawdź, czy wybrany układ to STM32F407VET6.
2. Sprawdź mapę pinów i zegary: USART1 używa PA9/PA10, a źródłem PLL dla 168 MHz ma być HSE 25 MHz. I2C2/OLED pozostaje wyłączone; przy jego ponownym włączeniu użyj PF0 (SDA) i PF1 (SCL). Dla RMII potwierdź zgodność wygenerowanej obsługi PHY z DP83848 na płytce; nie zakładaj, że sterownik LAN8742 z bazy F207 jest zgodny.
3. Wygeneruj kod do katalogu projektu. Zachowaj istniejący kod aplikacji i przejrzyj cały diff, zwłaszcza pliki zawierające bloki `USER CODE`. Potwierdź, że powstał `Core/Startup/startup_stm32f407vetx.s`; zachowany plik startowy F207 nie może być użyty.
4. W **Project Manager → Toolchain / IDE** wybierz **CMake** oraz kompilator **GCC**, po czym wygeneruj kod. CubeMX utworzy `CMakeLists.txt`, `mx-generated.cmake` oraz `gcc-arm-none-eabi.cmake` w katalogu projektu.
5. Otwórz katalog projektu w VS Code, zainstaluj rekomendowane rozszerzenia, wybierz preset `debug` i wykonaj **CMake: Configure** oraz **CMake: Build**. Wymagane są STM32CubeCLT z CMake, Ninja i `arm-none-eabi-gcc` dostępnymi w `PATH`.
6. Przed wgrywaniem sprawdź wynik kompilacji oraz konfigurację pamięci Flash/linkera dla 512 KB. Test komunikacji Ethernet i CAN wykonaj na docelowej płytce.

STM32CubeMX pozostaje narzędziem do edycji `.ioc` i generowania kodu. STM32CubeIDE nie jest wymagane do konfiguracji ani budowania projektu.

## Skrypty testowe pyAI

Skrypty diagnostyczne tworzone przy rozwijaniu tego projektu zapisujemy w
[`pyAI`](./pyAI), aby można było je przeglądać i uruchamiać ponownie.
Wymagają Python 3.9 lub nowszego, bez dodatkowych pakietów. Poniższe polecenia
uruchamiaj z katalogu projektu; błędy kończą skrypt kodem różnym od zera.

- [`test_mqtt_www.py`](./pyAI/test_mqtt_www.py) sprawdza równolegle siedem
  publikacji `DataSoft/dscan/uptime` (odstępy i przyrost uptime 9–12 s)
  oraz odpowiedzi HTTP 200 z dokładnie 50 wyrenderowanymi wierszami logów
  dla `/` i `/index.shtml`. Test trwa około 65–80 sekund.
  Używa unikalnego identyfikatora klienta, tylko subskrybuje temat testowy:
  nie publikuje poleceń, nie steruje LED i nie korzysta z `DataSoft/can`.

  ```powershell
  python .\pyAI\test_mqtt_www.py
  python .\pyAI\test_mqtt_www.py --device 192.168.1.249 --broker 192.168.1.2 --port 1883
  ```

- [`inspect_mcu.py`](./pyAI/inspect_mcu.py) odczytuje przez ST-Link w trybie
  HOTPLUG zapas stosu TCP/IP, statystyki sterty LwIP oraz rejestry CAN1.
  Nie wgrywa firmware, nie resetuje ani celowo nie zatrzymuje MCU.
  Wymaga STM32CubeProgrammer i GDB ARM; ich domyślne ścieżki odpowiadają
  lokalnej instalacji Windows i można je zmienić przez `--programmer` i `--gdb`.
  Numer docelowego ST-Link podaj jawnie przez `--serial`.

  ```powershell
  python .\pyAI\inspect_mcu.py --serial "numer-ST-LINK"
  ```

  Adresy symboli, pola TCB i układ statystyk są ustalane z aktualnego ELF,
  domyślnie `build\debug\JZ-F407-MQTT-CAN.elf` (`--elf` wybiera inny plik).
  **ELF musi odpowiadać dokładnie wgranemu firmware.** Diagnostyka jest
  przeznaczona dla STM32F407 i obecnego alokatora `heap_3`; odczyt działającego
  MCU nie jest atomowym snapshotem. Zapas stosu jest mierzony wzorcem A5
  pozostawionym przez FreeRTOS; wymagane jest minimum 512 bajtów.
  Błędy alokacji LwIP, niepoprawne odczyty i zajęte mailboxy TX CAN powodują
  niepowodzenie kontroli. Puste mailboxy nie dowodzą braku wcześniejszego TX,
  a ten skrypt nie zastępuje testu odbioru CAN z rzeczywistym nadajnikiem.

- [`test_rtc_daily.py`](./pyAI/test_rtc_daily.py) kompiluje i uruchamia na komputerze
  rzeczywisty kod RTC/NTP, kolejki logów i zadania zapisującego pliki, zastępując
  sprzęt oraz FatFs atrapami. Sprawdza granice CET/CEST, rok przestępny, zmianę roku,
  zachowanie ustawionego kalendarza przy inicjalizacji, odrzucanie błędnego czasu NTP,
  błędy RTC, zakończenie długich wpisów znakiem nowej linii oraz dopisywanie do
  właściwych dziennych plików po zmianie daty i jej korekcie wstecz.
  Wymaga Windows i kompilatora MSVC C/C++ z Visual Studio lub Build Tools.
  Instalację wykrywa przez `vswhere`; opcja `--vcvars` pozwala wskazać ją jawnie.
  Nie łączy się z płytką ani Internetem i nie wgrywa firmware.
  Pliki kompilacji powstają w katalogu tymczasowym i są automatycznie usuwane.
  Ten test nie potwierdza fizycznego podtrzymania RTC, działania NTP ani zapisu na SD.

  ```powershell
  python .\pyAI\test_rtc_daily.py
  python .\pyAI\test_rtc_daily.py --vcvars "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
  ```

## MQTT

### Połączenie i publikacja

Zadanie MQTT jest włączone. Łączy się z `192.168.1.2:1883` jako
`dscan-jz-f407`, bez używania identyfikatora `stm32` starej instalacji.
Publikuje czas działania w sekundach pod `DataSoft/dscan/uptime`,
po połączeniu i następnie co 10 sekund, z QoS 0 i bez retained.
Po każdym połączeniu subskrybuje `DataSoft/dscan/+/set/+` z QoS 1.
Poleceń sterujących nie publikuj z flagą retained: broker mógłby wysłać je ponownie po reconnect.

Zadanie co około 100 ms zleca krótką obsługę przez `tcpip_callback`; tworzenie
klienta, połączenie i publikacja odbywają się wyłącznie w wątku TCP/IP.
Zadanie MQTT używa statycznego stosu i bloku kontrolnego, aby oszczędzać
pamięć alokowaną dynamicznie. Projekt używa `heap_3` (alokatora newlib);
`configTOTAL_HEAP_SIZE` nie jest w tym wariancie limitem alokatora.

Log WWW/USART1 pokazuje próbę połączenia, rzeczywiste potwierdzenie CONNACK,
zakolejkowanie publikacji, wynik wysyłania i błędy. Wysłanie z QoS 0 nie
stanowi potwierdzenia odbioru przez aplikację odbiorcy. Po rozłączeniu lub
błędzie próba połączenia jest ponawiana po 10 sekundach; keep-alive wynosi
30 sekund. Działające WWW i heartbeat LED1 pozostają niezależne od brokera.

### Przekazywanie CAN↔MQTT

Przestrzeń tematów została zmieniona z używanego przez starą instalację
`DataSoft/can` na `DataSoft/dscan`. Komendy trafiają do kolejki nadawczej CAN,
a raporty CAN są publikowane z kolejki MQTT w wątku TCP/IP.

### Dane z CAN

Przykładowe tematy publikowane dla standardowych ramek CAN:

| Temat | Znaczenie / wartość |
|---|---|
| `DataSoft/dscan/<ID>/roleta` | Stan rolety: `ON`, `OFF` albo `STOP` |
| `DataSoft/dscan/<ID>/roleta_pos` | Wartość pozycji rolety |
| `DataSoft/dscan/<ID>/switch1` | Stan przełącznika 1: `ON` albo `OFF` |
| `DataSoft/dscan/<ID>/switch2` | Stan przełącznika 2: `ON` albo `OFF` |
| `DataSoft/dscan/<ID>/sensor1` | Stan czujnika 1: `ON` albo `OFF` |
| `DataSoft/dscan/<ID>/sensor2` | Stan czujnika 2: `ON` albo `OFF` |
| `DataSoft/dscan/<ID>/prad` | Wartość pomiaru prądu |
| `DataSoft/dscan/<ID>/temp` | Wartość temperatury przekazana przez urządzenie CAN |

`<ID>` to identyfikator urządzenia w ramce CAN. Przykładowo `DataSoft/dscan/1/roleta`.

Aby wysłać polecenie do urządzenia CAN, użyj tematu z segmentem `set`, na przykład:

```text
Temat:   DataSoft/dscan/1/set/roleta
Payload: ON
```

Obsługiwane końcówki poleceń to `roleta`, `roleta_pos`, `switch1`, `switch2`, `sensor1` i `sensor2`. Akceptowane są `ON`, `OFF`, `STOP` oraz liczby dziesiętne `0`–`255`. `<ID>` musi być w zakresie `1`–`255`. Polecenie jest wysyłane standardową ramką CAN o identyfikatorze `0x100 + <ID>`. Przekazanie `STOP` nie oznacza, że dany sterownik je obsługuje: obecny DS-MCU-CAN w trybie rolety manualnej traktuje wartość `255` jako zamykanie.

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

Dla polecenia MQTT, np. `DataSoft/dscan/8/set/roleta`, bramka wysyła ramkę z `StdId=0x100+8`, `DLC=3` i bajtami `00 01 01` dla polecenia `ON`. Przykładowe dane polecenia roletowego: `to=0`, `fun=1`, `val=1`. Dla `OFF` wartość `val` wynosi `0`, a dla `STOP` `255`. Polecenia przełączników używają odpowiednio `fun=2` albo `fun=3` i wartości `0`/`1`.

Sterownik odpowiada ramką o tym samym identyfikatorze standardowym; jej `to` zawiera identyfikator sterownika. Dla wyjścia przełącznika odpowiedź zawiera kod funkcji `2` lub `3` i jego stan. Bramka wykorzystuje `to` do publikacji pod `DataSoft/dscan/<ID>/...`. Sterownik GW_MCU obsługuje tryb włącznika i rolety manualnej; pozostałe kody wymienione w tabeli nie oznaczają, że ta wersja urządzenia je realizuje.

W trybie rolety manualnej raport funkcji `1` jest wysyłany po zakończeniu ruchu i informuje o osiągniętym stanie: `val=1` oznacza roletę otwartą, a `val=0` zamkniętą. Choć bramka przyjmuje payload `STOP` i zamienia go na `val=255`, obecny firmware GW_MCU nie obsługuje zatrzymania z CAN — każda wartość inna niż `1`, w tym `255`, uruchamia zamykanie.

## Interfejs WWW i diagnostyka

Po podłączeniu do tej samej sieci otwórz `http://192.168.1.249/`. Strona pozwala sterować dwiema diodami LED i wyświetla wpisy logu. Log jest również dostępny przez USB CDC oraz wyjście debuggera.

## Struktura projektu

| Katalog / plik | Zawartość |
|---|---|
| `Core/Src`, `Core/Inc` | Aplikacja, sterowniki peryferiów i moduły MQTT, CAN, HTTP oraz logowania |
| `LWIP` | Konfiguracja sieci Ethernet i stosu LwIP |
| `USB_DEVICE` | Interfejs USB CDC |
| `Middlewares` | LwIP i FreeRTOS |
| `Drivers` | Obecnie pozostałości HAL STM32F2/CMSIS; pliki dla STM32F4 muszą zostać wygenerowane przed kompilacją |
| `mkfs` | Pliki strony WWW osadzone w firmware |
| `JZ-F407-MQTT-CAN.ioc` | Konfiguracja STM32CubeMX dla STM32F407VET6 |

## Licencje

Projekt zawiera komponenty STM32, FreeRTOS i LwIP. Warunki licencyjne komponentów zewnętrznych znajdują się w odpowiadających im katalogach `Drivers` i `Middlewares`.
