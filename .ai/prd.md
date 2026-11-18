# Dokument wymagań produktu (PRD) - Zegarek pogodowy dla dziecka

## 1. Przegląd produktu

### 1.1. Opis

Zegarek pogodowy dla dziecka to stacjonarne urządzenie z ekranem dotykowym, które pokazuje aktualną godzinę, bieżącą pogodę, prognozę oraz postacie z bajki dobrane do temperatury odczuwalnej. Postacie ilustrują zakres ubioru odpowiedni do warunków, dzięki czemu siedmioletnie dziecko może samodzielnie zdecydować, jak się ubrać, bez czytania szczegółowych danych meteorologicznych.

Wszystkie funkcje opisane w tym dokumencie należą do zakresu MVP.

### 1.2. Użytkownicy

| Rola | Opis | Główne potrzeby |
| --- | --- | --- |
| Dziecko | Siedmiolatek, główny odbiorca ekranu głównego | Szybki odczyt godziny, temperatury i postaci podpowiadającej ubiór; zmiana motywu postaci |
| Rodzic | Opiekun i administrator urządzenia | Konfiguracja na ekranie i przez panel WWW, diagnostyka, pobieranie logów |
| Osoba budująca firmware | Rodzic lub inna osoba z dostępem do kodu i komputera | Dostarczenie sekretów, kompilacja, wgranie firmware przez USB/UART |

### 1.3. Platforma

- Urządzenie: Waveshare ESP32-S3-Touch-LCD-7 (wersja dotykowa), moduł ESP32-S3N16R8 (16 MB flash, 8 MB PSRAM).
- Ekran: 7 cali, IPS, 800×480, RGB565, dotyk pojemnościowy GT911.
- Oprogramowanie: ESP-IDF 5.5.6, LVGL 9.5.
- Zasilanie: USB Type-C, 5 V.
- Pamięć masowa: karta TF (SPI) na szablony postaci i logi.
- Szczegóły sprzętowe: `.ai/technical-specification.md`. Układ ekranu głównego: `.ai/design.png`.

### 1.4. Stała lokalizacja i źródła danych

| Parametr | Wartość |
| --- | --- |
| Miejscowość | Racibórz, Polska |
| Szerokość geograficzna | 50.09 |
| Długość geograficzna | 18.22 |
| Strefa czasowa | Europe/Warsaw |
| Źródło pogody | Open-Meteo |
| Źródło czasu | NTP |

Lokalizacja jest zapisana w konfiguracji i nie może być zmieniana przez użytkownika.

### 1.5. Język interfejsu

Interfejs ekranu, panel WWW i komunikaty są wyłącznie w języku polskim. Komunikaty są krótkie i zrozumiałe dla dziecka oraz rodzica.

### 1.6. Kryteria sukcesu

Kryteria jakościowe uzgodnione dla MVP:

1. Czas jest poprawnie synchronizowany dla strefy Europe/Warsaw, łącznie ze zmianą czasu letniego i zimowego.
2. Dane Open-Meteo są poprawnie pobierane i prezentowane.
3. Temperatury odczuwalne są prawidłowo przypisywane do pięciu grafik.
4. Dane aktualne i prognozowane są jednoznacznie rozróżnialne dzięki układowi ekranu i etykietom.
5. Godzina, temperatury i ikony są czytelne dla siedmioletniego dziecka.
6. Urządzenie automatycznie wraca do pełnej pracy po odzyskaniu Wi-Fi.
7. Konfiguracja i wybrany szablon są zachowywane po restarcie i zaniku zasilania.
8. Rotacja logów utrzymuje maksymalnie 30 plików.
9. Urządzenie działa bezpiecznie bez karty TF lub z uszkodzoną kartą.
10. Urządzenie pracuje stabilnie w trybie ciągłym.

Progi liczbowe przyjęte dla MVP:

| Miara | Proponowana wartość |
| --- | --- |
| Czas od uzyskania Wi-Fi do synchronizacji czasu | do 60 s |
| Czas od synchronizacji czasu do wyświetlenia pogody | do 60 s przy działającym API |
| Odległość czytelnego odczytu godziny | co najmniej 3 m |
| Odległość czytelnego odczytu temperatur i rozpoznania postaci | co najmniej 2 m |
| Ciągła praca bez restartu i bez ręcznej interwencji | co najmniej 7 dni |
| Opóźnienie reakcji na gest dotykowy | do 300 ms |
| Czas zmiany szablonu po geście | do 2 s |

## 2. Problem użytkownika

### 2.1. Problem dziecka

Siedmioletnie dziecko nie potrafi jeszcze samodzielnie przełożyć liczby stopni Celsjusza na decyzję o ubiorze. Nie rozumie też różnicy między temperaturą rzeczywistą a odczuwalną ani tego, że pogoda w ciągu dnia może się zmienić. Typowe aplikacje i stacje pogodowe prezentują zbyt wiele danych, małe czcionki i abstrakcyjne symbole, które nie odpowiadają na pytanie dziecka: „Jak mam się ubrać?”.

### 2.2. Problem rodzica

Rodzic chce, aby dziecko uczyło się samodzielności, ale nie musiało za każdym razem pytać o pogodę. Potrzebuje urządzenia, które:

- działa bez stałej obsługi i samo synchronizuje czas oraz pogodę;
- nie pokazuje dziecku nieaktualnych lub mylących danych;
- daje się skonfigurować z ekranu lub z komputera w sieci domowej;
- udostępnia diagnostykę, gdy coś nie działa;
- nie ujawnia haseł i nie wymaga dostępu z internetu.

### 2.3. Odpowiedź produktu

Urządzenie łączy czytelny zegar z prostą wizualizacją pogody:

- lewa postać pokazuje ubiór odpowiedni teraz, prawa ubiór odpowiedni na później;
- kolor temperatury odczuwalnej sygnalizuje, czy jest chłodniej, czy cieplej, niż wskazuje termometr;
- etykieta „dzisiaj” albo „jutro 8:00” wyjaśnia, czego dotyczy prognoza;
- prosta ikona pokazuje aktualne warunki, np. deszcz lub śnieg;
- dziecko może samo zmieniać motyw postaci, co zwiększa zaangażowanie;
- szczegóły techniczne są ukryte przed dzieckiem i dostępne dla rodzica.

## 3. Wymagania funkcjonalne

Wartości wcześniej traktowane jako propozycje zostały rozstrzygnięte. Obowiązują zapisy poniżej oraz sekcja 4.5.

### 3.1. Czas i synchronizacja

- FR-001: Urządzenie synchronizuje czas przez NTP natychmiast po uzyskaniu połączenia Wi-Fi.
- FR-002: Czas jest wyświetlany w strefie Europe/Warsaw z automatyczną obsługą czasu letniego i zimowego.
- FR-003: Do chwili pierwszej udanej synchronizacji urządzenie wyświetla `00:00`, ikonę łączenia oraz ukrywa temperatury, ikonę pogody, etykietę prognozy i postacie. Znany problem karty TF albo Wi-Fi jest w tym czasie dodatkowo opisany kartą „Uwaga” z FR-037.
- FR-004: Po synchronizacji zegar działa lokalnie i nadal wyświetla czas po utracie sieci.
- FR-005: Urządzenie ponawia synchronizację NTP co 24 godziny oraz zawsze po odzyskaniu sieci.
- FR-006: Godzina jest wyświetlana w formacie 24-godzinnym `HH:MM` z zerem wiodącym.

### 3.2. Pobieranie danych pogodowych

- FR-010: Urządzenie pobiera z Open-Meteo dla stałej lokalizacji:
  - aktualną temperaturę;
  - aktualną temperaturę odczuwalną;
  - aktualny kod pogody WMO;
  - aktualną informację o opadach;
  - godzinową prognozę temperatury i temperatury odczuwalnej obejmującą co najmniej dzień bieżący i następny;
  - prawdopodobieństwo i wartość prognozowanych opadów.
- FR-011: Pierwsze pobranie następuje natychmiast po synchronizacji czasu po uruchomieniu urządzenia.
- FR-012: Kolejne pobrania odbywają się zgodnie z konfigurowalnym interwałem odświeżania, domyślnie co 2 godziny.
- FR-013: Odpowiedź jest walidowana. Brak wymaganych pól lub niepoprawny format traktowane są jako błąd pobrania.
- FR-014: Po błędzie pobrania urządzenie ponawia żądanie trzy razy, po 1 min, 5 min i 15 min. Po wyczerpaniu prób kolejna próba następuje przy następnym planowym odświeżeniu.
- FR-015: Podczas ponawiania urządzenie zachowuje i wyświetla ostatnie poprawne dane, jeżeli nie wygasły.
- FR-016: Każde wywołanie API, jego wynik i kod błędu są zapisywane w logu.

### 3.3. Ważność danych i obsługa utraty sieci

