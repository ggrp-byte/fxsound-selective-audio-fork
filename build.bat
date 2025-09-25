@echo off
setlocal

echo =================================================
echo FxSound Build Preparation Script
echo =================================================
echo.

REM --- Sprawdzenie zaleznosci JUCE ---
echo Sprawdzanie zaleznosci JUCE w C:\JUCE...
if not exist "C:\JUCE\modules" (
    echo [BLAD] Nie znaleziono modulow JUCE w C:\JUCE\modules.
    echo.
    echo Upewnij sie, ze pobrales framework JUCE i umiesciles go
    echo w katalogu C:\JUCE.
    echo.
    echo Mozesz pobrac JUCE ze strony: https://juce.com/
    goto :error
)
echo Zaleznosc JUCE znaleziona.
echo.

REM --- Sprawdzenie pliku FxSound.jucer ---
echo Sprawdzanie pliku fxsound\FxSound.jucer...
if not exist "fxsound\FxSound.jucer" (
    echo [BLAD] Nie znaleziono pliku fxsound\FxSound.jucer.
    echo Upewnij sie, ze uruchamiasz ten skrypt z glownego katalogu repozytorium.
    goto :error
)
echo Znaleziono plik FxSound.jucer.
echo.


REM --- Ponowne generowanie projektu Visual Studio ---
echo Wyszukiwanie narzedzia Projucer...
echo Ten skrypt zaklada, ze plik 'Projucer.exe' jest w sciezce systemowej (PATH).
echo Jesli nie jest, dodaj go do PATH lub wykonaj komende recznie.
echo.
echo Uruchamianie Projucer w celu wygenerowania projektu Visual Studio...
Projucer.exe --resave "fxsound\FxSound.jucer"

if %errorlevel% neq 0 (
    echo [BLAD] Polecenie Projucer nie powiodlo sie.
    echo Upewnij sie, ze Projucer.exe jest zainstalowany i dostepny w sciezce systemowej PATH.
    echo.
    echo Moze byc konieczne otwarcie pliku .jucer w programie Projucer i zapisanie go recznie.
    goto :error
)

echo.
echo =================================================
echo Sukces!
echo =================================================
echo Projekt Visual Studio zostal poprawnie wygenerowany w katalogu 'fxsound\Project'.
echo Mozesz teraz otworzyc plik 'fxsound\Project\FxSound.sln' i skompilowac aplikacje.
goto :end

:error
echo.
echo Skrypt budujacy napotkal blad. Napraw powyzej wskazane problemy.
pause
exit /b 1

:end
pause
exit /b 0