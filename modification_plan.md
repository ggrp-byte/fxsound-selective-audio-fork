# Szczegółowy Plan Modyfikacji Kodu FxSound dla Selektywnego Przetwarzania Audio

## 1. Wprowadzenie

Niniejszy dokument przedstawia szczegółowy plan modyfikacji kodu źródłowego aplikacji FxSound, mający na celu wprowadzenie funkcjonalności selektywnego przetwarzania audio. Obecnie FxSound działa jako globalny procesor dźwięku, wpływając na wszystkie strumienie audio w systemie. Celem modyfikacji jest umożliwienie użytkownikowi wyboru konkretnych aplikacji, a docelowo nawet pojedynczych kart przeglądarki (np. YouTube w Brave/Brave Beta), których strumienie audio będą poddawane efektom FxSound, podczas gdy pozostałe strumienie pozostaną niezmodyfikowane. Plan ten opiera się na wcześniejszej analizie architektury FxSound oraz badaniach nad architekturą audio systemu Windows, w szczególności interfejsów WASAPI (Windows Audio Session API) i APO (Audio Processing Objects).

## 2. Przegląd Architektury FxSound i Punktów Integracji

Zgodnie z `README.md` projektu FxSound, aplikacja składa się z trzech głównych komponentów:

*   **FxSound GUI application (JUCE framework):** Interfejs użytkownika, który pozwala na konfigurację efektów i zarządzanie ustawieniami. Jest to aplikacja oparta na frameworku JUCE, co oznacza, że interakcje z systemem operacyjnym (w tym z API audio) są często abstrakcyjne przez warstwę JUCE, ale w razie potrzeby można używać natywnych API.
*   **Audiopassthru module:** Moduł odpowiedzialny za interakcję z urządzeniami audio. Z naszych badań wynika, że to tutaj odbywa się przechwytywanie strumieni audio i ich przekazywanie do modułu DSP. Prawdopodobnie działa on jako wirtualne urządzenie audio lub wykorzystuje mechanizmy loopback WASAPI.
*   **DfxDsp module:** Moduł Digital Signal Processing (DSP), który zawiera algorytmy efektów audio FxSound (korektor, wzmocnienie, itp.). Ten moduł przyjmuje strumień audio, przetwarza go i zwraca zmodyfikowany strumień.

Kluczowym elementem obecnej architektury FxSound jest jego działanie jako wirtualna karta dźwiękowa lub globalny APO (Audio Processing Object) typu LFX/GFX (Local/Global Effect APO). Aby zaimplementować selektywne przetwarzanie, musimy zmienić to podejście na przetwarzanie na poziomie sesji/aplikacji, co sugeruje wykorzystanie APO typu SFX/MFX (Stream/Mode Effect APO) lub bardziej zaawansowanego zarządzania strumieniami za pomocą WASAPI.

## 3. Kluczowe Technologie Windows Audio

Nasze badania wykazały, że kluczowymi technologiami do osiągnięcia selektywnego przetwarzania audio są:

*   **WASAPI (Windows Audio Session API):** Podstawowy interfejs do zarządzania strumieniami audio. Umożliwia enumerację sesji audio, kontrolę ich głośności i mutowania, a co najważniejsze, identyfikację procesu (PID) powiązanego z daną sesją za pomocą `IAudioSessionControl2::GetProcessId` [1].
*   **APO (Audio Processing Objects):** Obiekty COM, które implementują cyfrowe przetwarzanie sygnału. SFX/MFX APOs są szczególnie interesujące, ponieważ działają na poziomie strumienia aplikacji, co pozwala na wstrzyknięcie efektów FxSound w sposób selektywny dla każdej aplikacji [2].

## 4. Szczegółowy Plan Modyfikacji

### 4.1. Modyfikacja Modułu `Audiopassthru` (Przechwytywanie i Kierowanie Strumieni)

Moduł `Audiopassthru` jest sercem przechwytywania audio w FxSound. Obecnie prawdopodobnie przechwytuje on cały strumień audio systemu. Aby umożliwić selektywne przetwarzanie, musimy zmienić jego rolę na bardziej dynamiczną i świadomą sesji.

**Proponowane zmiany:**