- FR-020: Dane pogodowe są ważne przez czterokrotność skonfigurowanego interwału odświeżania, liczoną od ostatniej poprawnej aktualizacji. Dla interwału 2 godzin limit wynosi 8 godzin.
- FR-021: Po wygaśnięciu danych urządzenie ukrywa temperatury, ikonę pogody, etykietę prognozy i obie postacie. Godzina pozostaje widoczna.
- FR-022: Po utracie Wi-Fi urządzenie wyświetla ikonę łączenia, kartę „Uwaga” z krótkim komunikatem i cyklicznie próbuje odzyskać połączenie.
- FR-023: Po odzyskaniu Wi-Fi urządzenie natychmiast synchronizuje czas i pobiera pogodę, bez oczekiwania na kolejny interwał. Komunikat Wi-Fi znika z karty „Uwaga”.
- FR-025: Niepowodzenie uruchomienia Wi-Fi, w tym brak pamięci na sterownik, nie resetuje urządzenia. Godzina pozostaje widoczna, a karta „Uwaga” pokazuje „Wi-Fi nie działa”. Zdarzenie jest zapisywane w logu. Ponawianie połączenia dotyczy tylko radia, które zostało uruchomione.
- FR-024: Zmiana interwału odświeżania natychmiast zmienia limit ważności danych.

### 3.4. Ekran główny

- FR-030: Ekran główny ma ciemne tło i układ zgodny z `.ai/design.png`.
- FR-031: Na środku ekranu wyświetlane są:
  - aktualna godzina;
  - aktualna temperatura i temperatura odczuwalna rozdzielone ukośnikiem;
  - ikona aktualnej pogody pod aktualną temperaturą;
  - prognozowana temperatura i temperatura odczuwalna rozdzielone ukośnikiem;
  - etykieta prognozy „dzisiaj” albo „jutro 8:00”.
- FR-032: Po lewej stronie wyświetlana jest postać dobrana do aktualnej temperatury odczuwalnej, a po prawej postać dobrana do prognozowanej temperatury odczuwalnej.
- FR-033: Temperatury są zaokrąglane do pełnych stopni Celsjusza według reguły „połowa w górę od zera” (np. 2,5 → 3; −2,5 → −3). Wartość „−0” jest wyświetlana jako „0”.
- FR-034: Temperatura odczuwalna jest kolorowana na podstawie porównania wartości po zaokrągleniu:
  - niebieski, gdy odczuwalna jest niższa od rzeczywistej;
  - czerwony, gdy odczuwalna jest wyższa od rzeczywistej;
  - biały, gdy obie wartości są równe.
- FR-035: Reguła kolorowania dotyczy zarówno danych aktualnych, jak i prognozowanych.
- FR-036: Stan łączenia i pogody jest prezentowany na ekranie głównym prostymi ikonami statusu:
  - ikona łączenia: brak Wi-Fi lub brak pierwszej synchronizacji czasu;
  - ikona braku danych pogodowych: połączenie działa, ale dane pogodowe są niedostępne lub wygasły.
- FR-037: Problemy karty TF i Wi-Fi są dodatkowo opisane na środku ekranu, pod godziną, kartą „Uwaga”. Karta ma bursztynową obwódkę i krótki komunikat po polsku. Znika, gdy nie ma już żadnego z tych problemów. Ekran główny nie pokazuje kodów błędów, haseł ani innych danych technicznych. Komunikaty karty:
  - „Brak karty pamięci”, gdy karta nie odpowiada przy starcie;
  - „Karta pamięci nie działa”, gdy magistrala lub odczyt karty kończy się innym błędem;
  - „Karta pamięci jest pełna”;
  - „Wi-Fi nie działa”, gdy sterownik nie może się uruchomić;
  - komunikat połączenia: „Nie można połączyć z zapisaną siecią”, „Nie znaleziono sieci Wi-Fi” albo „Brak połączenia z siecią”.
- FR-038: Dotknięcia ekranu głównego poza zdefiniowanymi gestami są ignorowane.

### 3.5. Prognoza

- FR-040: Godzina prognozy jest konfigurowalna w pełnych godzinach od 08:00 do 20:00. Wartość domyślna to 15:00.
- FR-041: Gdy bieżący czas lokalny jest wcześniejszy niż godzina prognozy, wyświetlana jest prognoza na bieżący dzień na godzinę prognozy z etykietą „dzisiaj”.
- FR-042: Gdy bieżący czas lokalny osiągnie godzinę prognozy lub ją przekroczy, wyświetlana jest prognoza na następny dzień na 08:00 z etykietą „jutro 8:00”.
- FR-043: Przełączenie prognozy następuje dokładnie w chwili osiągnięcia godziny prognozy oraz o północy, na podstawie zapisanych danych godzinowych i bez oczekiwania na kolejne odświeżenie.
- FR-044: Jeżeli zapisane dane nie obejmują wymaganej godziny prognozy, urządzenie natychmiast pobiera nowe dane. Do czasu ich otrzymania prognoza, jej etykieta i prawa postać są ukryte.
- FR-045: Zmiana godziny prognozy w konfiguracji natychmiast przelicza wyświetlaną prognozę.

### 3.6. Przedziały temperatur i postacie

- FR-050: Postać jest wybierana na podstawie zaokrąglonej temperatury odczuwalnej T:

| Przedział | Plik |
| --- | --- |
| T < −5°C | `1.png` |
| −5°C ≤ T < 5°C | `2.png` |
| 5°C ≤ T < 15°C | `3.png` |
| 15°C ≤ T ≤ 25°C | `4.png` |
| T > 25°C | `5.png` |

- FR-051: Ta sama reguła dotyczy postaci aktualnej i prognozowanej.

### 3.7. Ikony pogody i mapowanie kodów WMO

- FR-060: Proste ikony pogody są przechowywane we flash urządzenia i nie zależą od karty TF.
- FR-061: Ikona dotyczy wyłącznie aktualnej pogody i jest wyświetlana pod aktualną temperaturą.
- FR-062: Kody WMO są mapowane na ikony zgodnie z tabelą:

| Kody WMO | Znaczenie | Ikona |
| --- | --- | --- |
| 0 | Bezchmurnie | Słońce |
| 1, 2 | Przeważnie pogodnie, częściowe zachmurzenie | Częściowe zachmurzenie |
| 3 | Pochmurno | Chmury |
| 45, 48 | Mgła, osadzająca się szadź | Mgła |
| 51, 53, 55 | Mżawka | Deszcz |
| 61, 63, 65 | Deszcz | Deszcz |
| 80, 81, 82 | Przelotny deszcz | Deszcz |
| 56, 57 | Marznąca mżawka | Marznący deszcz |
| 66, 67 | Marznący deszcz | Marznący deszcz |
| 71, 73, 75, 77 | Śnieg, ziarna śniegu | Śnieg |
| 85, 86 | Przelotny śnieg | Śnieg |
| 95, 96, 99 | Burza, burza z gradem | Burza |
| Każdy inny lub brak kodu | Stan nieznany | Nieznany stan pogody |

### 3.8. Szablony postaci

- FR-070: Szablony są przechowywane na karcie TF w katalogach `/images/[nazwa-szablonu]`.
- FR-071: Szablon jest kompletny, gdy zawiera pliki `1.png`–`5.png` (wielkość liter w nazwie nie ma znaczenia). Każdy plik to PNG z przezroczystym tłem. Docelowy wymiar to 266×480 px, czyli kolumna postaci z `.ai/design.png`. Dłuższy bok może mieć najwyżej 960 px, a plik najwyżej 400 KB. Obraz jest wpisywany w pole 266×480 ze zachowaniem proporcji i wyśrodkowaniem, bez rozciągania. Plik ponad limit jest pomijany jak uszkodzony.
- FR-072: Lista szablonów jest budowana przy uruchomieniu urządzenia. Katalogi są sortowane alfabetycznie, a niekompletne szablony są pomijane i zapisywane w logu.
- FR-073: Trzy dotknięcia prawego dolnego rogu ekranu głównego wybierają następny kompletny szablon. Po ostatnim szablonie wybierany jest pierwszy.
- FR-074: Wybrany szablon jest natychmiast zapisywany we flash i przywracany po restarcie.
- FR-075: Jeżeli zapisany szablon nie jest już dostępny lub jest niekompletny, wybierany jest pierwszy kompletny szablon według kolejności alfabetycznej.
- FR-076: Szablon awaryjny z pięcioma grafikami 266×480 jest przechowywany we flash. Jest używany, gdy karta TF jest niedostępna albo nie zawiera żadnego kompletnego szablonu. Nie bierze udziału w rotacji, gdy na karcie jest co najmniej jeden kompletny szablon.
- FR-077: Jeżeli dostępny jest tylko jeden szablon, gest zmiany szablonu nie zmienia wyglądu ekranu.
- FR-078: Jeżeli pojedynczy plik PNG nie daje się wczytać, urządzenie wyświetla odpowiadającą grafikę z szablonu awaryjnego i zapisuje błąd w logu.

### 3.9. Gesty dotykowe

- FR-080: Gest otwarcia konfiguracji: trzy dotknięcia lewego górnego rogu ekranu głównego.
- FR-081: Gest zmiany szablonu: trzy dotknięcia prawego dolnego rogu ekranu głównego.
- FR-082: Obszar rogu ma 120×120 px. Trzy dotknięcia muszą nastąpić w ciągu 1,5 s.
- FR-083: Dotknięcie poza obszarem rogu lub przekroczenie czasu zeruje licznik gestu.
- FR-084: Gesty nie mają dodatkowego zabezpieczenia.

### 3.10. Ekran konfiguracji na urządzeniu

