# Zegarek pogodowy dla dziecka

## Cel produktu

Urządzenie jest zegarkiem pogodowym przeznaczonym dla siedmioletniego dziecka. Ma w prosty, wizualny sposób:

- pokazywać aktualną godzinę;
- prezentować bieżącą i przyszłą pogodę;
- podpowiadać odpowiedni zakres ubioru za pomocą postaci z bajki dobranej do temperatury odczuwalnej.

Wszystkie funkcje opisane w tym dokumencie należą do zakresu MVP.

## Platforma

Urządzenie docelowe to Waveshare ESP32-S3-Touch-LCD-7 z ekranem dotykowym 800×480. Szczegóły sprzętowe i programowe znajdują się w pliku `.ai/technical-specification.md`.

Interfejs jest dostępny wyłącznie w języku polskim. Komunikaty powinny być krótkie i zrozumiałe dla dziecka oraz rodzica.

## Dane pogodowe i czas

Dane pogodowe są pobierane z Open-Meteo dla stałej lokalizacji:

- miejscowość: Racibórz, Polska;
- szerokość geograficzna: `50.09`;
- długość geograficzna: `18.22`;
- strefa czasowa: `Europe/Warsaw`.

Lokalizacja jest zapisana w konfiguracji, ale nie jest zmieniana przez użytkownika.

Urządzenie pobiera:

- aktualną temperaturę;
- aktualną temperaturę odczuwalną;
- aktualny kod pogody WMO;
- informację o opadach;
- prognozowaną temperaturę i temperaturę odczuwalną;
- prawdopodobieństwo oraz wartość prognozowanych opadów.

Godzina prognozy jest konfigurowalna. Przed jej upłynięciem pokazywana jest prognoza na bieżący dzień. Po jej upłynięciu urządzenie pokazuje prognozę na następny dzień na godzinę 08:00. Prognoza musi być opisana czytelną etykietą „dzisiaj” albo „jutro 8:00”.

Czas jest synchronizowany przez NTP dla strefy `Europe/Warsaw`. Do chwili pierwszej synchronizacji urządzenie wyświetla `00:00`, ikonę łączenia oraz ukrywa dane pogodowe i postacie.

## Ekran główny

Ekran główny ma ciemne tło i układ zgodny z `.ai/design.png`.

Na środku wyświetlane są:

- aktualna godzina w formacie `HH:MM`;
- aktualna temperatura i temperatura odczuwalna, rozdzielone ukośnikiem;
- prosta ikona aktualnej pogody umieszczona pod aktualną temperaturą;
- prognozowana temperatura i temperatura odczuwalna;
- etykieta wskazująca, czy prognoza dotyczy dzisiaj, czy jutra o 08:00.

Temperatury są zaokrąglane standardowo do pełnych stopni Celsjusza. Relacja temperatury odczuwalnej do rzeczywistej jest oznaczana kolorem:

- niebieskim, gdy temperatura odczuwalna jest niższa od rzeczywistej;
- czerwonym, gdy temperatura odczuwalna jest wyższa od rzeczywistej;
- białym, gdy obie wartości są równe.

Po lewej stronie wyświetlana jest postać dobrana do aktualnej temperatury odczuwalnej. Po prawej stronie wyświetlana jest postać dobrana do prognozowanej temperatury odczuwalnej.

Na ekranie głównym błędy są przedstawiane wyłącznie za pomocą prostych ikon statusu. Szczegółowe komunikaty są dostępne na ekranie konfiguracji i w logach.

## Przedziały temperatur i grafiki

Każdy szablon zawiera pięć plików PNG:

- temperatura `< −5°C` — `1.png`;
- temperatura `≥ −5°C` i `< 5°C` — `2.png`;
- temperatura `≥ 5°C` i `< 15°C` — `3.png`;
- temperatura `≥ 15°C` i `≤ 25°C` — `4.png`;
- temperatura `> 25°C` — `5.png`.

Szablony są przechowywane na karcie TF w katalogach `/images/[nazwa-szablonu]`. Nazwy katalogów są sortowane alfabetycznie. Niekompletne szablony są pomijane, a po ostatnim poprawnym szablonie wybierany jest ponownie pierwszy.

Trzykrotne dotknięcie prawego dolnego rogu ekranu głównego wybiera następny szablon. Wybór jest natychmiast zapisywany we flash i przywracany po restarcie.

Domyślny szablon awaryjny jest przechowywany we flash urządzenia.

## Ikony pogody

Proste ikony aktualnej pogody są przechowywane we flash. Należy zdefiniować pełne mapowanie kodów WMO obejmujące co najmniej:

- słońce;
- częściowe zachmurzenie;
- chmury;
- deszcz;
- śnieg;
- mgłę;
- burzę;
- marznący deszcz;
- nieznany stan pogody.

Ikona dotyczy wyłącznie aktualnej pogody i jest wyświetlana pod aktualną temperaturą.

## Ekran konfiguracji

Ekran konfiguracji otwiera się po trzech zwykłych dotknięciach lewego górnego rogu. Nie ma dodatkowego zabezpieczenia wejścia.