1.  **Enumeracja i Monitorowanie Sesji Audio:**
    *   `Audiopassthru` będzie musiał aktywnie monitorować system pod kątem nowych i zamykanych sesji audio. Będzie to wymagało implementacji interfejsów WASAPI, takich jak `IAudioSessionManager2` i `IAudioSessionEnumerator`.
    *   Dla każdej aktywnej sesji, moduł będzie pobierał jej identyfikator procesu (PID) za pomocą `IAudioSessionControl2::GetProcessId`. Następnie, na podstawie PID, będzie uzyskiwał nazwę pliku wykonywalnego procesu (np. `chrome.exe`, `spotify.exe`).
    *   Moduł będzie utrzymywał wewnętrzną listę aktywnych sesji audio wraz z ich PID i nazwami procesów.

2.  **Dynamiczne Tworzenie i Zarządzanie Strumieniami Loopback:**
    *   Zamiast przechwytywać globalny strumień, `Audiopassthru` będzie tworzył oddzielne strumienie loopback (nagrywanie tego, co jest odtwarzane) dla każdej interesującej sesji audio. Jest to możliwe dzięki funkcjonalności WASAPI loopback [3].
    *   Dla każdej sesji, która ma być przetwarzana przez FxSound, `Audiopassthru` będzie inicjował `IAudioClient` w trybie loopback, aby uzyskać dostęp do surowych danych audio tej konkretnej sesji.
    *   Będzie to wymagało zarządzania wieloma instancjami `IAudioClient` i `IAudioCaptureClient`.

3.  **Kierowanie Strumieni do DSP:**
    *   Przechwycone dane audio z każdej sesji loopback będą kierowane do instancji modułu `DfxDsp`.
    *   Jeśli użytkownik wybrał przetwarzanie dla danej aplikacji, strumień audio z tej aplikacji zostanie przekazany do `DfxDsp`. Jeśli nie, strumień zostanie przepuszczony bez modyfikacji.
    *   Wyzwanie: Skąd FxSound wie, do którego urządzenia wyjściowego ma odesłać przetworzony strumień? Prawdopodobnie będzie musiał stworzyć wirtualne urządzenie audio, które będzie miksować wszystkie przetworzone i nieprzetworzone strumienie, a następnie wysyłać je do rzeczywistego urządzenia wyjściowego.

### 4.2. Modyfikacja Modułu `DfxDsp` (Przetwarzanie Sygnału)

Moduł `DfxDsp` zawiera algorytmy przetwarzania dźwięku. Jego podstawowa funkcjonalność prawdopodobnie nie będzie wymagała drastycznych zmian, ale będzie musiał być przystosowany do pracy z wieloma strumieniami audio jednocześnie.

**Proponowane zmiany:**

1.  **Obsługa Wielu Instancji DSP:**
    *   `DfxDsp` będzie musiał być zaprojektowany tak, aby można było tworzyć wiele jego instancji. Każda instancja będzie odpowiedzialna za przetwarzanie strumienia audio z jednej konkretnej sesji/aplikacji.
    *   Umożliwi to stosowanie różnych presetów FxSound dla różnych aplikacji (np. jeden preset dla YouTube, inny dla Spotify).
2.  **Zarządzanie Presetami:**
    *   Każda instancja `DfxDsp` będzie powiązana z konkretnym presetem efektów, wybranym przez użytkownika dla danej aplikacji.

### 4.3. Modyfikacja Aplikacji GUI (JUCE Framework)

Interfejs użytkownika będzie musiał zostać rozszerzony, aby umożliwić użytkownikowi zarządzanie selektywnym przetwarzaniem audio.

**Proponowane zmiany:**

1.  **Wyświetlanie Aktywnych Sesji Audio:**
    *   GUI będzie musiało komunikować się z modułem `Audiopassthru` (lub nowym modułem zarządzającym sesjami), aby uzyskać listę aktywnych sesji audio (nazwa procesu, PID).
    *   Lista ta powinna być dynamicznie aktualizowana w czasie rzeczywistym.
2.  **Interfejs Wyboru Aplikacji/Sesji:**
    *   Dla każdej wykrytej sesji audio, GUI powinno wyświetlać opcje:
        *   Włącz/Wyłącz przetwarzanie FxSound dla tej sesji.
        *   Wybierz preset FxSound do zastosowania dla tej sesji.
3.  **Zapisywanie Ustawień:**
    *   Ustawienia selektywnego przetwarzania (które aplikacje są przetwarzane, jakie presety są używane) muszą być zapisywane i ładowane przy starcie systemu/aplikacji.