- FR-090: Ekran konfiguracji zawiera ustawienia edytowalne:
  - wybór szablonu z listy kompletnych szablonów;
  - interwał odświeżania pogody: 30 min, 1 h, 2 h, 3 h albo 6 h; domyślnie 2 h;
  - jasność dzienną;
  - zmniejszoną jasność;
  - początek i koniec przedziału zmniejszonej jasności (domyślnie 20:00–06:00);
  - godzinę prognozy;
  - rozmiar czcionki godziny, temperatur i etykiet prognozy, każdy wybierany niezależnie spośród przetestowanych wariantów.
- FR-091: Ekran konfiguracji zawiera informacje tylko do odczytu:
  - aktualny czas i status synchronizacji;
  - status Wi-Fi, SSID zapisany podczas kompilacji i adres IP urządzenia;
  - adres panelu WWW `http://[IP]/configuration`;
  - lokalizację;
  - bieżące i prognozowane dane pogodowe, w tym kod WMO, opady, prawdopodobieństwo i wartość prognozowanych opadów;
  - status Open-Meteo i czas ostatniej poprawnej aktualizacji;
  - status karty TF;
  - szczegółowe komunikaty błędów.
- FR-092: Zmiany są stosowane dopiero po naciśnięciu „Zapisz”. Przycisk „Anuluj” odrzuca zmiany i wraca do ekranu głównego.
- FR-093: Zmiany są walidowane przed zapisem. Niepoprawna wartość blokuje zapis i jest oznaczona krótkim komunikatem przy polu.
- FR-094: Po zapisaniu zmiany są stosowane natychmiast, bez restartu.
- FR-095: Ostatni poprawny zapis nadpisuje poprzednią konfigurację. Konfiguracja nie jest wersjonowana.
- FR-096: Brak aktywności na ekranie konfiguracji przez 2 minuty powoduje powrót do ekranu głównego bez zapisywania zmian.

### 3.11. Schemat i trwałość konfiguracji

- FR-100: Konfiguracja jest przechowywana we flash i opisana jednym schematem zawierającym dla każdego pola: wartość domyślną, typ danych, dozwolony zakres oraz numer wersji schematu.
- FR-101: Ekran urządzenia i panel WWW korzystają z tej samej konfiguracji i tego samego schematu walidacji.
- FR-102: Zapis konfiguracji jest atomowy. Zanik zasilania podczas zapisu nie może pozostawić konfiguracji w stanie częściowo zapisanym.
- FR-103: Konfiguracja i wybrany szablon są zachowywane po restarcie, zaniku zasilania i aktualizacji firmware.
- FR-104: Po wykryciu uszkodzonej lub nieczytelnej konfiguracji urządzenie używa wartości domyślnych, zapisuje błąd w logu i pokazuje go na ekranie konfiguracji.
- FR-105: Po zmianie wersji schematu urządzenie migruje konfigurację: zachowuje poprawne wartości znanych pól, dla nowych pól stosuje wartości domyślne, a wartości spoza zakresu zastępuje domyślnymi.

### 3.12. Sekrety i kompilacja

- FR-110: SSID i hasło Wi-Fi oraz hasło panelu WWW są dostarczane podczas kompilacji z zewnętrznego pliku sekretów.
- FR-111: Plik sekretów nie jest przechowywany w repozytorium i znajduje się w `.gitignore`.
- FR-112: Repozytorium zawiera wersjonowany plik przykładowy bez prawdziwych wartości.
- FR-113: Brak pliku sekretów lub hasła panelu WWW przerywa kompilację czytelnym komunikatem.
- FR-114: SSID i hasło Wi-Fi nie mogą być zmienione z ekranu urządzenia ani z panelu WWW. Zmiana sieci wymaga ponownej kompilacji i flashowania.
- FR-115: Hasła Wi-Fi i panelu WWW nie są zapisywane w logach, wyświetlane na ekranie ani zwracane przez panel WWW.

### 3.13. Panel WWW

- FR-120: Panel jest dostępny wyłącznie w sieci lokalnej przez HTTP pod adresem `http://[adres-ip-urządzenia]/configuration`.
- FR-121: Każda strona i każde żądanie panelu, w tym zapis konfiguracji, lista logów i pobieranie plików, wymagają uwierzytelnienia stałym hasłem ustalonym podczas kompilacji.
- FR-122: Żądanie bez uwierzytelnienia lub z błędnym hasłem jest odrzucane bez ujawniania danych konfiguracji i diagnostyki.
- FR-123: Nieudane próby logowania są zapisywane w logu bez podanego hasła. Kolejne nieudane próby z tego samego urządzenia są spowalniane: 1 s, 2 s, 4 s, 8 s, potem 16 s.
- FR-124: Panel udostępnia te same ustawienia edytowalne i informacje diagnostyczne co ekran konfiguracji, z przyciskami „Zapisz” i „Anuluj”.
- FR-125: Panel waliduje dane po stronie urządzenia według wspólnego schematu. Niepoprawne dane nie są zapisywane, a panel pokazuje komunikat błędu.
- FR-126: Panel wyświetla listę wszystkich istniejących plików logów (maksymalnie 30) i umożliwia pobranie każdego pliku osobno.
- FR-127: Pobieranie jest ograniczone do plików w katalogu logów; żądania innych ścieżek są odrzucane.
- FR-128: Jeżeli logowanie do plików jest wyłączone z powodu awarii karty TF, panel informuje o tym zamiast listy logów.
- FR-129: Brak szyfrowania HTTP jest świadomie zaakceptowanym ograniczeniem MVP (patrz 4.3).

### 3.14. Karta TF i tryb awaryjny

- FR-130: Karta TF przechowuje szablony postaci i logi.
- FR-131: Jeżeli karta jest nieobecna, uszkodzona lub pełna, urządzenie:
  - uruchamia ekran główny z szablonem awaryjnym z flash;
  - wyłącza logowanie do plików;
  - pokazuje błąd karty na ekranie głównym w karcie „Uwaga”, na ekranie konfiguracji i w panelu WWW;
  - nie restartuje się cyklicznie z powodu braku karty ani z powodu braku pamięci przy starcie Wi-Fi.
- FR-132: Wyjmowanie i wymiana karty podczas pracy nie są obsługiwane. Karta jest sprawdzana raz, przy starcie. Zmiany na karcie są uwzględniane po restarcie.
- FR-133: Awaria karty nie wpływa na zegar, pobieranie pogody, ikony pogody ani konfigurację.

### 3.15. Logowanie

- FR-140: Urządzenie tworzy jeden tekstowy plik logu dziennie o nazwie `RRRR-MM-DD.log`.
- FR-141: Urządzenie przechowuje maksymalnie 30 plików. Po utworzeniu nowego pliku ponad limit najstarszy plik jest usuwany.
- FR-142: Logi zawierają znaczniki czasu oraz wpisy dotyczące: uruchomień i restartów (z przyczyną restartu), statusu i zdarzeń Wi-Fi, synchronizacji czasu, wywołań Open-Meteo, kodów i szczegółów błędów, zmian konfiguracji, zmian szablonu, logowania do panelu i stanu karty TF.
- FR-143: Przed synchronizacją NTP wpisy używają czasu od uruchomienia i są oznaczone jako niezsynchronizowane. Trafiają do najnowszego istniejącego pliku logu. Gdy żadnego pliku nie ma, powstaje `niesynchronizowane.log`.
- FR-144: Logi nie zawierają haseł ani innych sekretów.
- FR-145: Logi używają poziomów: błąd, ostrzeżenie i informacja. Domyślnie zapisywany jest poziom informacja oraz poziomy wyższe.

### 3.16. Jasność

- FR-150: Urządzenie obsługuje jasność dzienną i zmniejszoną jasność w przedziale czasu.
- FR-151: Przedział zmniejszonej jasności może przechodzić przez północ, domyślnie 20:00–06:00.
- FR-152: Zmiana ustawień jasności lub synchronizacja czasu natychmiast stosuje odpowiedni tryb.
- FR-153: Przed pierwszą synchronizacją czasu używana jest jasność dzienna.
- FR-154: Podświetlenie jest włączane cyfrowym wyjściem EXIO2 ekspandera CH422G i nie ma regulacji sprzętowej. Przyciemnianie jest programowe: półprzezroczysta czarna warstwa na całym interfejsie, która przepuszcza dotyk. Jasność dzienna i zmniejszona są wybierane w krokach co 10%, od 10% do 100%. Domyślnie 100% i 30%. Początek i koniec przedziału nie mogą być takie same.
- FR-155: Jasność zmniejszona nie może całkowicie wygasić ekranu; godzina musi pozostać czytelna.

### 3.17. Typografia

- FR-160: Rozmiary czcionek godziny, temperatur i etykiet prognozy są wybierane niezależnie: mały, średni albo duży. Domyślnie średni. W pikselach: godzina 59 / 75 / 86, temperatury 37 / 45 / 45, etykiety 20 / 28 / 36. Godzina i temperatury są krojem Bold. Duży rozmiar temperatury jest taki sam jak średni, bo szerszy napis nie mieści się w ramce z odstępem 3 px.
- FR-161: Każda kombinacja wariantów mieści się na ekranie 800×480 bez nakładania się elementów.
- FR-162: Czcionki obsługują polskie znaki diakrytyczne oraz znaki `°` i `−`.

### 3.18. Aktualizacja firmware

