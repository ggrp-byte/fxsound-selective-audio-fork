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

#include <Windows.h>
#include <AudioClient.h>
#include <mmdeviceapi.h>
#include <string>
#include <functional>

// Forward declaration
class WasapiLoopbackCapture;

// Callback function type for when audio data is captured
using AudioCaptureCallback = std::function<void(WasapiLoopbackCapture*, const BYTE*, UINT32, WAVEFORMATEX*)>;

class WasapiLoopbackCapture
{
public:
    WAVEFORMATEX* GetWaveFormat() const { return m_waveFormat; }
    WasapiLoopbackCapture(DWORD processId);
    ~WasapiLoopbackCapture();

    bool IsCapturing() const;
    DWORD GetProcessId() const;
    HRESULT Start(AudioCaptureCallback callback);
    void Stop();
    HRESULT SetSourceMuted(bool isMuted);

private:
    HRESULT Initialize();
    void Cleanup();
    static DWORD WINAPI CaptureThread(LPVOID context);
    void CaptureThreadImpl();

    DWORD m_processId;
    std::wstring m_deviceId;

    IAudioClient* m_audioClient = nullptr;
    IAudioCaptureClient* m_captureClient = nullptr;
    ISimpleAudioVolume* m_simpleAudioVolume = nullptr;
    WAVEFORMATEX* m_waveFormat = nullptr;

    HANDLE m_captureThread = nullptr;
    HANDLE m_stopEvent = nullptr;

    bool m_isCapturing = false;
    AudioCaptureCallback m_captureCallback;
};
