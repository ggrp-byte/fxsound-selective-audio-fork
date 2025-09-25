/*
FxSound
Copyright (C) 2024  FxSound LLC

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include "WasapiLoopback.h"
#include <map>
#include <memory>
#include <mutex>
#include <vector>

class DfxDsp; // Forward declaration

class ProcessCaptureManager
{
public:
    ProcessCaptureManager();
    ~ProcessCaptureManager();

    void SetDspModule(DfxDsp* dspModule);

    // Methods to be called from the UI
    void StartCaptureForProcess(DWORD processId);
    void StopCaptureForProcess(DWORD processId);
    bool IsProcessCapturing(DWORD processId) const;

private:
    void InitializeAudioRenderer();
    void ShutdownAudioRenderer();

    // The callback that WasapiLoopbackCapture instances will call
    void OnAudioDataReceived(const BYTE* data, UINT32 size, WAVEFORMATEX* format);

    // Audio rendering thread
    static DWORD WINAPI RenderThread(LPVOID context);
    void RenderThreadImpl();

    DfxDsp* m_dspModule = nullptr;

    // Map of process IDs to their capture objects
    std::map<DWORD, std::unique_ptr<WasapiLoopbackCapture>> m_captures;
    mutable std::mutex m_capturesMutex;

    // Audio rendering members
    IAudioClient* m_renderClient = nullptr;
    IAudioRenderClient* m_renderRenderClient = nullptr;
    WAVEFORMATEX* m_renderWaveFormat = nullptr;
    HANDLE m_renderThread = nullptr;
    HANDLE m_renderStopEvent = nullptr;

    // A simple circular buffer for mixing audio data between threads
    std::vector<BYTE> m_mixerBuffer;
    UINT32 m_mixerBufferWritePosition = 0;
    UINT32 m_mixerBufferReadPosition = 0;
    std::mutex m_mixerMutex;
};