### 4.4. Rozwiązanie Problemu Identyfikacji Kart Przeglądarki (YouTube w Brave/Brave Beta)

To jest najbardziej złożony aspekt zadania. Windows API nie udostępnia bezpośredniego sposobu na rozróżnianie strumieni audio z poszczególnych kart przeglądarki. Wymaga to bardziej zaawansowanych i potencjalnie niestabilnych metod.

**Proponowane podejścia (wymagające dalszych badań i eksperymentów):**

1.  **Analiza Metadanych Sesji (najmniej inwazyjne):**
    *   Niektóre aplikacje (w tym przeglądarki) mogą dodawać niestandardowe metadane do sesji audio, które mogłyby pomóc w identyfikacji źródła (np. tytuł strony internetowej, URL). Należy zbadać, czy Brave/Brave Beta to robią i czy te metadane są dostępne przez WASAPI (np. za pomocą `IPropertyStore` powiązanego z sesją audio) [4].
    *   Jeśli tak, FxSound mógłby analizować te metadane, aby rozróżniać strumienie z różnych kart przeglądarki. Byłoby to jednak zależne od implementacji przeglądarki i mogłoby się zmieniać wraz z jej aktualizacjami.
2.  **Wstrzykiwanie Biblioteki DLL (bardziej inwazyjne):**
    *   Wstrzyknięcie niestandardowej biblioteki DLL do procesu przeglądarki. Ta biblioteka mogłaby przechwytywać wywołania API audio wewnątrz procesu przeglądarki i oznaczać strumienie audio na poziomie poszczególnych kart (np. dodając unikalne identyfikatory lub metadane). FxSound mógłby następnie odczytywać te oznaczenia.
    *   **Wyzwania:** Jest to bardzo skomplikowane, podatne na błędy, może być blokowane przez mechanizmy bezpieczeństwa przeglądarek/systemu (np. ASLR, DEP, antywirusy) i wymagałoby utrzymywania oddzielnych implementacji dla różnych przeglądarek i ich wersji.
3.  **Ograniczenie do Poziomu Aplikacji (rozwiązanie początkowe):**
    *   Na początek, rozwiązanie może ograniczyć się do przetwarzania audio na poziomie całej aplikacji (np. cały proces `brave.exe` lub `brave_beta.exe`). To już byłoby znacznym ulepszeniem w stosunku do globalnego przetwarzania i stanowiłoby solidną podstawę do dalszych eksperymentów z identyfikacją kart.

**Rekomendowane podejście początkowe:** Rozpoczęcie od implementacji selektywnego przetwarzania na poziomie aplikacji (PID), a następnie, jeśli to możliwe i uzasadnione, eksperymentowanie z analizą metadanych sesji dla identyfikacji kart przeglądarki. Wstrzykiwanie DLL jest zbyt ryzykowne i złożone na początkowym etapie.

## 5. Kroki Implementacji (Iteracyjne)

Biorąc pod uwagę złożoność zadania, proponuję następujące iteracyjne kroki implementacji:

### Iteracja 1: Podstawowa Enumeracja i Identyfikacja Procesów

*   **Cel:** Zintegrowanie mechanizmu enumeracji sesji audio i identyfikacji PID/nazw procesów z FxSound.
*   **Modyfikacje:**
    *   W module `Audiopassthru` (lub nowym module zarządzającym sesjami) zaimplementować funkcjonalność z `main.cpp` (WASAPI, `IAudioSessionManager2`, `IAudioSessionEnumerator`, `IAudioSessionControl2::GetProcessId`).
    *   Stworzyć prosty interfejs w GUI FxSound (tymczasowy), który wyświetla listę aktywnych procesów generujących dźwięk.
*   **Testowanie:** Uruchomienie FxSound, odtwarzanie dźwięku z różnych aplikacji (przeglądarka, Spotify, odtwarzacz wideo) i sprawdzenie, czy GUI poprawnie wyświetla te procesy.

### Iteracja 2: Selektywne Przechwytywanie Strumieni

*   **Cel:** Umożliwienie FxSound selektywnego przechwytywania strumieni audio z wybranych procesów.
*   **Modyfikacje:**
    *   W `Audiopassthru` zaimplementować dynamiczne tworzenie strumieni loopback WASAPI dla wybranych sesji audio.
    *   Strumienie z nieprzetwarzanych sesji powinny być kierowane bezpośrednio do urządzenia wyjściowego bez modyfikacji.
    *   Strumienie z przetwarzanych sesji powinny być kierowane do instancji `DfxDsp`.
