# FxSound z Modyfikacją Per-Process

Witaj w zmodyfikowanej wersji FxSound! Ta wersja została gruntownie przebudowana, aby umożliwić stosowanie efektów audio do wybranych aplikacji (procesów), zamiast do całego systemu.

Poniżej znajdziesz instrukcję, jak skompilować i uruchomić ten projekt.

## Krok 1: Wymagane Narzędzia

Zanim zaczniesz, upewnij się, że masz zainstalowane następujące oprogramowanie:

1.  **Visual Studio 2022:** Upewnij się, że podczas instalacji zaznaczyłeś komponent **"Programowanie aplikacji klasycznych w języku C++"**. Jest to niezbędne do kompilacji projektu.
2.  **Framework JUCE:** Nasz projekt bazuje na frameworku JUCE. Musisz go pobrać ręcznie.
    *   **Link do pobrania:** [https://juce.com/](https://juce.com/)
    *   Po pobraniu, rozpakuj archiwum i umieść cały folder `JUCE` bezpośrednio na dysku `C:\`, tak aby ścieżka do modułów wyglądała następująco: `C:\JUCE\modules`.

## Krok 2: Przygotowanie Projektu (Automatyczne)

Aby maksymalnie uprościć proces, przygotowałem specjalny skrypt, który sprawdzi zależności i wygeneruje pliki projektu dla Visual Studio.

1.  Otwórz wiersz poleceń (Command Prompt) w głównym katalogu tego repozytorium.
2.  Uruchom skrypt, wpisując komendę:
    ```bash
    build.bat
    ```
3.  Skrypt sprawdzi, czy folder `C:\JUCE` istnieje. Jeśli wszystko pójdzie dobrze, automatycznie wygeneruje najnowsze pliki projektu.

Jeśli skrypt napotka błędy, wyświetli odpowiednie instrukcje. Najczęstszym problemem jest brak folderu JUCE w oczekiwanej lokalizacji.

## Krok 3: Kompilacja w Visual Studio

Po pomyślnym wykonaniu skryptu `build.bat`, wszystko jest gotowe do kompilacji.

1.  Przejdź do folderu `fxsound\Project\`.
2.  Otwórz plik **`FxSound.sln`** w Visual Studio 2022.
3.  Wybierz konfigurację (np. "Debug" lub "Release") i architekturę (np. "x64").
4.  Skompiluj projekt, klikając **Build > Build Solution** (lub naciskając `Ctrl+Shift+B`).

Po zakończeniu kompilacji, plik wykonywalny `.exe` znajdziesz w odpowiednim podfolderze wewnątrz `fxsound\Project\Builds\`.

## Testowanie Nowej Funkcjonalności

Po uruchomieniu aplikacji:
1.  Odtwórz dźwięk w dowolnej aplikacji (np. Spotify, Chrome).
2.  Kliknij ikonę menu w prawym górnym rogu okna FxSound i wybierz "Settings".
3.  W zakładce "Audio" powinieneś zobaczyć listę aktywnych aplikacji wraz z przełącznikami.
4.  Zaznacz przełącznik przy wybranej aplikacji, aby nałożyć na nią efekty FxSound.

Powodzenia! W razie problemów, daj mi znać.