- FR-170: Firmware jest aktualizowany przez port USB-UART Type-C.
- FR-171: Aktualizacja firmware nie usuwa konfiguracji zapisanej we flash, chyba że osoba aktualizująca świadomie wyczyści pamięć.
- FR-172: Aktualizacje OTA są poza zakresem MVP.

### 3.19. Wymagania niefunkcjonalne

- NFR-001: Urządzenie pracuje w trybie ciągłym bez konieczności ręcznych restartów.
- NFR-002: Błędy sieci, API i karty TF nie mogą powodować zawieszenia interfejsu ani restartu urządzenia.
- NFR-003: Zegar jest odświeżany co najmniej raz na sekundę; zmiana minuty jest widoczna bez opóźnienia większego niż 1 s.
- NFR-004: Interfejs pozostaje responsywny podczas pobierania danych, zapisu konfiguracji i operacji na karcie TF.
- NFR-005: Panel WWW nie jest dostępny spoza sieci lokalnej; urządzenie nie otwiera żadnych usług w internecie.
- NFR-006: Kod i repozytorium nie zawierają żadnych sekretów.

## 4. Granice produktu

### 4.1. W zakresie MVP

- Zegar synchronizowany przez NTP dla strefy Europe/Warsaw.
- Pobieranie aktualnej pogody i prognozy z Open-Meteo dla stałej lokalizacji Racibórz.
- Ekran główny z godziną, temperaturami, ikoną pogody, prognozą z etykietą i dwiema postaciami.
- Pięć przedziałów temperatur i automatyczny dobór grafik.
- Dynamiczne szablony PNG na karcie TF oraz szablon awaryjny we flash.
- Zmiana szablonu gestem i jej trwały zapis.
- Ekran konfiguracji na urządzeniu.
- Panel WWW w sieci lokalnej z uwierzytelnieniem hasłem.
- Harmonogram przyciemniania przechodzący przez północ.
- Obsługa utraty sieci, błędów API i awarii karty TF.
- Dzienne logi z rotacją 30 plików i ich pobieranie przez panel.
- Trwała konfiguracja z jednym schematem i numerem wersji.
- Aktualizacja firmware przez USB/UART.

### 4.2. Poza zakresem MVP

- Aktualizacje OTA.
- Zmiana lokalizacji przez użytkownika i obsługa wielu lokalizacji.
- Zmiana sieci Wi-Fi z urządzenia lub panelu WWW, tryb punktu dostępowego i portal konfiguracyjny.
- HTTPS, konta użytkowników, role i zmiana hasła panelu bez ponownej kompilacji.
- Dostęp do panelu spoza sieci lokalnej i integracje z chmurą.
- Wymiana karty TF podczas pracy urządzenia.
- Przesyłanie szablonów przez panel WWW.
- Wersjonowanie i historia zmian konfiguracji.
- Inne języki niż polski.
- Ostrzeżenia pogodowe, alarmy, budzik, dźwięki, ikony nocne.
- Prezentacja opadów na ekranie głównym w postaci liczbowej.
- Zasilanie bateryjne jako wspierany tryb pracy.
- Dowolny, płynny rozmiar czcionek.

### 4.3. Świadome ograniczenia

- Panel WWW działa przez nieszyfrowane HTTP. Hasło i dane mogą zostać przechwycone przez inne urządzenie w tej samej sieci lokalnej. Ryzyko jest akceptowane dla MVP pod warunkiem pracy wyłącznie w zaufanej sieci domowej.
- Wejście do konfiguracji na urządzeniu nie jest zabezpieczone; dziecko może je otworzyć i zmienić ustawienia.
- Zmiana sieci Wi-Fi lub hasła panelu wymaga ponownej kompilacji i flashowania firmware. Wcześniejsze założenie o poprawianiu danych Wi-Fi z poziomu konfiguracji zostało zastąpione tą decyzją.
- Ostatni zapis konfiguracji wygrywa; równoczesna edycja na ekranie i w panelu może nadpisać zmiany drugiej osoby.
- Zmiany na karcie TF są widoczne dopiero po restarcie.

### 4.4. Założenia i zależności

- Urządzenie pracuje w zasięgu domowej sieci Wi-Fi 2,4 GHz.
- Dostępność Open-Meteo i serwerów NTP jest zależnością zewnętrzną.
- Płyta nie ma podtrzymywanego zegara czasu rzeczywistego, więc po każdym uruchomieniu czas wymaga synchronizacji NTP.
- Grafiki szablonów są przygotowywane przez rodzica i kopiowane na kartę TF poza urządzeniem. Docelowy plik postaci ma 266×480 px.
- Urządzenie jest zasilane stale przez USB Type-C.

### 4.5. Rozstrzygnięcia

Kwestie poniżej są zamknięte i obowiązują w MVP.

| Nr | Kwestia | Rozstrzygnięcie |
| --- | --- | --- |
| OQ-01 | Czy ponowna kompilacja i flashowanie po zmianie Wi-Fi to akceptowalne ograniczenie końcowe | Akceptowalne w MVP |
| OQ-02 | Jak rodzic pozna adres IP panelu | Adres IP i pełny adres panelu na ekranie konfiguracji |
| OQ-03 | Mechanizm uwierzytelniania przez HTTP i akceptacja ryzyka braku szyfrowania | Formularz logowania i sesja w ciasteczku. Hasło nie trafia do adresu URL. Brak HTTPS jest akceptowany w zaufanej sieci lokalnej |
| OQ-04 | Dostępne interwały odświeżania pogody | 30 min, 1 h, 2 h, 3 h, 6 h; domyślnie 2 h |
| OQ-05 | Zakres i wartość domyślna godziny prognozy | Pełne godziny 08:00–20:00; domyślnie 15:00 |
| OQ-06 | Poziomy jasności dziennej i nocnej | Skala 10–100% co 10%; domyślnie 100% i 30% |
| OQ-07 | Czy płyta umożliwia sprzętową regulację podświetlenia | Nie. EXIO2 jest wyjściem cyfrowym. Przyciemnianie jest programowe |
| OQ-08 | Warianty rozmiarów czcionek | Mały, średni i duży. Godzina 59/75/86 px Bold, temperatury 37/45/45 px Bold, etykiety 20/28/36 px |
| OQ-09 | Migracja konfiguracji po zmianie wersji schematu | Zachowanie poprawnych pól, wartości domyślne dla nowych i niepoprawnych |
| OQ-10 | Zachowanie po wykryciu uszkodzonej konfiguracji | Wartości domyślne, wpis w logu, komunikat w konfiguracji |
| OQ-11 | Poziomy szczegółowości logowania | Błąd, ostrzeżenie, informacja; domyślnie informacja |
| OQ-12 | Mapowanie kodów WMO na ikony | Tabela w FR-062 |
| OQ-13 | Wymiary, limity rozmiaru i format plików PNG | PNG z przezroczystym tłem. Docelowo 266×480 px. Dłuższy bok najwyżej 960 px, plik najwyżej 400 KB. Wyświetlanie: contain w kolumnie 266×480 |
| OQ-14 | Odstępy między trzema ponownymi próbami pobrania | 1 min, 5 min, 15 min |
| OQ-15 | Mierzalne kryteria akceptacji | Wartości z tabeli w 1.6 |
| OQ-16 | Obszar i czas gestu trzech dotknięć | 120×120 px, 1,5 s |
| OQ-17 | Czas bezczynności zamykający ekran konfiguracji | 2 minuty |
| OQ-18 | Udział szablonu awaryjnego w rotacji | Tylko gdy brak kompletnych szablonów na karcie |
| OQ-19 | Miejsce zapisu logów sprzed synchronizacji NTP | Najnowszy istniejący plik logu. Gdy go nie ma: `niesynchronizowane.log` |

## 5. Historyjki użytkowników

### 5.1. Ekran główny

#### US-001

- ID: US-001
- Tytuł: Odczyt aktualnej godziny
- Opis: Jako dziecko chcę zobaczyć dużą, czytelną godzinę, aby wiedzieć, która jest godzina.
- Kryteria akceptacji:
  1. Po synchronizacji czasu godzina jest wyświetlana na środku ekranu w formacie `HH:MM` z zerem wiodącym, np. `07:05`.
  2. Godzina jest zgodna z czasem Europe/Warsaw z dokładnością do 1 s względem serwera NTP w chwili synchronizacji.
  3. Zmiana minuty jest widoczna na ekranie w ciągu 1 s.
  4. Godzina pozostaje widoczna niezależnie od stanu Wi-Fi, API i karty TF.

#### US-002

- ID: US-002
- Tytuł: Odczyt aktualnej temperatury i temperatury odczuwalnej
- Opis: Jako dziecko chcę zobaczyć, jaka jest temperatura teraz i jak ją odczuwam, aby wiedzieć, czy jest zimno, czy ciepło.
- Kryteria akceptacji:
  1. Przy ważnych danych ekran pokazuje aktualną temperaturę i temperaturę odczuwalną rozdzielone ukośnikiem, np. `3° / −1°`.
  2. Obie wartości są zaokrąglone do pełnych stopni zgodnie z FR-033.
  3. Wartość 0,4 i −0,4 jest wyświetlana jako `0`, a nie `−0`.
  4. Temperatury ujemne są poprzedzone znakiem minus.

#### US-003

