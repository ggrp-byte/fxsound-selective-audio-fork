# FxSound

FxSound is a digital audio program built for Windows PC's. The background processing, built on a high-fidelity audio engine, acts as a sort of digital soundcard for your system. This means that your signals will have the clean passthrough when FxSound is active. There are active effects for shaping and boosting your sound's volume, timbre, and equalization included on top of this clean processing, allowing you to customize and enhance your sound.

## General Information
* Website: https://www.fxsound.com
* Installer: https://download.fxsound.com/fxsoundlatest
* Source code: https://github.com/fxsound2/fxsound-app
* Issue tracker: https://github.com/fxsound2/fxsound-app/issues
* Forum: https://forum.fxsound.com
* [Donate to FxSound](https://www.paypal.com/donate/?hosted_button_id=JVNQGYXCQ2GPG)

## Build Instructions
### Prerequisites
* Download and install the [latest version of FxSound](https://download.fxsound.com/fxsoundlatest)
* Install [Visual Studio 2022](https://visualstudio.microsoft.com/vs)
* Install [Windows SDK](https://developer.microsoft.com/en-us/windows/downloads/windows-sdk)
* Install [JUCE framework](https://api.juce.com/api/v1/download/juce/latest/windows)

FxSound application requires FxSound Audio Enhancer virtual audio driver. So, to run FxSound application built from source, we need to install FxSound which installs the audio driver.

### Build FxSound from Visual Studio
* Open [fxsound/Project/FxSound.sln](https://github.com/fxsound2/fxsound-app/blob/main/fxsound/Project/FxSound.sln) in Visual Studio
* Build the required configuration and platform and run

### Build after exporting the project form Projucer
FxSound application has three components.
1. FxSound GUI application which uses JUCE framework
2. Audiopassthru module which is used by the application to interact with the audio devices
3. DfxDsp module which is the DSP for processing audio

Due to the some limitations with Projucer, after exporting the Visual Studio solution from Projucer, few changes have to be made in the solution to build FxSound.
1. Since the audiopassthru and DfxDsp dependency projects cannot be added to the solution when FxSound.sln is exported, open fxsound/Project/FxSound.sln in Visual Studio and add the existing projects audiopassthru/audiopassthru.vcxproj, dsp/DfxDsp.vcxproj.
2. From FxSound_App project, add reference to audiopassthru and DfxDsp.
3. If you run FxSound from Visual Studio, to let the application to use the presets, set the Working Directory to ```$(SolutionDir)..\..\bin\$(PlatformTarget)``` in FxSound_App Project->Properties->Debugging setting.

## How to contribute
We welcome anyone who wants to contribute to this project. For more details on how to contribute, follow [this contributing guideline](./CONTRIBUTING.md).

## Acknowledgements
Our special thanks to Advanced Installer for supporting us with Advanced Installer Professional license to build our installer.

[![image](https://github.com/user-attachments/assets/c133fe06-619c-4c17-bce9-f1cf051c5265)](https://www.advancedinstaller.com)

This project uses the [JUCE](https://juce.com) framework, which is licensed under [AGPL v3.0](https://github.com/juce-framework/JUCE/blob/master/LICENSE.md).

Thanks to [Theremino](https://www.theremino.com) for the valuable contributions they do through major feature enhancements in FxSound.

## License
[AGPL v3.0](https://github.com/fxsound2/fxsound-app/blob/main/LICENSE)



---

# FxSound Selective Audio Fork

This repository is a fork of the original [fxsound2/fxsound-app](https://github.com/fxsound2/fxsound-app) project, augmented with a prototype C++ code for WASAPI audio session enumeration and a design document outlining a proposed solution for selective audio processing.

**IMPORTANT:** This fork does NOT contain a fully integrated, working solution for selective audio processing. The modifications are conceptual and serve as a starting point for further development. Implementing the full solution requires significant C++ development, deep understanding of Windows audio architecture (WASAPI, APOs), and potentially advanced techniques for browser tab identification.

## Contents:

*   `main.cpp`: A C++ prototype demonstrating how to enumerate WASAPI audio sessions and retrieve associated process IDs (PIDs) and process names. This code is Windows-specific.
*   `design_document.md`: A detailed design document outlining the proposed architecture for selective audio processing, including challenges and potential solutions.
*   Original FxSound source code.

## How to use this repository:

### 1. Clone the repository:

```bash
git clone https://github.com/ggrp-byte/fxsound-selective-audio-fork.git
cd fxsound-selective-audio-fork
```

### 2. Compile and run the `main.cpp` prototype (Windows only):

This prototype demonstrates the core concept of identifying audio sessions by process. It needs to be compiled and run on a Windows machine.

**Prerequisites:**

*   Windows operating system (Windows 10 or newer recommended).
*   Visual Studio with the "Desktop development with C++" workload installed.

**Compilation Steps (using Visual Studio):**

1.  Open Visual Studio.
2.  Create a new "Empty Project (C++)".
3.  Add the `main.cpp` file from the cloned repository to your project:
    *   Right-click on "Source Files" in the Solution Explorer.
    *   Select "Add" -> "Existing Item..."
    *   Navigate to the `fxsound-selective-audio-fork` directory and select `main.cpp`.
4.  Ensure your project configuration is set to "x64" (or "x86" for 32-bit) and "Debug" or "Release".
5.  Build the project (Build -> Build Solution).

**Running the prototype:**

*   After successful compilation, you will find `main.exe` in your project's output directory (e.g., `x64/Debug` or `x64/Release`).
*   Run `main.exe` from a Command Prompt or PowerShell. It will list active audio sessions, their PIDs, and process names.

### 3. Explore the FxSound source code and design document:

*   Review `design_document.md` to understand the proposed approach for integrating selective audio processing into FxSound.
*   Examine the original FxSound source code to identify areas where the proposed changes could be implemented. Refer to the original FxSound `README.md` (also included in this fork) for their build instructions.

## Further Development:

Integrating the selective audio processing functionality into FxSound would involve:

1.  **Modifying FxSound's `Audiopassthru` module:** To intercept and route specific audio sessions based on user selection.
2.  **Implementing a custom Audio Processing Object (APO):** To apply FxSound's DSP effects only to the selected audio streams.
3.  **Updating FxSound's GUI (JUCE application):** To allow users to select which applications/sessions to process.
4.  **Addressing browser tab identification:** This remains a significant challenge, as Windows APIs do not directly expose per-tab audio streams. Advanced techniques or heuristics would be required.

This repository provides the foundational research and a conceptual prototype to guide these complex development efforts.

