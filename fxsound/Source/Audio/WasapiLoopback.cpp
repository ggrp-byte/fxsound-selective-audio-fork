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

#include "WasapiLoopback.h"
#include <atlbase.h> // For CComPtr
#include <audiopolicy.h>
#include <avrt.h>
#include <iostream>

WasapiLoopbackCapture::WasapiLoopbackCapture(DWORD processId)
    : m_processId(processId), m_isCapturing(false)
{
    m_stopEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
}

WasapiLoopbackCapture::~WasapiLoopbackCapture()
{
    Stop();
    if (m_stopEvent)
    {
        CloseHandle(m_stopEvent);
    }
}

bool WasapiLoopbackCapture::IsCapturing() const
{
    return m_isCapturing;
}

DWORD WasapiLoopbackCapture::GetProcessId() const
{
    return m_processId;
}

HRESULT WasapiLoopbackCapture::Start(AudioCaptureCallback callback)
{
    if (m_isCapturing) return S_FALSE;

    HRESULT hr = Initialize();
    if (FAILED(hr))
    {
        Cleanup();
        return hr;
    }

    m_captureCallback = callback;

    hr = m_audioClient->Start();
    if (FAILED(hr))
    {
        Cleanup();
        return hr;
    }

    m_captureThread = CreateThread(nullptr, 0, CaptureThread, this, 0, nullptr);
    if (m_captureThread == nullptr)
    {
        m_audioClient->Stop();
        Cleanup();
        return E_FAIL;
    }

    m_isCapturing = true;
    return S_OK;
}

void WasapiLoopbackCapture::Stop()
{
    if (!m_isCapturing)
    {
        return;
    }

    if (m_stopEvent)
    {
        SetEvent(m_stopEvent);
    }

    if (m_captureThread)
    {
        WaitForSingleObject(m_captureThread, INFINITE);
        CloseHandle(m_captureThread);
        m_captureThread = nullptr;
    }

    m_isCapturing = false;
}

HRESULT WasapiLoopbackCapture::Initialize()
{
    HRESULT hr;
    CComPtr<IMMDeviceEnumerator> pEnumerator;
    CComPtr<IMMDeviceCollection> pDeviceCollection;
    CComPtr<IMMDevice> pDevice;

    hr = CoInitialize(NULL); // Ensure COM is initialized on this thread

    // Find the device and session for the process
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    if (FAILED(hr)) { CoUninitialize(); return hr; }

    hr = pEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pDeviceCollection);
    if (FAILED(hr)) { CoUninitialize(); return hr; }

    UINT deviceCount;
    hr = pDeviceCollection->GetCount(&deviceCount);
    if (FAILED(hr)) { CoUninitialize(); return hr; }

    bool found = false;
    for (UINT i = 0; i < deviceCount; i++)
    {
        hr = pDeviceCollection->Item(i, &pDevice);
        if (FAILED(hr)) continue;

        CComPtr<IAudioSessionManager2> pSessionManager;
        hr = pDevice->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, NULL, (void**)&pSessionManager);
        if (FAILED(hr)) { pDevice.Release(); continue; }

        CComPtr<IAudioSessionEnumerator> pSessionEnumerator;
        hr = pSessionManager->GetSessionEnumerator(&pSessionEnumerator);
        if (FAILED(hr)) { pDevice.Release(); continue; }

        int sessionCount;
        hr = pSessionEnumerator->GetCount(&sessionCount);
        if (FAILED(hr)) { pDevice.Release(); continue; }

        for (int j = 0; j < sessionCount; j++)
        {
            CComPtr<IAudioSessionControl> pSessionControl;
            CComPtr<IAudioSessionControl2> pSessionControl2;
            hr = pSessionEnumerator->GetSession(j, &pSessionControl);
            if (FAILED(hr)) continue;

            hr = pSessionControl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&pSessionControl2);
            if (FAILED(hr)) continue;

            DWORD sessionProcessId = 0;
            hr = pSessionControl2->GetProcessId(&sessionProcessId);

            if (SUCCEEDED(hr) && sessionProcessId == m_processId)
            {
                // Found the process. Now get the device ID and the volume control.
                LPWSTR pwszID = NULL;
                hr = pDevice->GetId(&pwszID);
                if (SUCCEEDED(hr))
                {
                    m_deviceId = pwszID;
                    CoTaskMemFree(pwszID);
                }

                pSessionControl->QueryInterface(__uuidof(ISimpleAudioVolume), (void**)&m_simpleAudioVolume);

                found = true;
                break;
            }
        }
        if (found) break;
        pDevice.Release();
    }

    if (!found) { CoUninitialize(); return E_FAIL; }

    // Now initialize the capture client
    hr = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&m_audioClient);
    if (FAILED(hr)) { CoUninitialize(); return hr; }

    hr = m_audioClient->GetMixFormat(&m_waveFormat);
    if (FAILED(hr)) { CoUninitialize(); return hr; }

    hr = m_audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                  AUDCLNT_STREAMFLAGS_LOOPBACK,
                                  0, 0, m_waveFormat, NULL);
    if (FAILED(hr)) { CoUninitialize(); return hr; }

    hr = m_audioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&m_captureClient);
    if (FAILED(hr)) { CoUninitialize(); return hr; }

    CoUninitialize();
    return S_OK;
}