- ID: US-003
- Tytuł: Kolor temperatury odczuwalnej
- Opis: Jako dziecko chcę, aby kolor temperatury odczuwalnej podpowiadał mi, czy jest chłodniej, czy cieplej, niż pokazuje termometr.
- Kryteria akceptacji:
  1. Gdy zaokrąglona temperatura odczuwalna jest niższa od zaokrąglonej rzeczywistej, jest wyświetlana na niebiesko.
  2. Gdy jest wyższa, jest wyświetlana na czerwono.
  3. Gdy obie zaokrąglone wartości są równe, jest wyświetlana na biało.
  4. Ta sama reguła dotyczy danych aktualnych i prognozowanych.

#### US-004

- ID: US-004
- Tytuł: Ikona aktualnej pogody
- Opis: Jako dziecko chcę zobaczyć prosty obrazek aktualnej pogody, aby wiedzieć, czy pada deszcz, śnieg albo świeci słońce.
- Kryteria akceptacji:
  1. Ikona jest wyświetlana pod aktualną temperaturą.
  2. Ikona jest dobierana na podstawie aktualnego kodu WMO zgodnie z tabelą w FR-062.
  3. Kod spoza tabeli lub brak kodu powoduje wyświetlenie ikony nieznanego stanu pogody.
  4. Ikony są wczytywane z flash i wyświetlają się również bez karty TF.
  5. Ikona nie jest wyświetlana przy prognozie.

#### US-005

- ID: US-005
- Tytuł: Postać podpowiadająca ubiór teraz
- Opis: Jako dziecko chcę zobaczyć po lewej stronie postać ubraną odpowiednio do aktualnej temperatury odczuwalnej, aby wiedzieć, jak się ubrać teraz.
- Kryteria akceptacji:
  1. Lewa postać odpowiada zaokrąglonej aktualnej temperaturze odczuwalnej zgodnie z tabelą w FR-050.
  2. Postać pochodzi z aktywnego szablonu.
  3. Zmiana przedziału po odświeżeniu danych zmienia postać bez restartu.

#### US-006

- ID: US-006
- Tytuł: Prognoza na dzisiaj
- Opis: Jako dziecko chcę wiedzieć, jaka będzie pogoda później dzisiaj, aby przygotować się na wyjście po południu.
- Kryteria akceptacji:
  1. Przed godziną prognozy ekran pokazuje prognozowaną temperaturę i temperaturę odczuwalną dla dzisiejszej godziny prognozy.
  2. Prognoza ma etykietę „dzisiaj”.
  3. Prognoza jest wyraźnie oddzielona od danych aktualnych układem ekranu.

#### US-007

- ID: US-007
- Tytuł: Prognoza na jutro rano
- Opis: Jako dziecko chcę po południu i wieczorem widzieć pogodę na jutrzejszy poranek, aby wiedzieć, jak ubrać się rano.
- Kryteria akceptacji:
  1. Od chwili osiągnięcia godziny prognozy ekran pokazuje prognozę na następny dzień na 08:00.
  2. Prognoza ma etykietę „jutro 8:00”.
  3. Przełączenie następuje w ciągu 1 minuty od osiągnięcia godziny prognozy, bez oczekiwania na kolejne odświeżenie danych.

#### US-008

- ID: US-008
- Tytuł: Postać podpowiadająca ubiór na później
- Opis: Jako dziecko chcę zobaczyć po prawej stronie postać ubraną odpowiednio do prognozowanej temperatury odczuwalnej, aby przygotować ubranie na później.
- Kryteria akceptacji:
  1. Prawa postać odpowiada zaokrąglonej prognozowanej temperaturze odczuwalnej zgodnie z FR-050.
  2. Prawa postać zmienia się razem z przełączeniem prognozy z „dzisiaj” na „jutro 8:00” i odwrotnie.
  3. Prawa postać pochodzi z tego samego szablonu co lewa.

#### US-009

- ID: US-009
- Tytuł: Zmiana dnia o północy
- Opis: Jako dziecko chcę, aby po północy prognoza znów dotyczyła bieżącego dnia, aby etykieta zawsze była prawdziwa.
- Kryteria akceptacji:
  1. O 00:00 etykieta zmienia się z „jutro 8:00” na „dzisiaj”, a wyświetlana jest prognoza na nowy dzień na godzinę prognozy.
  2. Przełączenie korzysta z zapisanych danych godzinowych, jeżeli je obejmują.
  3. Jeżeli zapisane dane nie obejmują wymaganej godziny, urządzenie natychmiast pobiera dane, a do ich otrzymania ukrywa prognozę, etykietę i prawą postać.

#### US-010

- ID: US-010
- Tytuł: Poprawne przypisanie postaci na granicach przedziałów
- Opis: Jako rodzic chcę, aby postacie były przypisane jednoznacznie również na granicach przedziałów, aby dziecko nie dostawało sprzecznych podpowiedzi.
- Kryteria akceptacji:
  1. Zaokrąglone wartości są przypisywane następująco: −6 → `1.png`, −5 → `2.png`, 4 → `2.png`, 5 → `3.png`, 14 → `3.png`, 15 → `4.png`, 25 → `4.png`, 26 → `5.png`.
  2. Klasyfikacja używa wartości po zaokrągleniu, tej samej, którą widzi dziecko, np. −5,4 → −5 → `2.png`, −5,5 → −6 → `1.png`, 25,4 → 25 → `4.png`, 25,5 → 26 → `5.png`.
  3. Reguła jest identyczna dla postaci aktualnej i prognozowanej.

#### US-011

- ID: US-011
- Tytuł: Proste ikony statusu
- Opis: Jako dziecko chcę widzieć prosty znak, gdy urządzenie nie ma połączenia lub danych, zamiast niezrozumiałych komunikatów.
- Kryteria akceptacji:
  1. Przy braku Wi-Fi lub przed pierwszą synchronizacją czasu wyświetlana jest ikona łączenia.
  2. Przy działającym Wi-Fi i niedostępnych lub wygasłych danych pogodowych wyświetlana jest ikona braku danych pogodowych.
  3. Ekran główny nie wyświetla kodów błędów. Problemy karty TF i Wi-Fi są opisane kartą „Uwaga” z FR-037.
  4. Ikony statusu i karta „Uwaga” znikają po ustąpieniu swojej przyczyny.

#### US-012

- ID: US-012
- Tytuł: Ignorowanie przypadkowych dotknięć
- Opis: Jako rodzic chcę, aby przypadkowe dotknięcia ekranu przez dziecko nie zmieniały ustawień, aby urządzenie było odporne na zabawę.
- Kryteria akceptacji:
  1. Dotknięcia poza rogami gestów nie wywołują żadnej akcji.
  2. Jedno lub dwa dotknięcia rogu nie wywołują żadnej akcji.
  3. Trzy dotknięcia wykonane w czasie dłuższym niż limit gestu nie wywołują akcji.
  4. Dotknięcie poza obszarem rogu w trakcie gestu zeruje licznik.

### 5.2. Szablony postaci

#### US-013

- ID: US-013
- Tytuł: Zmiana szablonu gestem
- Opis: Jako dziecko chcę trzykrotnie dotknąć prawego dolnego rogu, aby zmienić postacie na inne z ulubionej bajki.
- Kryteria akceptacji:
  1. Trzy dotknięcia prawego dolnego rogu w limicie czasu wybierają następny kompletny szablon w kolejności alfabetycznej.
  2. Obie postacie zmieniają się w ciągu 2 s.
  3. Wybór jest natychmiast zapisywany we flash.
  4. Zmiana jest zapisywana w logu.

#### US-014

- ID: US-014
- Tytuł: Zawijanie listy szablonów i pomijanie niekompletnych
- Opis: Jako dziecko chcę, aby kolejne zmiany szablonu zawsze pokazywały poprawne postacie i wracały do początku listy.
- Kryteria akceptacji:
  1. Po ostatnim kompletnym szablonie wybierany jest pierwszy.
  2. Katalog bez któregokolwiek z plików `1.png`–`5.png` nie pojawia się w rotacji ani na liście w konfiguracji.
  3. Pominięcie niekompletnego szablonu jest zapisane w logu z nazwą katalogu i brakującymi plikami.

#### US-015

- ID: US-015
- Tytuł: Zmiana szablonu przy jednym dostępnym szablonie
- Opis: Jako dziecko chcę, aby gest zmiany szablonu nie psuł ekranu, gdy dostępny jest tylko jeden zestaw postaci.
- Kryteria akceptacji:
  1. Gdy istnieje jeden kompletny szablon lub używany jest szablon awaryjny, gest nie zmienia wyglądu ekranu.
  2. Gest nie powoduje błędu, migotania ani restartu.

#### US-016

- ID: US-016
- Tytuł: Przywrócenie szablonu po restarcie
- Opis: Jako dziecko chcę, aby po wyłączeniu i włączeniu zegara wyświetlały się postacie, które wybrałem.
- Kryteria akceptacji:
  1. Po restarcie aktywny jest szablon zapisany we flash.
  2. Jeżeli zapisany szablon nie istnieje lub jest niekompletny, wybierany jest pierwszy kompletny szablon alfabetycznie, a zdarzenie jest zapisywane w logu.
  3. Jeżeli na karcie nie ma kompletnych szablonów, używany jest szablon awaryjny.

#### US-017

