# FxSound Selective Audio Fork

This repository is a fork of the original [fxsound2/fxsound-app](https://github.com/fxsound2/fxsound-app) project, augmented with a prototype C++ code for WASAPI audio session enumeration and a design document outlining a proposed solution for selective audio processing.

**IMPORTANT:** This fork does NOT contain a fully integrated, working solution for selective audio processing. The modifications are conceptual and serve as a starting point for further development. Implementing the full solution requires significant C++ development, deep understanding of Windows audio architecture (WASAPI, APOs), and potentially advanced techniques for browser tab identification.

## Contents:

*   `main.cpp`: A C++ prototype demonstrating how to enumerate WASAPI audio sessions and retrieve associated process IDs (PIDs) and process names. This code is Windows-specific.
*   `design_document.md`: A detailed design document outlining the proposed architecture for selective audio processing, including challenges and potential solutions.
*   `modification_plan.md`: A detailed plan for iterative modifications to the FxSound codebase.
*   Original FxSound source code.

## How to use this repository:

### 1. Clone the repository:

```bash
git clone https://github.com/ggrp-byte/fxsound-selective-audio-fork.git
cd fxsound-selective-audio-fork
```

### 2. Prepare your Windows Development Environment (Crucial for JUCE projects):

This project relies heavily on the JUCE framework. The compilation errors you encountered (`C1083: Nie można otworzyć pliku dołącz: 'juce_core/juce_core.h'`) indicate that JUCE is not correctly set up or linked in your Visual Studio project. Please follow these steps carefully:

**Prerequisites:**

*   **Windows operating system** (Windows 10 or newer recommended).
*   **Visual Studio 2022** with the "Desktop development with C++" workload installed.
*   **JUCE framework:** Download the latest version from [https://juce.com/get-juce/](https://juce.com/get-juce/).

**Setup Steps:**

1.  **Install JUCE:**
    *   Download the JUCE framework (e.g., `juce-x.x.x-windows.zip`).
    *   Extract the contents to a convenient location on your system (e.g., `C:\JUCE`).

2.  **Configure JUCE with Projucer:**
    *   Navigate to the extracted JUCE folder and find the `Projucer.exe` application (usually in `JUCE\Projucer\Builds\VisualStudio2022\x64\Release` or similar).
    *   Run `Projucer.exe`.
    *   In Projucer, go to `File -> Global Paths...`.
    *   Set the `JUCE Path` to the root directory where you extracted JUCE (e.g., `C:\JUCE`). Click `OK`.

3.  **Open and Re-save the FxSound Project in Projucer:**
    *   In Projucer, open the FxSound project file: `fxsound-selective-audio-fork/fxsound/FxSound.jucer`.
    *   Once opened, simply save the project (`File -> Save Project`) or use `File -> Save and Open in IDE...` (select Visual Studio 2022).
    *   This step is crucial as Projucer regenerates the Visual Studio solution (`.sln`) and project (`.vcxproj`) files, ensuring all JUCE paths and dependencies are correctly configured for your environment.

### 3. Compile and run the FxSound application with simulated modifications (Iteration 1):

This iteration includes simulated code in `fxsound/Source/MainComponent.h` and `fxsound/Source/MainComponent.cpp` to enumerate WASAPI audio sessions and display them in a simple text box within the FxSound GUI.

**Compilation Steps (using Visual Studio):**

1.  **Open the Solution:** Open `fxsound-selective-audio-fork/fxsound/Project/FxSound.sln` in Visual Studio 2022.
2.  **Add `psapi.lib` Linker Dependency:**
    *   In the Solution Explorer, right-click on the `FxSound_App` project and select `Properties`.
    *   Navigate to `Linker -> Input`.
    *   In the `Additional Dependencies` field, add `psapi.lib`. Make sure it's separated by a semicolon if there are other libraries.
    *   Click `Apply` and `OK`.
3.  **Build the Project:** Select your desired configuration (e.g., `Release` or `Debug`, `x64`) and build the solution (`Build -> Build Solution`).

**Running the application:**

*   After a successful build, run the FxSound application from Visual Studio (`Debug -> Start Debugging` or `Start Without Debugging`).
*   You should see the FxSound GUI. Within the main window, there should be a new, simple text box (likely in the top-left corner) that displays a list of active audio sessions, their Process IDs (PIDs), and process names. This list should update periodically.

### 4. Report your findings:

*   **Did the project compile successfully after following the JUCE setup steps?**
*   **Did you encounter any new compilation errors? If so, please provide the exact error messages.**
*   **When running FxSound, do you see the text box with the enumerated audio sessions?**
*   **Does the list of sessions appear to be correct and update as you open/close applications that play audio?**

Your detailed feedback is crucial for the next steps in this iterative development process. Based on your report, I will proceed with further modifications as outlined in `modification_plan.md`.

