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

#include "ProcessCaptureManager.h"
#include "DfxDsp.h"
#include <atlbase.h>
#include <avrt.h>
#include <iostream>

#define MIXER_BUFFER_SIZE 204800 // 200KB buffer

ProcessCaptureManager::ProcessCaptureManager()
{
    m_renderStopEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    InitializeAudioRenderer();
    m_mixerBuffer.resize(MIXER_BUFFER_SIZE);
}

ProcessCaptureManager::~ProcessCaptureManager()
{
    ShutdownAudioRenderer();
    CloseHandle(m_renderStopEvent);
}

void ProcessCaptureManager::SetDspModule(DfxDsp* dspModule)
{
    m_dspModule = dspModule;
}

void ProcessCaptureManager::StartCaptureForProcess(DWORD processId)
{
    std::lock_guard<std::mutex> lock(m_capturesMutex);

    if (m_captures.find(processId) != m_captures.end())
    {
        return;
    }

    auto capture = std::make_unique<WasapiLoopbackCapture>(processId);

    auto callback = [this](WasapiLoopbackCapture* cap, const BYTE* data, UINT32 size, WAVEFORMATEX* format) {
        this->OnAudioDataReceived(data, size, format);
    };

    HRESULT hr = capture->Start(callback);
    if (SUCCEEDED(hr))
    {
        capture->SetSourceMuted(true);
        m_captures[processId] = std::move(capture);
    }
}

void ProcessCaptureManager::StopCaptureForProcess(DWORD processId)
{
    std::lock_guard<std::mutex> lock(m_capturesMutex);

    auto it = m_captures.find(processId);
    if (it != m_captures.end())
    {
        it->second->SetSourceMuted(false);
        it->second->Stop();
        m_captures.erase(it);
    }
}

bool ProcessCaptureManager::IsProcessCapturing(DWORD processId) const
{
    std::lock_guard<std::mutex> lock(m_capturesMutex);
    return m_captures.find(processId) != m_captures.end();
}

void ProcessCaptureManager::InitializeAudioRenderer()
{
    HRESULT hr;
    CComPtr<IMMDeviceEnumerator> pEnumerator;
    CComPtr<IMMDevice> pDevice;

    CoInitialize(NULL);

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    if (FAILED(hr)) return;

    hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
    if (FAILED(hr)) return;

    hr = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&m_renderClient);
    if (FAILED(hr)) return;

    hr = m_renderClient->GetMixFormat(&m_renderWaveFormat);
    if (FAILED(hr)) return;

    hr = m_renderClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 0, 0, m_renderWaveFormat, NULL);
    if (FAILED(hr)) return;

    hr = m_renderClient->GetService(__uuidof(IAudioRenderClient), (void**)&m_renderRenderClient);
    if (FAILED(hr)) return;

    m_renderClient->Start();
    m_renderThread = CreateThread(nullptr, 0, RenderThread, this, 0, nullptr);
}

void ProcessCaptureManager::ShutdownAudioRenderer()
{
    if (m_renderStopEvent)
    {
        SetEvent(m_renderStopEvent);
    }
    if (m_renderThread)
    {
        WaitForSingleObject(m_renderThread, INFINITE);
        CloseHandle(m_renderThread);
        m_renderThread = nullptr;
    }

    if (m_renderClient) m_renderClient->Stop();
    m_renderRenderClient.Release();
    m_renderClient.Release();
    if(m_renderWaveFormat) CoTaskMemFree(m_renderWaveFormat);
}

void ProcessCaptureManager::OnAudioDataReceived(const BYTE* data, UINT32 size, WAVEFORMATEX* format)
{
    // Simple passthrough, no mixing or format conversion yet
    std::lock_guard<std::mutex> lock(m_mixerMutex);
    if (m_mixerBufferWritePosition + size < MIXER_BUFFER_SIZE)
    {
        memcpy(&m_mixerBuffer[m_mixerBufferWritePosition], data, size);
        m_mixerBufferWritePosition += size;
    }
}

DWORD WINAPI ProcessCaptureManager::RenderThread(LPVOID context)
{
    ProcessCaptureManager* pThis = static_cast<ProcessCaptureManager*>(context);
    CoInitialize(NULL);
    pThis->RenderThreadImpl();
    CoUninitialize();
    return 0;
}

void ProcessCaptureManager::RenderThreadImpl()
{
    HRESULT hr;
    UINT32 bufferFrameCount;
    UINT32 numFramesPadding;
    BYTE *pData;

    hr = m_renderClient->GetBufferSize(&bufferFrameCount);
    if (FAILED(hr)) return;

    bool stillPlaying = true;
    while (stillPlaying)
    {
        DWORD waitResult = WaitForSingleObject(m_renderStopEvent, 20);
        if (waitResult == WAIT_OBJECT_0)
        {
            stillPlaying = false;
            continue;
        }

        hr = m_renderClient->GetCurrentPadding(&numFramesPadding);
        if (FAILED(hr)) { stillPlaying = false; continue; }

        UINT32 numFramesAvailable = bufferFrameCount - numFramesPadding;
        UINT32 bytesAvailable = numFramesAvailable * m_renderWaveFormat->nBlockAlign;

        if (bytesAvailable > 0)
        {
            std::lock_guard<std::mutex> lock(m_mixerMutex);

            UINT32 bytesToProcess = 0;
            UINT32 bytesInData = m_mixerBufferWritePosition - m_mixerBufferReadPosition;
            bytesToProcess = min(bytesAvailable, bytesInData);

            if (bytesToProcess > 0)
            {
                hr = m_renderRenderClient->GetBuffer(numFramesAvailable, &pData);
                if (SUCCEEDED(hr))
                {
                    memcpy(pData, &m_mixerBuffer[m_mixerBufferReadPosition], bytesToProcess);

                    if (m_dspModule && FxModel::getModel().getPowerState())
                    {
                        int numSampleSets = bytesToProcess / m_renderWaveFormat->nBlockAlign;
                        // Assuming the DSP works in-place on a buffer of short ints
                        m_dspModule->processAudio((short int*)pData, (short int*)pData, numSampleSets, false);
                    }

                    m_mixerBufferReadPosition += bytesToProcess;

                    // Simple wrap around for the circular buffer
                    if (m_mixerBufferReadPosition == m_mixerBufferWritePosition) {
                        m_mixerBufferReadPosition = 0;
                        m_mixerBufferWritePosition = 0;
                    }

                    m_renderRenderClient->ReleaseBuffer(bytesToProcess / m_renderWaveFormat->nBlockAlign, 0);
                }
            }
        }
    }
}