- ID: US-017
- Tytuł: Wybór szablonu z ekranu konfiguracji
- Opis: Jako rodzic chcę wybrać szablon z listy w konfiguracji, aby ustawić konkretny motyw bez przełączania po kolei.
- Kryteria akceptacji:
  1. Lista zawiera wyłącznie kompletne szablony w kolejności alfabetycznej.
  2. Wybrany szablon staje się aktywny po naciśnięciu „Zapisz”.
  3. „Anuluj” pozostawia poprzedni szablon.

#### US-018

- ID: US-018
- Tytuł: Uszkodzony plik grafiki w kompletnym szablonie
- Opis: Jako dziecko chcę zawsze widzieć postać, nawet gdy jeden z obrazków jest uszkodzony.
- Kryteria akceptacji:
  1. Jeżeli plik PNG istnieje, ale nie daje się wczytać, wyświetlana jest grafika z tego samego przedziału z szablonu awaryjnego.
  2. Błąd jest zapisywany w logu z nazwą szablonu i pliku oraz pokazywany w konfiguracji.
  3. Pozostałe grafiki szablonu działają normalnie.

### 5.3. Uruchomienie, czas i pogoda

#### US-019

- ID: US-019
- Tytuł: Uruchomienie przed synchronizacją
- Opis: Jako dziecko chcę, aby zaraz po włączeniu zegar nie pokazywał błędnych danych, tylko informował, że się łączy.
- Kryteria akceptacji:
  1. Do pierwszej synchronizacji NTP ekran pokazuje `00:00` i ikonę łączenia.
  2. Temperatury, ikona pogody, etykieta prognozy i postacie są ukryte.
  3. Uruchomienie jest zapisywane w logu z przyczyną restartu.

#### US-020

- ID: US-020
- Tytuł: Automatyczna synchronizacja czasu
- Opis: Jako rodzic chcę, aby zegar sam ustawiał właściwy czas, również po zmianie czasu letniego i zimowego.
- Kryteria akceptacji:
  1. Po uzyskaniu Wi-Fi czas jest synchronizowany przez NTP w czasie określonym w 1.6.
  2. Po synchronizacji wyświetlany jest czas Europe/Warsaw.
  3. Zmiana czasu letniego i zimowego odbywa się automatycznie o właściwej godzinie.
  4. Synchronizacja jest okresowo ponawiana i zapisywana w logu.

#### US-021

- ID: US-021
- Tytuł: Natychmiastowe pobranie pogody po uruchomieniu
- Opis: Jako dziecko chcę zobaczyć pogodę zaraz po włączeniu zegara, bez czekania kilku godzin.
- Kryteria akceptacji:
  1. Pogoda jest pobierana natychmiast po pierwszej synchronizacji czasu.
  2. Po poprawnym pobraniu ekran pokazuje temperatury, ikonę, prognozę z etykietą i obie postacie.
  3. Wywołanie i jego wynik są zapisywane w logu.

#### US-022

- ID: US-022
- Tytuł: Okresowe odświeżanie pogody
- Opis: Jako rodzic chcę, aby pogoda była odświeżana regularnie, aby dziecko widziało aktualne dane.
- Kryteria akceptacji:
  1. Pogoda jest pobierana co skonfigurowany interwał, domyślnie co 2 godziny.
  2. Zmiana interwału obowiązuje od następnego planowania bez restartu.
  3. Czas ostatniej poprawnej aktualizacji jest widoczny w konfiguracji.

#### US-023

- ID: US-023
- Tytuł: Ponawianie po błędzie Open-Meteo
- Opis: Jako rodzic chcę, aby chwilowy błąd serwisu pogodowego nie zostawiał dziecka bez danych na wiele godzin.
- Kryteria akceptacji:
  1. Po błędzie pobrania urządzenie wykonuje trzy ponowne próby z rosnącymi odstępami.
  2. W tym czasie wyświetlane są ostatnie poprawne dane, jeżeli nie wygasły.
  3. Po trzeciej nieudanej próbie urządzenie czeka do następnego planowego odświeżenia.
  4. Każda próba i jej kod błędu są zapisywane w logu, a status Open-Meteo jest widoczny w konfiguracji.

#### US-024

- ID: US-024
- Tytuł: Niepoprawna odpowiedź serwisu pogodowego
- Opis: Jako rodzic chcę, aby niekompletne lub błędne dane z API nie zostały pokazane dziecku.
- Kryteria akceptacji:
  1. Odpowiedź bez wymaganych pól lub w niepoprawnym formacie jest traktowana jak błąd pobrania.
  2. Ostatnie poprawne dane nie są nadpisywane.
  3. Uruchamiane jest ponawianie zgodnie z US-023.

#### US-025

- ID: US-025
- Tytuł: Praca zegara po utracie sieci
- Opis: Jako dziecko chcę, aby zegar dalej pokazywał godzinę, gdy w domu nie działa internet.
- Kryteria akceptacji:
  1. Po utracie Wi-Fi godzina jest nadal wyświetlana i odmierzana.
  2. Wyświetlana jest ikona łączenia oraz karta „Uwaga” z komunikatem połączenia.
  3. Ostatnie poprawne dane pogodowe pozostają widoczne do upływu limitu ważności.
  4. Utrata połączenia jest zapisywana w logu.

#### US-026

- ID: US-026
- Tytuł: Wygaśnięcie nieaktualnych danych pogodowych
- Opis: Jako rodzic chcę, aby dziecko nie widziało zbyt starej pogody, która mogłaby wprowadzić je w błąd.
- Kryteria akceptacji:
  1. Po upływie czterokrotności interwału od ostatniej poprawnej aktualizacji temperatury, ikona pogody, etykieta prognozy i postacie są ukrywane.
  2. Dla interwału 2 godzin dane znikają po 8 godzinach.
  3. Godzina pozostaje widoczna, a na ekranie wyświetlana jest odpowiednia ikona statusu.
  4. Wygaśnięcie danych jest zapisywane w logu.

#### US-027

- ID: US-027
- Tytuł: Automatyczny powrót po odzyskaniu sieci
- Opis: Jako rodzic chcę, aby po naprawie internetu zegar sam wrócił do pełnej pracy bez mojej interwencji.
- Kryteria akceptacji:
  1. Po odzyskaniu Wi-Fi urządzenie natychmiast synchronizuje czas i pobiera pogodę.
  2. Po poprawnym pobraniu dane i postacie są ponownie wyświetlane, a ikona łączenia i komunikat Wi-Fi znikają. Komunikat karty TF pozostaje, dopóki karta jest niedostępna.
  3. Harmonogram odświeżania jest liczony od tego pobrania.
  4. Odzyskanie połączenia jest zapisywane w logu.

#### US-028

- ID: US-028
- Tytuł: Brak możliwości połączenia z zapisaną siecią
- Opis: Jako rodzic chcę wiedzieć, że urządzenie nie może połączyć się z siecią zapisaną w firmware, aby podjąć decyzję o ponownym flashowaniu.
- Kryteria akceptacji:
  1. Przy błędnych danych Wi-Fi lub niedostępnej sieci ekran główny pokazuje `00:00` lub ostatni znany czas, ikonę łączenia oraz kartę „Uwaga” z komunikatem połączenia.
  2. Urządzenie cyklicznie ponawia próbę połączenia, jeżeli radio zostało uruchomione.
  3. Gdy sterownik Wi-Fi nie może się uruchomić, urządzenie nie resetuje się, a karta „Uwaga” pokazuje „Wi-Fi nie działa”.
  4. Ekran konfiguracji pokazuje status Wi-Fi, SSID i komunikat błędu.
  5. Błąd jest zapisywany w logu bez hasła.

### 5.4. Ekran konfiguracji na urządzeniu

#### US-029

- ID: US-029
- Tytuł: Otwarcie ekranu konfiguracji
- Opis: Jako rodzic chcę otworzyć konfigurację bezpośrednio na urządzeniu, aby zmienić ustawienia bez komputera.
- Kryteria akceptacji:
  1. Trzy dotknięcia lewego górnego rogu w limicie czasu otwierają ekran konfiguracji.
  2. Ekran pokazuje aktualnie zapisane wartości.
  3. Wejście nie wymaga hasła.

#### US-030

- ID: US-030
- Tytuł: Zapisanie zmian konfiguracji
- Opis: Jako rodzic chcę zatwierdzić zmiany przyciskiem „Zapisz”, aby zostały zastosowane i zachowane.
- Kryteria akceptacji:
  1. Po naciśnięciu „Zapisz” poprawne zmiany są zapisywane atomowo we flash.
  2. Zmiany są stosowane natychmiast, bez restartu.
  3. Urządzenie wraca do ekranu głównego.
  4. Zmiana jest zapisywana w logu bez sekretów.

#### US-031

- ID: US-031
- Tytuł: Anulowanie zmian konfiguracji
- Opis: Jako rodzic chcę wycofać niezatwierdzone zmiany, aby nie zmieniać ustawień przez pomyłkę.
- Kryteria akceptacji:
  1. Po naciśnięciu „Anuluj” wszystkie niezapisane zmiany są odrzucane.
  2. Urządzenie wraca do ekranu głównego z poprzednimi ustawieniami.
  3. Konfiguracja we flash nie jest modyfikowana.

#### US-032

