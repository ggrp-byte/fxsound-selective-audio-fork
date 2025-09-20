#include <iostream>
#include <vector>
#include <string>
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audiopolicy.h>
#include <psapi.h>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "propsys.lib")

// Function to get process name from PID
std::string GetProcessName(DWORD pid)
{
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (hProcess)
    {
        char buffer[MAX_PATH];
        if (GetModuleFileNameExA(hProcess, 0, buffer, MAX_PATH))
        {
            std::string fullPath = buffer;
            size_t lastSlash = fullPath.find_last_of("\\");
            if (lastSlash != std::string::npos)
            {
                return fullPath.substr(lastSlash + 1);
            }
        }
        CloseHandle(hProcess);
    }
    return "Unknown Process";
}

int main()
{
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
    {
        std::cerr << "CoInitializeEx failed: " << hr << std::endl;
        return hr;
    }

    IMMDeviceEnumerator* pEnumerator = NULL;
    IMMDevice* pDevice = NULL;
    IAudioSessionManager2* pSessionManager = NULL;
    IAudioSessionEnumerator* pSessionEnumerator = NULL;

    try
    {
        hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
        if (FAILED(hr)) throw "CoCreateInstance for IMMDeviceEnumerator failed";

        hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
        if (FAILED(hr)) throw "GetDefaultAudioEndpoint failed";

        hr = pDevice->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, NULL, (void**)&pSessionManager);
        if (FAILED(hr)) throw "Activate for IAudioSessionManager2 failed";

        hr = pSessionManager->GetSessionEnumerator(&pSessionEnumerator);
        if (FAILED(hr)) throw "GetSessionEnumerator failed";

        int sessionCount;
        hr = pSessionEnumerator->GetCount(&sessionCount);
        if (FAILED(hr)) throw "GetCount failed";

        std::cout << "Active Audio Sessions: " << sessionCount << std::endl;

        for (int i = 0; i < sessionCount; ++i)
        {
            IAudioSessionControl* pSessionControl = NULL;
            IAudioSessionControl2* pSessionControl2 = NULL;
            hr = pSessionEnumerator->GetSession(i, &pSessionControl);
            if (FAILED(hr)) continue;

            hr = pSessionControl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&pSessionControl2);
            if (SUCCEEDED(hr))
            {
                DWORD pid = 0;
                hr = pSessionControl2->GetProcessId(&pid);
                if (SUCCEEDED(hr))
                {
                    std::cout << "  Session " << i << ": PID = " << pid << ", Process Name = " << GetProcessName(pid) << std::endl;
                }
                else
                {
                    std::cout << "  Session " << i << ": Could not get PID (Error: " << hr << ")" << std::endl;
                }
                pSessionControl2->Release();
            }
            else
            {
                std::cout << "  Session " << i << ": Could not query IAudioSessionControl2 (Error: " << hr << ")" << std::endl;
            }
            pSessionControl->Release();
        }
    }
    catch (const char* msg)
    {
        std::cerr << "Error: " << msg << ", HRESULT: " << hr << std::endl;
    }

    // Clean up
    if (pSessionEnumerator) pSessionEnumerator->Release();
    if (pSessionManager) pSessionManager->Release();
    if (pDevice) pDevice->Release();
    if (pEnumerator) pEnumerator->Release();

    CoUninitialize();
    return 0;
}


