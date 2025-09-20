# Projekt Rozwiązania Selektywnego Przetwarzania Audio dla FxSound

## 1. Cel
Celem tego projektu jest modyfikacja aplikacji FxSound w celu umożliwienia selektywnego przetwarzania dźwięku dla wybranych aplikacji lub nawet konkretnych kart przeglądarki (np. YouTube), zamiast stosowania globalnych zmian systemowych. Użytkownik będzie mógł wybrać, które strumienie audio mają być poddane efektom FxSound, a które mają pozostać niezmodyfikowane.

## 2. Architektura FxSound (zrozumienie bieżące)
Zgodnie z `README.md` i wstępną analizą, FxSound składa się z trzech głównych komponentów:
*   **FxSound GUI application (JUCE framework):** Interfejs użytkownika do zarządzania ustawieniami.
*   **Audiopassthru module:** Moduł odpowiedzialny za interakcję z urządzeniami audio. Prawdopodobnie to tutaj odbywa się przechwytywanie i przekazywanie strumieni audio.
*   **DfxDsp module:** Moduł DSP (Digital Signal Processing) odpowiedzialny za faktyczne przetwarzanie dźwięku (efekty, korekcja, itp.).

FxSound działa jako wirtualna karta dźwiękowa, co oznacza, że przechwytuje cały strumień audio systemu przed jego wysłaniem do rzeczywistego urządzenia wyjściowego.

## 3. Architektura Audio w Windows (kluczowe wnioski z badań)
*   **WASAPI (Windows Audio Session API):** Jest to podstawowy interfejs do zarządzania przepływem danych audio między aplikacjami a urządzeniami końcowymi audio w systemie Windows.
*   **Sesje Audio:** WASAPI grupuje powiązane strumienie audio w 


sesje audio. Każda aplikacja zazwyczaj ma jedną lub więcej sesji audio. Kluczowe jest to, że WASAPI pozwala na indywidualne kontrolowanie poziomu głośności każdej sesji audio.
*   **IAudioSessionControl2::GetProcessId:** Ta metoda interfejsu `IAudioSessionControl2` pozwala na uzyskanie identyfikatora procesu (PID) powiązanego z daną sesją audio. Jest to kluczowe do identyfikacji, która aplikacja generuje dany strumień audio.
*   **APO (Audio Processing Objects):** Obiekty przetwarzania audio (APO) to oprogramowanie oparte na COM, które zapewnia cyfrowe przetwarzanie sygnału (DSP) dla strumieni audio w systemie Windows. Istnieją różne typy APO, w tym:
    *   **SFX/MFX (Stream/Mode Effect APOs):** Są one instancjonowane dla każdego strumienia aplikacji i przetwarzają sygnał audio wchodzący i wychodzący z pojedynczej aplikacji. To jest potencjalnie najbardziej obiecujące miejsce do wstrzyknięcia efektów FxSound w sposób selektywny.
    *   **LFX/GFX (Local/Global Effect APOs):** Działają na poziomie urządzenia, wpływając na wszystkie strumienie audio przechodzące przez dane urządzenie. To jest obecne podejście FxSound, które chcemy zmienić.

## 4. Proponowane Rozwiązanie

### 4.1. Ogólna Koncepcja
Zamiast globalnego przetwarzania audio, FxSound powinien działać jako APO typu SFX/MFX, co pozwoli na przetwarzanie audio na poziomie poszczególnych sesji. Aplikacja GUI FxSound będzie odpowiedzialna za enumerację aktywnych sesji audio, wyświetlanie ich użytkownikowi i umożliwienie wyboru, które sesje mają być przetwarzane przez moduł DfxDsp.

### 4.2. Kluczowe Kroki Implementacji

1.  **Enumeracja Sesji Audio:**
    *   Wykorzystanie interfejsów WASAPI (`IAudioSessionManager2`, `IAudioSessionEnumerator`, `IAudioSessionControl`, `IAudioSessionControl2`) do pobrania listy wszystkich aktywnych sesji audio w systemie.
    *   Dla każdej sesji, użycie `IAudioSessionControl2::GetProcessId` do uzyskania PID procesu, który ją utworzył.
    *   Następnie, na podstawie PID, uzyskanie nazwy procesu (np. `chrome.exe`, `firefox.exe`, `vlc.exe`).