- ID: US-032
- Tytuł: Walidacja wartości konfiguracji
- Opis: Jako rodzic chcę, aby urządzenie nie pozwoliło zapisać błędnych ustawień, aby nie zepsuć działania zegara.
- Kryteria akceptacji:
  1. Wartości spoza dozwolonego typu lub zakresu blokują zapis.
  2. Przy błędnym polu pojawia się krótki komunikat po polsku.
  3. Przy błędzie walidacji żadna zmiana nie jest zapisywana.
  4. Dane tylko do odczytu, w tym lokalizacja i SSID, nie mogą być edytowane.

#### US-033

- ID: US-033
- Tytuł: Ustawienie godziny prognozy
- Opis: Jako rodzic chcę ustawić godzinę prognozy, aby odpowiadała porze, gdy dziecko wychodzi z domu po południu.
- Kryteria akceptacji:
  1. Godzinę można wybrać tylko z dozwolonego zakresu pełnych godzin.
  2. Po zapisaniu prognoza i etykieta są natychmiast przeliczane.
  3. Jeżeli bieżący czas jest już po nowej godzinie prognozy, wyświetlana jest prognoza „jutro 8:00”.

#### US-034

- ID: US-034
- Tytuł: Ustawienie interwału odświeżania
- Opis: Jako rodzic chcę ustawić częstotliwość pobierania pogody, aby dopasować aktualność danych do potrzeb.
- Kryteria akceptacji:
  1. Interwał można wybrać tylko z dostępnych wartości.
  2. Po zapisaniu zmienia się harmonogram odświeżania i limit ważności danych równy czterokrotności interwału.
  3. Jeżeli po zmianie dane przekraczają nowy limit ważności, zostają natychmiast ukryte.

#### US-035

- ID: US-035
- Tytuł: Ustawienie jasności i przedziału nocnego
- Opis: Jako rodzic chcę ustawić jasność dzienną i nocną oraz godziny przyciemniania, aby ekran nie przeszkadzał dziecku w zasypianiu.
- Kryteria akceptacji:
  1. Można ustawić jasność dzienną, jasność zmniejszoną oraz początek i koniec przedziału.
  2. Przedział może przechodzić przez północ, np. 20:00–06:00.
  3. Po zapisaniu odpowiedni tryb jasności jest stosowany natychmiast.
  4. Przy zmniejszonej jasności godzina pozostaje czytelna.
  5. Walidacja odrzuca przedział o identycznym początku i końcu.

#### US-036

- ID: US-036
- Tytuł: Ustawienie rozmiarów czcionek
- Opis: Jako rodzic chcę powiększyć lub zmniejszyć godzinę, temperatury i etykiety, aby były czytelne dla dziecka z miejsca, z którego patrzy.
- Kryteria akceptacji:
  1. Dla godziny, temperatur i etykiet prognozy można niezależnie wybrać jeden z przetestowanych wariantów.
  2. Po zapisaniu ekran główny używa nowych rozmiarów.
  3. Żadna kombinacja wariantów nie powoduje nakładania się elementów ani wyjścia poza ekran.

#### US-037

- ID: US-037
- Tytuł: Podgląd statusu i diagnostyki na urządzeniu
- Opis: Jako rodzic chcę zobaczyć na ekranie konfiguracji stan urządzenia, aby zrozumieć, dlaczego dane nie są wyświetlane.
- Kryteria akceptacji:
  1. Ekran pokazuje aktualny czas i status synchronizacji, status Wi-Fi, SSID, adres IP, status Open-Meteo z czasem ostatniej aktualizacji, status karty TF oraz szczegółowe komunikaty błędów.
  2. Ekran pokazuje bieżące i prognozowane dane pogodowe, w tym kod WMO, opady oraz prawdopodobieństwo i wartość prognozowanych opadów.
  3. Hasło Wi-Fi nie jest wyświetlane.

#### US-038

- ID: US-038
- Tytuł: Odczyt adresu panelu WWW
- Opis: Jako rodzic chcę poznać adres panelu WWW na ekranie urządzenia, aby otworzyć go na komputerze lub telefonie.
- Kryteria akceptacji:
  1. Przy połączeniu z Wi-Fi ekran konfiguracji pokazuje adres IP i pełny adres `http://[IP]/configuration`.
  2. Przy braku połączenia ekran pokazuje informację, że panel jest niedostępny.

#### US-039

- ID: US-039
- Tytuł: Automatyczne zamknięcie nieużywanej konfiguracji
- Opis: Jako rodzic chcę, aby pozostawiony otwarty ekran konfiguracji sam się zamknął, aby dziecko znów widziało zegar.
- Kryteria akceptacji:
  1. Po określonym czasie bezczynności urządzenie wraca do ekranu głównego.
  2. Niezapisane zmiany są odrzucane.
  3. Każde dotknięcie ekranu konfiguracji zeruje licznik bezczynności.

### 5.5. Panel WWW i uwierzytelnianie

#### US-040

- ID: US-040
- Tytuł: Uwierzytelnienie w panelu WWW
- Opis: Jako rodzic chcę zalogować się do panelu hasłem, aby tylko osoby znające hasło mogły zmieniać ustawienia przez sieć.
- Kryteria akceptacji:
  1. Wejście na `http://[IP]/configuration` wymaga podania hasła ustalonego podczas kompilacji.
  2. Po podaniu poprawnego hasła panel pokazuje ustawienia i diagnostykę.
  3. Hasło nie jest widoczne w adresie URL, kodzie strony ani logach.

#### US-041

- ID: US-041
- Tytuł: Odmowa dostępu bez poprawnego hasła
- Opis: Jako rodzic chcę, aby osoba bez hasła nie mogła odczytać ani zmienić konfiguracji, aby urządzenie było chronione w sieci domowej.
- Kryteria akceptacji:
  1. Żądanie bez uwierzytelnienia lub z błędnym hasłem do dowolnej strony, operacji zapisu, listy logów lub pliku logu jest odrzucane.
  2. Odpowiedź odmowy nie zawiera danych konfiguracji, diagnostyki ani logów.
  3. Nieudana próba jest zapisywana w logu bez wpisanego hasła.
  4. Kolejne nieudane próby są spowalniane: 1 s, 2 s, 4 s, 8 s, potem 16 s.

#### US-042

- ID: US-042
- Tytuł: Zmiana konfiguracji przez panel WWW
- Opis: Jako rodzic chcę zmienić ustawienia z komputera lub telefonu, aby nie musieć obsługiwać małego ekranu.
- Kryteria akceptacji:
  1. Panel udostępnia te same ustawienia edytowalne co ekran urządzenia.
  2. „Zapisz” waliduje dane na urządzeniu według wspólnego schematu i zapisuje je atomowo.
  3. Niepoprawne dane nie są zapisywane, a panel pokazuje komunikat błędu przy polu.
  4. „Anuluj” przywraca w formularzu zapisane wartości.
  5. Zapisane zmiany są natychmiast stosowane na urządzeniu.

#### US-043

- ID: US-043
- Tytuł: Równoczesna edycja na ekranie i w panelu
- Opis: Jako rodzic chcę wiedzieć, która zmiana obowiązuje, gdy ustawienia edytowano jednocześnie na urządzeniu i w panelu.
- Kryteria akceptacji:
  1. Obowiązuje ostatni poprawnie zapisany zestaw ustawień.
  2. Zapis nie pozostawia konfiguracji w stanie mieszanym z dwóch źródeł.
  3. Po zapisie w panelu ekran konfiguracji otwarty później pokazuje nowe wartości.

#### US-044

- ID: US-044
- Tytuł: Diagnostyka w panelu WWW
- Opis: Jako rodzic chcę zobaczyć w panelu stan urządzenia, aby zdiagnozować problem bez podchodzenia do zegara.
- Kryteria akceptacji:
  1. Panel pokazuje te same informacje tylko do odczytu co ekran konfiguracji, opisane w US-037.
  2. Panel nie pokazuje haseł Wi-Fi ani panelu.

#### US-045

- ID: US-045
- Tytuł: Przeglądanie i pobieranie logów
- Opis: Jako rodzic chcę pobrać wybrany plik logu, aby przeanalizować problem lub przekazać go osobie pomagającej.
- Kryteria akceptacji:
  1. Panel pokazuje listę wszystkich istniejących plików logów, maksymalnie 30, posortowaną od najnowszego.
  2. Każdy plik można pobrać osobno jako plik tekstowy.
  3. Żądanie pliku spoza katalogu logów lub nieistniejącego pliku jest odrzucane.
  4. Przy wyłączonym logowaniu z powodu awarii karty panel pokazuje komunikat zamiast listy.

### 5.6. Sekrety, kompilacja i aktualizacje

#### US-046

- ID: US-046
- Tytuł: Dostarczenie sekretów podczas kompilacji
- Opis: Jako osoba budująca firmware chcę podać dane Wi-Fi i hasło panelu w osobnym pliku, aby nie trafiły do repozytorium.
- Kryteria akceptacji:
  1. Sekrety są odczytywane z zewnętrznego pliku podczas kompilacji.
  2. Plik sekretów jest wpisany do `.gitignore`.
  3. Repozytorium zawiera plik przykładowy bez prawdziwych wartości z opisem wymaganych pól.

#### US-047