*   **Testowanie:** Wybranie jednej aplikacji do przetwarzania, sprawdzenie, czy jej dźwięk jest modyfikowany, a dźwięk z innych aplikacji pozostaje niezmieniony.

### Iteracja 3: Integracja z DfxDsp i Zarządzanie Presetami

*   **Cel:** Umożliwienie stosowania różnych presetów FxSound dla różnych aplikacji.
*   **Modyfikacje:**
    *   Zmodyfikować `DfxDsp`, aby mógł działać w wielu instancjach, każda z własnym presetem.
    *   Zintegrować GUI z wyborem presetów dla każdej sesji/aplikacji.
*   **Testowanie:** Zastosowanie różnych presetów do różnych aplikacji i weryfikacja, czy efekty są poprawne.

### Iteracja 4: Ulepszenia GUI i Zapis Ustawień

*   **Cel:** Ukończenie interfejsu użytkownika i zapewnienie trwałości ustawień.
*   **Modyfikacje:**
    *   Finalizacja interfejsu wyboru aplikacji/sesji w GUI.
    *   Implementacja mechanizmu zapisu/ładowania ustawień selektywnego przetwarzania.

### Iteracja 5: Eksperymenty z Identyfikacją Kart Przeglądarki

*   **Cel:** Zbadanie możliwości identyfikacji strumieni audio z poszczególnych kart przeglądarki.
*   **Modyfikacje:**
    *   Zbadanie metadanych sesji WASAPI dla strumieni z Brave/Brave Beta w poszukiwaniu informacji o kartach.
    *   Jeśli znajdzie się użyteczne metadane, zaimplementowanie logiki do ich parsowania i wyświetlania w GUI.
*   **Testowanie:** Odtwarzanie YouTube na jednej karcie, innej treści na drugiej i próba selektywnego przetwarzania tylko jednej z nich.

## 6. Wymagane Narzędzia i Środowisko Deweloperskie (dla Użytkownika)

Do kompilacji i testowania tych modyfikacji niezbędne będzie środowisko deweloperskie Windows:

*   **System Operacyjny:** Windows 10/11 (64-bit).
*   **Visual Studio 2022:** Z zainstalowanym pakietem "Programowanie aplikacji klasycznych w C++" (Desktop development with C++).
*   **Windows SDK:** Najnowsza wersja, zainstalowana wraz z Visual Studio.
*   **JUCE Framework:** FxSound opiera się na JUCE. Będziesz potrzebował JUCE SDK i Projucer do zarządzania projektem.
*   **Znajomość C++ i Windows API:** Do zrozumienia i debugowania kodu.

## 7. Podsumowanie

Implementacja selektywnego przetwarzania audio w FxSound to ambitny projekt. Niniejszy plan przedstawia iteracyjne podejście, zaczynając od podstawowej identyfikacji procesów, a kończąc na eksperymentach z identyfikacją kart przeglądarki. Będę dostarczał Ci zmodyfikowane pliki i instrukcje do kompilacji i testowania, a Twoje informacje zwrotne będą kluczowe dla postępu prac.

## 8. Referencje

[1] `IAudioSessionControl2::GetProcessId` method (audiopolicy.h): [https://learn.microsoft.com/en-us/windows/win32/api/audiopolicy/nf-audiopolicy-iaudiosessioncontrol2-getprocessid](https://learn.microsoft.com/en-us/windows/win32/api/audiopolicy/nf-audiopolicy-iaudiosessioncontrol2-getprocessid)
[2] Audio Processing Object Architecture - Windows drivers: [https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/audio-processing-object-architecture](https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/audio-processing-object-architecture)
[3] Sample – WASAPI loopback capture (record what you hear): [https://matthewvaneerde.wordpress.com/2008/12/16/sample-wasapi-loopback-capture-record-what-you-hear/](https://matthewvaneerde.wordpress.com/2008/12/16/sample-wasapi-loopback-capture-record-what-you-hear/)
[4] IPropertyStore interface (propsys.h): [https://learn.microsoft.com/en-us/windows/win32/api/propsys/nn-propsys-ipropertystore](https://learn.microsoft.com/en-us/windows/win32/api/propsys/nn-propsys-ipropertystore)