void WasapiLoopbackCapture::Cleanup()
{
    m_simpleAudioVolume.Release();
    m_captureClient.Release();
    m_audioClient.Release();

    if (m_waveFormat)
    {
        CoTaskMemFree(m_waveFormat);
        m_waveFormat = nullptr;
    }
}

HRESULT WasapiLoopbackCapture::SetSourceMuted(bool isMuted)
{
    if (!m_simpleAudioVolume)
    {
        return E_FAIL;
    }
    return m_simpleAudioVolume->SetMute(isMuted, NULL);
}


DWORD WINAPI WasapiLoopbackCapture::CaptureThread(LPVOID context)
{
    WasapiLoopbackCapture* pThis = static_cast<WasapiLoopbackCapture*>(context);
    CoInitialize(NULL); // Initialize COM for this thread
    pThis->CaptureThreadImpl();
    CoUninitialize();
    return 0;
}

void WasapiLoopbackCapture::CaptureThreadImpl()
{
    HRESULT hr;
    UINT32 packetLength = 0;
    UINT32 numFramesAvailable = 0;
    BYTE *pData = nullptr;
    DWORD flags = 0;

    DWORD taskIndex = 0;
    HANDLE hTask = AvSetMmThreadCharacteristics(L"Pro Audio", &taskIndex);

    bool stillCapturing = true;
    while (stillCapturing)
    {
        DWORD waitResult = WaitForSingleObject(m_stopEvent, 50);
        if (waitResult == WAIT_OBJECT_0)
        {
            stillCapturing = false;
            continue;
        }

        hr = m_captureClient->GetNextPacketSize(&packetLength);
        if (FAILED(hr)) { stillCapturing = false; break; }

        while (packetLength != 0)
        {
            hr = m_captureClient->GetBuffer(&pData, &numFramesAvailable, &flags, NULL, NULL);
            if (FAILED(hr)) { stillCapturing = false; break; }

            if (m_captureCallback && numFramesAvailable > 0)
            {
                UINT32 dataSize = numFramesAvailable * m_waveFormat->nBlockAlign;
                m_captureCallback(this, pData, dataSize, m_waveFormat);
            }

            hr = m_captureClient->ReleaseBuffer(numFramesAvailable);
            if (FAILED(hr)) { stillCapturing = false; break; }

            hr = m_captureClient->GetNextPacketSize(&packetLength);
            if (FAILED(hr)) { stillCapturing = false; break; }
        }
    }

    if (hTask) AvRevertMmThreadCharacteristics(hTask);

    m_audioClient->Stop();
    Cleanup();
}