- ID: US-047
- Tytuł: Przerwanie kompilacji bez hasła
- Opis: Jako osoba budująca firmware chcę, aby kompilacja zatrzymała się przy braku hasła, aby nie powstało urządzenie z niezabezpieczonym panelem.
- Kryteria akceptacji:
  1. Brak pliku sekretów lub puste hasło panelu przerywa kompilację.
  2. Komunikat błędu wskazuje brakujący plik lub pole i nie wyświetla wartości innych sekretów.

#### US-048

- ID: US-048
- Tytuł: Zmiana sieci Wi-Fi
- Opis: Jako rodzic chcę przenieść zegar do innej sieci, wiedząc, że wymaga to ponownego wgrania firmware.
- Kryteria akceptacji:
  1. Ekran konfiguracji i panel nie pozwalają zmienić SSID ani hasła Wi-Fi.
  2. Po zmianie danych w pliku sekretów, kompilacji i flashowaniu urządzenie łączy się z nową siecią.
  3. Po flashowaniu pozostała konfiguracja i wybrany szablon są zachowane.

#### US-049

- ID: US-049
- Tytuł: Aktualizacja firmware przez USB/UART
- Opis: Jako osoba budująca firmware chcę wgrać nową wersję przez kabel USB, zachowując ustawienia użytkownika.
- Kryteria akceptacji:
  1. Firmware można wgrać przez port USB-UART Type-C.
  2. Po aktualizacji konfiguracja jest zachowana, chyba że pamięć została świadomie wyczyszczona.
  3. Przy zmienionej wersji schematu konfiguracja jest migrowana zgodnie z US-055.
  4. Urządzenie nie oferuje aktualizacji OTA.

#### US-050

- ID: US-050
- Tytuł: Ochrona sekretów w działaniu urządzenia
- Opis: Jako rodzic chcę mieć pewność, że hasła nie wyciekną przez ekran, panel ani logi.
- Kryteria akceptacji:
  1. Hasła Wi-Fi i panelu nie występują w żadnym pliku logu.
  2. Hasła nie są wyświetlane na ekranie ani zwracane w odpowiedziach panelu.
  3. Pobrany plik logu nie zawiera sekretów.

### 5.7. Karta TF i tryb awaryjny

#### US-051

- ID: US-051
- Tytuł: Praca bez karty TF
- Opis: Jako dziecko chcę, aby zegar działał i pokazywał postacie, nawet gdy w urządzeniu nie ma karty pamięci.
- Kryteria akceptacji:
  1. Bez karty urządzenie uruchamia ekran główny z szablonem awaryjnym z flash.
  2. Zegar, pogoda i ikony pogody działają normalnie.
  3. Logowanie do plików jest wyłączone.
  4. Ekran główny pokazuje kartę „Uwaga” z tekstem „Brak karty pamięci”. Ten sam błąd widać na ekranie konfiguracji i w panelu.
  5. Brak karty nie resetuje urządzenia i nie blokuje uruchomienia Wi-Fi.

#### US-052

- ID: US-052
- Tytuł: Uszkodzona lub pełna karta TF
- Opis: Jako rodzic chcę, aby uszkodzona lub zapełniona karta nie zawiesiła urządzenia.
- Kryteria akceptacji:
  1. Przy karcie nieczytelnej, uszkodzonej lub pełnej urządzenie przechodzi w tryb awaryjny opisany w US-051.
  2. Ekran główny i ekran konfiguracji pokazują rodzaj błędu: „Karta pamięci nie działa” albo „Karta pamięci jest pełna”.
  3. Urządzenie nie restartuje się cyklicznie z powodu błędu karty.

#### US-053

- ID: US-053
- Tytuł: Karta bez kompletnych szablonów
- Opis: Jako dziecko chcę widzieć postacie, nawet gdy na karcie nie ma poprawnego szablonu.
- Kryteria akceptacji:
  1. Gdy karta działa, ale nie zawiera kompletnego szablonu, używany jest szablon awaryjny.
  2. Logowanie do plików pozostaje włączone.
  3. Konfiguracja pokazuje informację o braku kompletnych szablonów.

#### US-054

- ID: US-054
- Tytuł: Zmiany na karcie podczas pracy
- Opis: Jako rodzic chcę wiedzieć, kiedy nowe szablony skopiowane na kartę zostaną uwzględnione.
- Kryteria akceptacji:
  1. Lista szablonów jest budowana przy uruchomieniu.
  2. Nowe lub zmienione szablony są widoczne po restarcie.
  3. Wyjęcie karty podczas pracy nie jest obsługiwane; dokumentacja użytkownika zawiera to ostrzeżenie.

### 5.8. Trwałość konfiguracji

#### US-055

- ID: US-055
- Tytuł: Migracja konfiguracji po zmianie schematu
- Opis: Jako rodzic chcę, aby po aktualizacji firmware moje ustawienia nie zostały utracone.
- Kryteria akceptacji:
  1. Przy różnicy wersji schematu poprawne wartości znanych pól są zachowane.
  2. Nowe pola otrzymują wartości domyślne.
  3. Wartości niezgodne z nowym typem lub zakresem są zastępowane domyślnymi.
  4. Migracja jest zapisywana w logu.

#### US-056

- ID: US-056
- Tytuł: Zachowanie konfiguracji po zaniku zasilania
- Opis: Jako rodzic chcę, aby odłączenie zasilania nie kasowało ustawień ani nie psuło konfiguracji.
- Kryteria akceptacji:
  1. Po restarcie obowiązuje ostatnia poprawnie zapisana konfiguracja i szablon.
  2. Zanik zasilania podczas zapisu pozostawia poprzednią lub nową konfigurację, nigdy częściową.

#### US-057

- ID: US-057
- Tytuł: Uszkodzona konfiguracja we flash
- Opis: Jako rodzic chcę, aby urządzenie uruchomiło się poprawnie nawet przy uszkodzonej konfiguracji.
- Kryteria akceptacji:
  1. Przy nieczytelnej lub niepoprawnej konfiguracji urządzenie używa wartości domyślnych.
  2. Błąd jest zapisywany w logu i widoczny w konfiguracji.
  3. Kolejny zapis przez rodzica tworzy poprawną konfigurację.

### 5.9. Logowanie

#### US-058

- ID: US-058
- Tytuł: Dzienne pliki logów
- Opis: Jako rodzic chcę, aby zdarzenia były zapisywane w osobnych plikach dla każdego dnia, aby łatwo znaleźć dane z konkretnej daty.
- Kryteria akceptacji:
  1. Każdego dnia powstaje jeden plik tekstowy nazwany datą.
  2. Wpisy zawierają znacznik czasu i opis zdarzeń wymienionych w FR-142.

#### US-059

- ID: US-059
- Tytuł: Rotacja logów
- Opis: Jako rodzic chcę, aby logi nie zapełniły karty pamięci.
- Kryteria akceptacji:
  1. Na karcie znajduje się najwyżej 30 plików logów.
  2. Utworzenie 31. pliku powoduje usunięcie najstarszego.
  3. Rotacja działa również po restartach i zmianie czasu letniego i zimowego.

#### US-060

- ID: US-060
- Tytuł: Logi sprzed synchronizacji czasu
- Opis: Jako rodzic chcę widzieć zdarzenia z początku uruchomienia, nawet zanim zegar zna dokładną godzinę.
- Kryteria akceptacji:
  1. Wpisy sprzed synchronizacji NTP zawierają czas od uruchomienia.
  2. Wpisy te są wyraźnie oznaczone jako niezsynchronizowane.
  3. Po synchronizacji kolejne wpisy używają czasu lokalnego.

### 5.10. Jasność

#### US-061

- ID: US-061
- Tytuł: Automatyczne przyciemnianie nocą
- Opis: Jako rodzic chcę, aby ekran automatycznie przyciemniał się wieczorem i rozjaśniał rano, aby nie przeszkadzał w spaniu.
- Kryteria akceptacji:
  1. W przedziale zmniejszonej jasności, domyślnie 20:00–06:00, używana jest jasność zmniejszona.
  2. Poza przedziałem używana jest jasność dzienna.
  3. Przedział przechodzący przez północ działa poprawnie, np. o 23:00 i 03:00 jasność jest zmniejszona, a o 12:00 dzienna.
  4. Przejście między trybami następuje w ciągu 1 minuty od granicy przedziału.

#### US-062

- ID: US-062
- Tytuł: Jasność przed synchronizacją czasu i po niej
- Opis: Jako rodzic chcę, aby jasność była właściwa zaraz po ustawieniu zegara.
- Kryteria akceptacji:
  1. Przed synchronizacją czasu używana jest jasność dzienna.
  2. Bezpośrednio po synchronizacji stosowany jest tryb wynikający z bieżącej godziny.

#### US-063

- ID: US-063
- Tytuł: Przyciemnianie programowe przy braku sprzętowej regulacji
- Opis: Jako rodzic chcę, aby przyciemnianie działało nawet wtedy, gdy płyta nie pozwala płynnie regulować podświetlenia.
- Kryteria akceptacji:
  1. Możliwość sprzętowej regulacji podświetlenia jest zweryfikowana na urządzeniu docelowym i opisana w dokumentacji technicznej.
  2. Przy braku regulacji sprzętowej zmniejszona jasność jest realizowana programowo dla całego interfejsu.
  3. Przyciemnianie programowe zachowuje rozróżnialność kolorów niebieskiego, czerwonego i białego oraz czytelność godziny.
