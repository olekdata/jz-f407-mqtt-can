# JZ-STM32F407 — schemat i mapa pinów

## Źródło i ważne zastrzeżenie

Mapa została opracowana na podstawie [schematu `Schematic_STM32F407VET6.pdf`](./DOC/Schematic_STM32F407VET6.pdf). Płytka jest zaprojektowana wokół STM32F407VET6.

Pliki `JZ-F407-MQTT-CAN.ioc` i `.cproject` wskazują STM32F407VETx, ale wygenerowane źródła, HAL/CMSIS i plik startup nadal pochodzą z bazy F207. Mapa opisuje połączenia sprzętowe z rysunku; przed budowaniem lub wgrywaniem wygeneruj kod dla STM32F407VET6 w STM32CubeMX i sprawdź go z tym schematem. Szczególnej weryfikacji wymaga sterownik PHY DP83848.

### Uzupełniające źródła: firmware i Zephyr

Jako dodatkową referencję do uruchamiania peryferiów wykorzystuj [art103/JZ-F407VET6](https://github.com/art103/JZ-F407VET6/tree/41c0851cd68953f00af3ac4ea2939ae89e4b8580), w szczególności jego [README](https://github.com/art103/JZ-F407VET6/blob/41c0851cd68953f00af3ac4ea2939ae89e4b8580/ReadMe.md), [konfigurację CubeMX](https://github.com/art103/JZ-F407VET6/blob/41c0851cd68953f00af3ac4ea2939ae89e4b8580/Industrial_Board.ioc) i [konfigurację/test CAN](https://github.com/art103/JZ-F407VET6/blob/41c0851cd68953f00af3ac4ea2939ae89e4b8580/Core/Src/can.c). Repozytorium opisuje firmware testujące peryferia oraz ustawienia konkretnej sztuki płytki; nie zastępuje lokalnego schematu ani weryfikacji naszej rewizji sprzętu.

- [Dokumentacja płytki JZ-F407VET6 w Zephyr](https://docs.zephyrproject.org/latest/boards/others/jz_f407vet6/doc/index.html) potwierdza rodzinę MCU i opisuje pinout, peryferia oraz sposób programowania. Zephyr oznacza płytkę jako „Not actively maintained”, więc traktuj ją jako niezależne źródło porównawcze, a nie gwarancję działania w aktualnej wersji systemu.
- [Device tree płytki w Zephyr](https://github.com/zephyrproject-rtos/zephyr/blob/6c1bfafa25b7a830d69e0369c889f0b35b386007/boards/others/jz_f407vet6/jz_f407vet6.dts) wskazuje m.in. HSE 25 MHz, LSE dla RTC, czterobitowe SDIO, konfigurację SPI2 oraz domyślne prędkości UART. To użyteczny przykład konfiguracji, ale nie należy kopiować ustawień Zephyr/CubeMX bez sprawdzenia ich wpływu na obecny firmware.
- Konfiguracja CubeMX identyfikuje STM32F407VET6, HSE 25 MHz, taktowanie rdzenia 168 MHz, RMII, CAN1/CAN2 po 500 kbit/s oraz USB OTG FS w trybie urządzenia i USB OTG HS w trybie hosta FS. To użyteczny punkt startowy do porównania, nie gotowa konfiguracja bramki ani potwierdzenie zgodności starego protokołu CAN.
- Repozytorium opisuje linię PE2 jako DS18B20. Wspiera to interpretację lokalnego sygnału PE2-DQ jako interfejsu czujnika 1-Wire; potwierdź obsadzony element i połączenie na swojej płytce.
- Są istotne rozbieżności sprzętowe: README repozytorium i dokumentacja Zephyr opisują W25Q64JV (64 Mbit / 8 MB), a lokalny schemat oznacza W25Q128 (128 Mbit / 16 MiB). Zephyr podaje dla W25Q64JV identyfikator JEDEC `EF 40 17` i zegar SPI do 40 MHz; przed użyciem sterownika Flash sprawdź oznaczenie i identyfikator układu, a parametry potwierdź w jego nocie katalogowej. README art103 zgłasza też brak połączenia masy transceivera CAN1 na testowanej przez autora płytce i opisuje mostek od spodu C22 do spodu C21. Przed modyfikacją CAN1 sprawdź ciągłość połączeń na konkretnej rewizji; nie stosuj tej poprawki w ciemno.
- Konfiguracje SDIO nie są jednakowe: `.ioc` art103 wybiera tryb 1-bitowy, natomiast device tree Zephyr włącza D0–D3, zgodnie z liniami wyprowadzonymi na lokalnym schemacie. Dobierz szerokość po sprawdzeniu płytki i sterownika.
- Konfiguracje USB także się różnią: `.ioc` art103 używa USB OTG HS jako hosta FS, ale device tree Zephyr ma węzeł hosta wyłączony domyślnie. Dokumentacja Zephyr wymienia host USB jako funkcję płytki; zweryfikuj złącze, VBUS i aktualną konfigurację, zamiast zakładać, że host jest uruchomiony.
- Ustawienia UART są zależne od przykładu: device tree Zephyr ustawia USART1 i USART2 na 115200, a `.ioc` art103 konfiguruje USART2 na 9600 z parzystością nieparzystą. Ustal prędkość i format na podstawie faktycznego protokołu aplikacji, w szczególności jeśli wykorzystasz Modbus RTU.
- `can.c` pokazuje konfigurację CAN i test pętli zwrotnej. Nie potwierdza to zgodności z naszymi ramkami ani transferu OTA; te zachowania wymagają osobnego projektu i testów.

## Mapa interfejsów MCU

| Interfejs | Piny MCU | Połączenie na płytce |
|---|---|---|
| Ethernet RMII | PA1 — RX_CLK; PA2 — MDIO; PA7 — CRS_DV; PB11 — TX_EN; PB12 — TXD0; PB13 — TXD1; PC1 — MDC; PC4 — RXD0; PC5 — RXD1 | PHY DP83848 i gniazdo/magnetyki HR911105A |
| CAN1 | PD0 — RX; PD1 — TX | Transceiver TJA1050, linie CAN1H/CAN1L |
| CAN2 | PB5 — RX; PB6 — TX | Drugi transceiver TJA1050, linie CAN2H/CAN2L |
| USART1 / RS232 | PA9 — TX; PA10 — RX | Konwerter poziomów MAX232ESA |
| USART2 / RS485 | PD5 — TX; PD6 — RX; PD7 — DE | Transceiver MAX3485; PD7 steruje kierunkiem nadawania |
| SPI2 | PB10 — SCK; PC2 — MISO; PC3 — MOSI | Wspólna magistrala dla W25Q128 i złącza NRF24L01 |
| I2C1 | PB8 — SCL; PB9 — SDA | EEPROM AT24C02BN. Etykieta SDA na schemacie zawiera literówkę `I2CI`; pin MCU odpowiada I2C1_SDA. |
| SDIO, tryb 4-bitowy | PC8 — D0; PC9 — D1; PC10 — D2; PC11 — D3; PC12 — CLK; PD2 — CMD; PD3 — card detect | Gniazdo karty microSD |
| USB OTG FS | PA11 — DM; PA12 — DP | Repozytorium referencyjne używa tej pary w trybie urządzenia |
| USB OTG HS, PHY FS | PB14 — DM; PB15 — DP | `.ioc` art103 konfiguruje hosta FS; Zephyr wyłącza węzeł hosta domyślnie. Zweryfikuj złącze i zasilanie VBUS |
| DCMI / złącza kamery | PA4 — HSYNC; PA6 — PIXCLK; PB7 — VSYNC; PC6 — D0; PC7 — D1; PE0 — D2; PE1 — D3; PE4 — D4; PE5 — D6; PE6 — D7; PD10 — PWDN; PD11 — RESET | Złącza P4/P5. Na schemacie nie znaleziono sygnału DCMI_D5; zweryfikuj, zanim skonfigurujesz pełne 8-bitowe DCMI. |
| DQ / 1-Wire | PE2 — DQ | Sygnał na złączu P7 z rezystorem podciągającym 4,7 kΩ; referencyjne `.ioc` oznacza PE2 jako DS18B20. Potwierdź element na swojej płytce. |
| RTC / zasilanie podtrzymujące | PC13 — TAMPER/RTC; PC14 — OSC32_IN; PC15 — OSC32_OUT; VBAT — CN5 | Zephyr włącza LSE i RTC; lokalny schemat pokazuje wejście VBAT przez złącze baterii CN5. |
| Przyciski | PE10 — SW1; PE11 — SW2; PE12 — SW3 | Trzy wejścia przycisków |
| Diody LED | PE13 — LED1; PE14 — LED2; PE15 — LED3 | Trzy wyjścia LED |
| Debug / boot | PA13 — SWDIO/JTMS; PA14 — SWCLK/JTCK; PA15 — JTDI; PB3 — JTDO; PB4 — NJTRST; NRST; BOOT0 | Złącze JTAG 2×10; PA13/PA14 są podstawowymi liniami SWD. Sprawdź stan strapu BOOT0 przy uruchamianiu i trybie bootloadera ROM. |

## Pamięć, radio, zegar i zasilanie

- **W25Q128** jest podłączona do SPI2: PE3 — FLASH-CS, PB10 — SCK, PC2 — MISO i PC3 — MOSI. Lokalny schemat podaje 128 Mbit (16 MiB), podczas gdy README art103 i device tree Zephyr deklarują W25Q64JV (8 MB); sprawdź oznaczenie układu przed doborem sterownika i adresów. Pamięć może być kandydatem na bufor obrazu OTA, ale sam schemat nie potwierdza takiego wykorzystania ani podziału pamięci.
- **NRF24L01** korzysta ze wspólnych linii SPI2; PE7 — CE, PE8 — CSN i PE9 — IRQ. Oddzielny CSN pozwala współdzielić magistralę z pamięcią Flash.
- **AT24C02BN** jest zewnętrzną EEPROM na I2C1; nie jest pamięcią obrazu firmware’u.
- MCU ma rezonator X1 25 MHz na OSC_IN/OSC_OUT. PHY Ethernet ma własny obwód zegarowy 25 MHz. Zephyr opisuje też rezonator RTC 32,768 kHz; schemat wyprowadza PC14/PC15 oraz VBAT przez CN5. Potwierdź obsadzenie rezonatora i baterii na konkretnej płytce.
- Schemat pokazuje regulator AMS1117-3.3 oraz szyny VIN, +5 V i +3.3 V. Dopuszczalne napięcie wejściowe i budżet prądowy potwierdź w pełnym schemacie/BOM i dokumentacji elementów przed podłączeniem zasilania.

## Wskazówki dla konfiguracji firmware’u

1. **Dwa niezależne CAN:** schemat wyprowadza CAN1 i CAN2 przez osobne transceivery. Nie przypisuje jednoznacznie, który kanał ma zachować dotychczasową magistralę, a który ma obsługiwać transfer OTA. Ustal tę rolę przed konfiguracją i nie zmieniaj parametrów ani ramek starego kanału bez testu zgodności.
2. **Konflikty funkcji alternatywnych:** wybieraj AF zgodne z połączeniami na płytce. Przykładowo PA11/PA12 mają również alternatywną funkcję CAN1, PB8/PB9 mogą pełnić CAN1, a PB8/PB9 są tu użyte przez EEPROM I2C1. CAN1 na schemacie jest podłączony do PD0/PD1.
3. **USB:** na schemacie występują dwie pary D-/D+: PA11/PA12 i PB14/PB15. Referencyjne `.ioc` konfiguruje pierwszą jako urządzenie OTG FS, a drugą jako host OTG HS z PHY FS. PA9/PA10 są jednocześnie użyte przez USART1/RS232, choć w OTG FS mogą pełnić VBUS/ID. Zweryfikuj złącza, wybrany kontroler USB oraz ustawienia VBUS/ID w CubeMX; nie zakładaj, że obie pary są równocześnie gotowymi portami.
4. **OTA:** W25Q128 daje sprzętową możliwość przechowania obrazu, ale samo połączenie pamięci nie implementuje OTA. Najpierw ustal dokładny model MCU aktualizowanego przez CAN i sprawdź w AN2606, czy jego bootloader ROM obsługuje CAN oraz jakie ma wymagania pinów i wejścia w tryb bootloadera; AN3154 opisuje protokół CAN tylko dla obsługiwanych wersji. Jeśli cel nie udostępnia właściwego bootloadera ROM, trzeba zaprojektować i zweryfikować własny. Niezależnie od wariantu określ format obrazu, adresowanie, sektory kasowania, kontrolę integralności oraz współpracę z bootloaderem. Nie traktuj EEPROM ani karty SD jako skonfigurowanego bufora OTA.
5. **Zegar i debug:** skonfiguruj HSE zgodnie z rezonatorem 25 MHz i zachowaj PA13/PA14 dla SWD. PA15/PB3/PB4 są użyte na złączu JTAG; ich użycie jako GPIO zależy od konfiguracji debugowania.

## Dokumentacja producentów przydatna do projektu

- [ST: dokumentacja STM32F407/417](https://www.st.com/en/microcontrollers-microprocessors/stm32f407-417/documentation.html) — aktualne wydania RM0090, kart katalogowych i erraty; korzystaj z erraty dla dokładnej rodziny i rewizji MCU przy konfiguracji zegarów, CAN, USB, Ethernetu i Flash.
- [ST AN2606](https://www.st.com/resource/en/application_note/an2606-.pdf) — lista bootloaderów systemowych i dostępnych interfejsów zależnie od konkretnego MCU.
- [ST AN3154](https://www.st.com/resource/en/application_note/cd00264321-can-protocol-used-in-the-stm32-bootloader-stmicroelectronics.pdf) — protokół CAN bootloadera STM32; nie zakładaj, że każdy MCU ani każda wersja bootloadera go obsługuje.
- [ST X-CUBE-SBSFU](https://www.st.com/en/embedded-software/x-cube-sbsfu.html) i [AN5056](https://www.st.com/resource/en/application_note/an5056-integration-guide-for-the-stm32-secure-boot-and-secure-firmware-update-solution-stmicroelectronics.pdf) — referencje do bezpiecznego startu, weryfikacji i instalacji obrazu. Przykładowy transport opisany w AN5056 używa UART/YMODEM; to nie jest gotowy transport OTA przez CAN.
- [Winbond W25Q-JV](https://www.winbond.com/hq/product/code-storage-flash/qspi-nor/w25q-jv/?__locale=en) — źródło producenta dla właściwości pamięci SPI po potwierdzeniu dokładnego oznaczenia układu.
- [NXP TJA1050](https://www.nxp.com/docs/en/data-sheet/TJA1050.pdf) — nota katalogowa transceivera CAN przydatna przy analizie zasilania i warstwy fizycznej.

Docelowa bramka OTA ma działać na F407, natomiast aktualizowany przez CAN MCU jest osobnym węzłem. Jego dokładny model, bootloader i przypisanie kanału CAN trzeba potwierdzić w projekcie. Mapa pinów nie zastępuje walidacji na rzeczywistej płytce.