2.  **Identyfikacja Strumieni z Przeglądarek (np. YouTube):**
    *   Identyfikacja strumieni z przeglądarek jest bardziej złożona, ponieważ wiele kart może działać w ramach jednego procesu przeglądarki. System Windows nie udostępnia bezpośredniego API do rozróżniania strumieni audio z poszczególnych kart przeglądarki.
    *   **Możliwe podejścia (wymagające dalszych badań/eksperymentów):**
        *   **Analiza metadanych sesji:** Niektóre aplikacje (w tym przeglądarki) mogą dodawać niestandardowe metadane do sesji audio, które mogłyby pomóc w identyfikacji źródła (np. tytuł strony internetowej). Wymaga to sprawdzenia, czy przeglądarki takie jak Chrome/Firefox to robią i czy te metadane są dostępne przez WASAPI.
        *   **Wstrzykiwanie kodu (bardziej inwazyjne):** Wstrzyknięcie biblioteki DLL do procesu przeglądarki, która mogłaby przechwytywać i oznaczać strumienie audio na poziomie poszczególnych kart. Jest to jednak bardzo skomplikowane, podatne na błędy i może być blokowane przez mechanizmy bezpieczeństwa przeglądarek/systemu.
        *   **Ograniczenie do poziomu aplikacji:** Na początek, rozwiązanie może ograniczyć się do przetwarzania audio na poziomie całej aplikacji (np. cały Chrome, cały Firefox), co już byłoby znacznym ulepszeniem w stosunku do globalnego przetwarzania.

3.  **Integracja z FxSound GUI:**
    *   GUI FxSound powinno wyświetlać listę wykrytych aplikacji/sesji audio (np. 


Chrome, Firefox, Spotify) wraz z opcją włączenia/wyłączenia efektów FxSound dla każdej z nich.
    *   Użytkownik powinien mieć możliwość tworzenia profili (presetów) dla różnych aplikacji.

4.  **Modyfikacja Modułu Audiopassthru:**
    *   Moduł ten będzie musiał zostać zmodyfikowany, aby nie tylko przechwytywał globalny strumień audio, ale także był w stanie identyfikować poszczególne sesje audio i ich procesy źródłowe.
    *   W zależności od wyboru użytkownika w GUI, `Audiopassthru` będzie musiał przekierować wybrane strumienie audio do modułu `DfxDsp` w celu przetworzenia, a pozostałe strumienie przepuścić bez modyfikacji.
    *   Może to wymagać implementacji niestandardowego APO typu SFX/MFX, który będzie wstrzykiwany do ścieżki audio wybranych procesów.

5.  **Modyfikacja Modułu DfxDsp:**
    *   Moduł `DfxDsp` prawdopodobnie nie będzie wymagał dużych zmian w samej logice przetwarzania, ale będzie musiał być w stanie przyjmować strumienie audio z `Audiopassthru` i zwracać przetworzone strumienie.
    *   Kluczowe będzie zarządzanie wieloma instancjami DSP, jeśli użytkownik będzie chciał stosować różne presety dla różnych aplikacji jednocześnie.

## 5. Wyzwania i Ryzyka

*   **Identyfikacja kart przeglądarki:** Jak wspomniano, jest to największe wyzwanie. Bez bezpośredniego API, rozwiązanie będzie musiało polegać na heurystyce lub bardziej inwazyjnych metodach, co zwiększa złożoność i ryzyko niestabilności.
*   **Stabilność i wydajność:** Wstrzykiwanie APO i zarządzanie wieloma strumieniami audio może wpłynąć na stabilność systemu i wprowadzić opóźnienia (latency).
*   **Kompatybilność:** Zmiany w architekturze audio Windows mogą wymagać częstych aktualizacji.
*   **Uprawnienia:** Wymagane uprawnienia do wstrzykiwania APO i monitorowania procesów mogą być problematyczne.

## 6. Plan Dalszych Działań

1.  **Prototypowanie enumeracji sesji:** Stworzenie małej aplikacji testowej, która enumeruje sesje audio, pobiera PID i nazwy procesów.
2.  **Eksperymenty z APO:** Zbadanie możliwości tworzenia i rejestrowania niestandardowych APO typu SFX/MFX.
3.  **Integracja z JUCE:** Zbadanie, w jaki sposób JUCE (używane przez FxSound GUI) może współdziałać z WASAPI i niestandardowymi APO.

## 7. Podsumowanie
Rozwiązanie selektywnego przetwarzania audio jest technicznie wykonalne na poziomie aplikacji, wykorzystując WASAPI i APO. Największym wyzwaniem pozostaje precyzyjna kontrola na poziomie kart przeglądarki. Proponuje się rozpoczęcie od implementacji na poziomie aplikacji, a następnie, jeśli to możliwe i uzasadnione, rozważenie bardziej zaawansowanych metod dla kart przeglądarki.

