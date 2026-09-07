#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winioctl.h>

#include <cstdio>
#include <cwchar>

namespace interception_test {

constexpr DWORD IOCTL_WRITE =
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x820, METHOD_BUFFERED, FILE_ANY_ACCESS);

constexpr DWORD IOCTL_GET_HARDWARE_ID =
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x880, METHOD_BUFFERED, FILE_ANY_ACCESS);

struct KeyboardInputData {
    USHORT UnitId;
    USHORT MakeCode;
    USHORT Flags;
    USHORT Reserved;
    ULONG ExtraInformation;
};

static HANDLE OpenDevice(int slot)
{
    char path[64]{};
    std::snprintf(path, sizeof(path), R"(\\.\interception%02d)", slot);

    return CreateFileA(
        path,
        GENERIC_READ,
        0,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr);
}

static bool GetHardwareId(HANDLE device, wchar_t* buffer, DWORD bufferBytes)
{
    DWORD returned = 0;
    if (!DeviceIoControl(
            device,
            IOCTL_GET_HARDWARE_ID,
            nullptr,
            0,
            buffer,
            bufferBytes,
            &returned,
            nullptr)) {
        return false;
    }

    return returned > 0;
}

static bool SendKey(HANDLE device, USHORT scanCode, bool keyUp)
{
    KeyboardInputData input{};
    input.UnitId = 0;
    input.MakeCode = scanCode;
    input.Flags = keyUp ? 0x0001 : 0x0000;
    input.Reserved = 0;
    input.ExtraInformation = 0;

    DWORD returned = 0;
    const BOOL ok = DeviceIoControl(
        device,
        IOCTL_WRITE,
        &input,
        sizeof(input),
        nullptr,
        0,
        &returned,
        nullptr);

    return ok && returned == sizeof(input);
}

static bool PressScanCode(USHORT scanCode, DWORD holdMs = 40)
{
    for (int slot = 0; slot < 10; ++slot) {
        HANDLE device = OpenDevice(slot);
        if (device == INVALID_HANDLE_VALUE) {
            continue;
        }

        wchar_t hardwareId[512]{};
        const bool present = GetHardwareId(
            device,
            hardwareId,
            static_cast<DWORD>(sizeof(hardwareId)));

        if (!present) {
            CloseHandle(device);
            continue;
        }

        std::wprintf(
            L"[InterceptionTest] keyboard slot=%d hardwareId=%ls\n",
            slot + 1,
            hardwareId);

        const bool down = SendKey(device, scanCode, false);
        Sleep(holdMs);
        const bool up = SendKey(device, scanCode, true);

        CloseHandle(device);

        if (down && up) {
            return true;
        }

        std::printf(
            "[InterceptionTest] send failed on slot=%d, GetLastError=%lu\n",
            slot + 1,
            GetLastError());
    }

    return false;
}

} // namespace interception_test

int main()
{
    std::puts("[InterceptionTest] sending F7 in 3 seconds...");
    Sleep(3000);

    if (!interception_test::PressScanCode(0x41)) {
        std::puts("[InterceptionTest] FAILED: no usable Interception keyboard device.");
        return 1;
    }

    std::puts("[InterceptionTest] F7 sent.");
    return 0;
}
