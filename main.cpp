#include <iostream>
#include <mmdeviceapi.h>
#include <windows.h>
#include <audiopolicy.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <codecvt>
#include <locale>

// MACROS
#define CTRL_ALT_SHIFT_Q 0x51 // q
#define CTRL_ALT_SHIFT_W 0x57 // w
#define CTRL_ALT_SHIFT_E 0x45 // e
#define CTRL_ALT_SHIFT_R 0x52 // r

// Volume step
#define VOLUME_STEP 0.2f // 2%

void adjustVolume(float step, const std::wstring &processName);
void adjustActiveWindowVolume(float step);
DWORD GetProcessIDByName(const std::wstring &processName);
std::wstring processIdToName(DWORD processId);
std::wstring to_wstring(const std::string& stringToConvert);

int main()
{
    RegisterHotKey(nullptr, 1, MOD_CONTROL | MOD_ALT | MOD_SHIFT, CTRL_ALT_SHIFT_Q);
    RegisterHotKey(nullptr, 2, MOD_CONTROL | MOD_ALT | MOD_SHIFT, CTRL_ALT_SHIFT_W);
    RegisterHotKey(nullptr, 3, MOD_CONTROL | MOD_ALT | MOD_SHIFT, CTRL_ALT_SHIFT_E);
    RegisterHotKey(nullptr, 4, MOD_CONTROL | MOD_ALT | MOD_SHIFT, CTRL_ALT_SHIFT_R);

    MSG msg = {nullptr};

    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (msg.message == WM_HOTKEY) {
            switch (msg.wParam)
            {
                case 1:
                    adjustVolume(-VOLUME_STEP, L"Spotify.exe");
                    break;
                case 2:
                    adjustVolume(VOLUME_STEP, L"Spotify.exe");
                    break;
                case 3:
                    adjustActiveWindowVolume(-VOLUME_STEP);
                    break;
                case 4:
                    adjustActiveWindowVolume(VOLUME_STEP);
                    break;
            }
        }
    }

    return 0;
}

// adjust volume of specific process
void adjustVolume(float step, const std::wstring &processName)
{
    DWORD pid = GetProcessIDByName(processName);
    if (pid == 0)
    {
        std::wcerr << "Process not found: " << std::wstring(processName.begin(), processName.end()) << std::endl;
        return;
    }

    CoInitialize(nullptr);
    IMMDeviceEnumerator *deviceEnumerator = nullptr;
    IMMDevice *device = nullptr;
    IAudioSessionManager2 *audioManager = nullptr;
    IAudioSessionEnumerator *sessionEnumerator = nullptr;

    CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&deviceEnumerator);
    deviceEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &device);
    device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, (void**)&audioManager);
    audioManager->GetSessionEnumerator(&sessionEnumerator);

    int sessionCount;
    sessionEnumerator->GetCount(&sessionCount);

    for (int i = 0; i < sessionCount; i++)
    {
        IAudioSessionControl *sessionControl = nullptr;
        IAudioSessionControl2 *sessionControl2 = nullptr;
        sessionEnumerator->GetSession(i, &sessionControl);
        sessionControl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&sessionControl2);

        DWORD sessionPID;
        sessionControl2->GetProcessId(&sessionPID);

        if (sessionPID == pid)
        {
            ISimpleAudioVolume *audioVolume = nullptr;
            sessionControl2->QueryInterface(__uuidof(ISimpleAudioVolume), (void**)&audioVolume);

            float currentVolume = 0;
            audioVolume->GetMasterVolume(&currentVolume);
            currentVolume += step;
            currentVolume = (currentVolume > 1.0f) ? 1.0f : (currentVolume < 0.0f) ? 0.0f : currentVolume;
            audioVolume->SetMasterVolume(currentVolume, nullptr);

            audioVolume->Release();
            sessionControl2->Release();
            sessionControl->Release();
            break;
        }

        sessionControl2->Release();
        sessionControl->Release();
    }

    sessionEnumerator->Release();
    audioManager->Release();
    device->Release();
    deviceEnumerator->Release();
    CoUninitialize();
}

void adjustActiveWindowVolume(float step)
{
    HWND hwnd = GetForegroundWindow();
    if (hwnd == nullptr)
    {
        std::cerr << "Failed to get foreground window." << std::endl;
    }

    DWORD pid;
    GetWindowThreadProcessId(hwnd, &pid);

    std::wstring processName = processIdToName(pid);

    adjustVolume(step, processName);
}

DWORD GetProcessIDByName(const std::wstring &processName)
{
    PROCESSENTRY32 entry;
    entry.dwSize = sizeof(PROCESSENTRY32);
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (snapshot == INVALID_HANDLE_VALUE)
    {
        std::cerr << "Error: Unable to create process snapshot" << std::endl;
        return 0;
    }

    if (Process32First(snapshot, &entry))
    {
        do
        {
#ifdef UNICODE
            std::wstring exeFile = entry.szExeFile;
#else
            std::wstring exeFile = std::wstring(entry.szExeFile, entry.szExeFile + strlen(entry.szExeFile));
#endif
            if (processName == exeFile)
            {
                CloseHandle(snapshot);
                return entry.th32ProcessID;
            }
        } while (Process32Next(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return 0;
}

std::wstring processIdToName(DWORD processId)
{
    std::string res;
    HANDLE handle = OpenProcess(
            PROCESS_QUERY_LIMITED_INFORMATION,
            FALSE,
            processId
    );
    if (handle)
    {
        DWORD buffSize = 1024;
        CHAR buffer[1024];
        if (QueryFullProcessImageNameA(handle, 0, buffer, &buffSize))
        {
            res = buffer;

            // extract only process name
            size_t pos = res.find_last_of("\\/");
            if (pos != std::string::npos)
            {
                res = res.substr(pos + 1);
            }
        }
        else
        {
            std::cerr << "Error QueryFullProcessImageNameA :" << GetLastError() << std::endl;
        }
        CloseHandle(handle);
    } else
    {
        std::cerr << "Error OpenProcess :" << GetLastError() << std::endl;
    }
    std::wstring ret = to_wstring(res);
    return ret;
}

std::wstring to_wstring(const std::string& stringToConvert)
{
    std::wstring wideString =
            std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(stringToConvert);
    return wideString;
}