Ekran zawiera:

- wybór szablonu;
- interwał odświeżania pogody, domyślnie 2 godziny;
- jasność dzienną;
- zmniejszoną jasność;
- przedział zmniejszonej jasności, domyślnie `20:00–06:00`;
- aktualny czas i status synchronizacji;
- status Wi-Fi oraz zapisane podczas kompilacji SSID, tylko do odczytu;
- konfigurowalną godzinę prognozy;
- bieżące i prognozowane dane pogodowe;
- status Open-Meteo;
- status karty TF;
- szczegółowe komunikaty błędów;
- ustawienia rozmiaru czcionki;
- przyciski „Zapisz” i „Anuluj”.

Rozmiary czcionek są wybierane spośród kilku przetestowanych wariantów. Godzina, temperatury i etykiety prognozy mogą być skalowane niezależnie.

Zmiany są walidowane przed zapisem. Ostatnia poprawnie zapisana zmiana nadpisuje poprzednią; nie stosuje się wersjonowania zmian konfiguracji.

## Konfiguracja i sekrety

Konfiguracja jest przechowywana we flash urządzenia i posiada jeden schemat określający:

- wartości domyślne;
- typy danych;
- dozwolone zakresy;
- numer wersji schematu.

SSID i hasło Wi-Fi są wartościami docelowymi dostarczanymi podczas kompilacji. Nie można ich zmienić z ekranu urządzenia ani panelu WWW. Zmiana sieci wymaga ponownej kompilacji i flashowania firmware.

Hasło do panelu WWW również jest ustawiane podczas kompilacji. Sekrety są pobierane z zewnętrznego pliku, który:

- nie jest przechowywany w repozytorium;
- znajduje się w `.gitignore`;
- ma wersjonowany plik przykładowy bez prawdziwych wartości;
- jest wymagany do kompilacji — brak hasła przerywa proces budowania.

Hasła Wi-Fi i panelu WWW nie mogą być zapisywane w logach ani prezentowane w jawnej postaci.

## Panel WWW

Panel WWW jest dostępny wyłącznie w sieci lokalnej przez HTTP pod adresem:

`http://[adres-ip-urządzenia]/configuration`

Panel wymaga uwierzytelnienia stałym hasłem i udostępnia ustawienia urządzenia oraz informacje diagnostyczne. Konfiguracja we flash jest wspólnym źródłem danych dla ekranu urządzenia i panelu WWW.

Panel pokazuje listę maksymalnie 30 plików logów i umożliwia pobieranie każdego pliku osobno.

## Odświeżanie i obsługa błędów

Po uruchomieniu lub odzyskaniu połączenia urządzenie natychmiast synchronizuje czas i pogodę, bez oczekiwania na kolejny interwał.

W przypadku błędu Open-Meteo urządzenie:

1. wykonuje trzy próby z rosnącymi odstępami;
2. zachowuje ostatnie poprawne dane;
3. zapisuje szczegóły błędu w logu;
4. po wyczerpaniu prób wraca do normalnego interwału odświeżania.

Dane pogodowe są ukrywane po upływie czterokrotności skonfigurowanego interwału od ostatniej poprawnej aktualizacji. Dla domyślnego interwału 2 godzin następuje to po 8 godzinach.

Po utracie internetu zegar pozostaje widoczny, a po wygaśnięciu danych temperatury, ikona pogody i postacie zostają ukryte. Po odzyskaniu sieci urządzenie automatycznie wraca do normalnej pracy.

## Karta TF i tryb awaryjny

Karta TF przechowuje szablony postaci i logi. Wyjmowanie lub wymiana karty podczas działania urządzenia nie są obsługiwane.

Jeżeli karta jest nieobecna, uszkodzona lub pełna, urządzenie:

- uruchamia ekran z domyślnymi grafikami zapisanymi we flash;
- wyłącza logowanie do plików;
- pokazuje błąd na ekranie konfiguracji.

## Logowanie

Urządzenie tworzy jeden tekstowy plik logu dziennie. Przechowuje maksymalnie 30 plików, a po przekroczeniu limitu usuwa najstarsze.

Logi obejmują:

- znaczniki czasu;
- uruchomienia i restarty;
- status oraz zdarzenia Wi-Fi;
- synchronizację czasu;
- wywołania Open-Meteo;
- kody i szczegóły błędów.

Przed synchronizacją NTP wpisy używają czasu od uruchomienia i są oznaczone jako niezsynchronizowane.

## Jasność

Interfejs obsługuje jasność dzienną i zmniejszoną jasność w przedziale przechodzącym przez północ, np. `20:00–06:00`. Zmiana ustawień lub synchronizacja czasu natychmiast stosuje odpowiedni tryb.

Należy zweryfikować możliwość sprzętowego sterowania podświetleniem. Jeżeli płyta jej nie zapewnia, przyciemnianie zostanie zrealizowane programowo przez zmianę wyglądu całego interfejsu.

## Aktualizacje

Firmware jest aktualizowany przez USB/UART. Aktualizacje OTA pozostają poza zakresem projektu.
