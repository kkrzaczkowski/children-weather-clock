# Zegarek pogodowy dla dziecka

Firmware na Waveshare ESP32-S3-Touch-LCD-7. Ekran pokazuje godzinę, pogodę dla Raciborza i postacie dobrane do temperatury odczuwalnej.

## Sekrety

Skopiuj `secrets.env.example` do `secrets.env` i wpisz SSID, hasło Wi-Fi oraz hasło panelu. Plik `secrets.env` nie wchodzi do repozytorium. Puste SSID albo puste hasło panelu przerywa kompilację. Zmiana sieci wymaga ponownej kompilacji i wgrania firmware.

## Kompilacja i wgranie

Środowisko: ESP-IDF 5.5.6. Port w tym projekcie to COM6, UART.

```
idf.py build
idf.py -p COM6 flash monitor
```

Zwykłe wgranie nie kasuje konfiguracji zapisanej we flashu. Nie ma aktualizacji OTA.

## Karta TF

Na karcie utwórz katalogi:

- `/images/nazwa-szablonu/1.png` … `5.png` — postacie dla kolejnych przedziałów temperatury
- `/logs` — powstaje samo, tu są dzienne pliki logów

Docelowa grafika ma 266×480 px, PNG z przezroczystym tłem. Dłuższy bok najwyżej 960 px, plik najwyżej 400 KB.

Nie wyjmuj karty, gdy zegar jest włączony. Nowe szablony są widoczne po restarcie.

Bez karty zegar nadal pokazuje godzinę i pogodę, a postacie bierze z pamięci flash. Logi do plików są wtedy wyłączone.

## Obsługa

Trzy szybkie dotknięcia lewego górnego rogu otwierają konfigurację. Trzy dotknięcia prawego dolnego rogu zmieniają szablon postaci.

Panel w sieci domowej: `http://adres-ip-urządzenia/configuration`. Adres jest na ekranie konfiguracji. Połączenie jest nieszyfrowane i działa tylko w tej sieci.
