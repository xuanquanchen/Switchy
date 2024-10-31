#include <Windows.h>
#include <stdio.h>
#include <imm.h>
#pragma comment(lib, "Imm32.lib")

#if _DEBUG
#include <stdio.h>
#endif // _DEBUG

//define flag for switching subtypes
#define SIMPLIFIED_CHINESE_SWITCH_METHOD_SHIFT 1
#define SIMPLIFIED_CHINESE_SWITCH_METHOD_CTRL 2
#define SIMPLIFIED_CHINESE_SWITCH_METHOD_CTRL_SPACE 3

typedef NTSTATUS(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);

typedef struct {
	BOOL popup;
} Settings;

void ShowError(LPCSTR message);
DWORD GetOSVersion();
void PressKey(int keyCode);
void ReleaseKey(int keyCode);
void ToggleCapsLockState();
LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
void EnsureLanguageMode(int simplified_chinese_flag);


HHOOK hHook;
BOOL enabled = TRUE;
BOOL keystrokeCapsProcessed = FALSE;
BOOL keystrokeShiftProcessed = FALSE;
BOOL winPressed = FALSE;

Settings settings = {
	.popup = FALSE
};


int main(int argc, char** argv)
{
	if (argc > 1 && strcmp(argv[1], "nopopup") == 0)
	{
		settings.popup = FALSE;
	}
	else
	{
		settings.popup = GetOSVersion() >= 10;
	}
#if _DEBUG
	printf("Pop-up is %s\n", settings.popup ? "enabled" : "disabled");
#endif

	HANDLE hMutex = CreateMutex(0, 0, "Switchy");
	if (GetLastError() == ERROR_ALREADY_EXISTS)
	{
		ShowError("Another instance of Switchy is already running!");
		return 1;
	}

	// Set a global low-level keyboard hook. 
	//WH_KEYBOARD_LL allows the hook to be in the current process without requiring a DLL,
	//making it possible to capture keyboard events system-wide. (Low level hook don't need to inject into a DLL)
	hHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, 0, 0);

	if (hHook == NULL)
	{
		ShowError("Error calling \"SetWindowsHookEx(...)\"");
		return 1;
	}

	MSG messages;
	while (GetMessage(&messages, NULL, 0, 0))
	{
		TranslateMessage(&messages);
		DispatchMessage(&messages);
	}

	UnhookWindowsHookEx(hHook);

	return 0;
}


void ShowError(LPCSTR message)
{
	MessageBox(NULL, message, "Error", MB_OK | MB_ICONERROR);
}


DWORD GetOSVersion()
{
	HMODULE hMod = GetModuleHandleW(L"ntdll.dll");
	RTL_OSVERSIONINFOW osvi = { 0 };

	if (hMod)
	{
		RtlGetVersionPtr p = (RtlGetVersionPtr)GetProcAddress(hMod, "RtlGetVersion");

		if (p)
		{
			osvi.dwOSVersionInfoSize = sizeof(osvi);
			p(&osvi);
		}
	}

	return osvi.dwMajorVersion;
}


void PressKey(int keyCode)
{
	keybd_event(keyCode, 0, 0, 0);
}


void ReleaseKey(int keyCode)
{
	keybd_event(keyCode, 0, KEYEVENTF_KEYUP, 0);
}


void ToggleCapsLockState()
{
	PressKey(VK_CAPITAL);
	ReleaseKey(VK_CAPITAL);
#if _DEBUG
	printf("Caps Lock state has been toggled\n");
#endif // _DEBUG
}

//nCode is the situation of the hook, wParam is the event type now, lParam is a pointer pointing to KBDLLHOOKSTRUCT
LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	KBDLLHOOKSTRUCT* key = (KBDLLHOOKSTRUCT*)lParam;
	if (nCode == HC_ACTION && !(key->flags & LLKHF_INJECTED)) //make sure event is valid and not injected by other program(make sure just perform event on physical input)
	{
#if _DEBUG
		const char* keyStatus = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) ? "pressed" : "released"; //SYSJEYDOWN posted to the window with the keyboard focus when the user presses the F10 key (which activates the menu bar) or holds down the ALT key and then presses another key.
		printf("Key %d has been %s\n", key->vkCode, keyStatus);
#endif // _DEBUG
		if (key->vkCode == VK_CAPITAL)
		{
			if (wParam == WM_SYSKEYDOWN && !keystrokeCapsProcessed)
			{
				keystrokeCapsProcessed = TRUE;
				enabled = !enabled;
#if _DEBUG
				printf("Switchy has been %s\n", enabled ? "enabled" : "disabled");
#endif // _DEBUG
				return 1;
			}
			
			if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP)
			{
				keystrokeCapsProcessed = FALSE;

				if (winPressed)
				{
					winPressed = FALSE;
					ReleaseKey(VK_LWIN);
				}

				if (enabled && !settings.popup)
				{
					if (!keystrokeShiftProcessed)
					{
						//Caps Lock switch logic
						PressKey(VK_MENU); //Alt KEY
						PressKey(VK_LSHIFT);
						ReleaseKey(VK_MENU);
						ReleaseKey(VK_LSHIFT);

						EnsureLanguageMode(SIMPLIFIED_CHINESE_SWITCH_METHOD_CTRL_SPACE);
					}
					else
					{
						keystrokeShiftProcessed = FALSE; //reset situation
					}
				}
			}

			if (!enabled)
			{
				return CallNextHookEx(hHook, nCode, wParam, lParam);
			}

			if (wParam == WM_KEYDOWN && !keystrokeCapsProcessed)
			{
				keystrokeCapsProcessed = TRUE;

				if (keystrokeShiftProcessed == TRUE) //We set shift first
				{
					ToggleCapsLockState();
					return 1;
				}
				else
				{
					if (settings.popup)
					{
						PressKey(VK_LWIN);
						PressKey(VK_SPACE);
						ReleaseKey(VK_SPACE);
						winPressed = TRUE;

					EnsureLanguageMode(SIMPLIFIED_CHINESE_SWITCH_METHOD_CTRL_SPACE);
					}
				}
			}
			return 1;
		}

		else if (key->vkCode == VK_LSHIFT)
		{

			if ((wParam == WM_KEYUP || wParam == WM_SYSKEYUP) && !keystrokeCapsProcessed)
			{
				keystrokeShiftProcessed = FALSE;
			}

			if (!enabled)
			{
				return CallNextHookEx(hHook, nCode, wParam, lParam);
			}

			if (wParam == WM_KEYDOWN && !keystrokeShiftProcessed)
			{
				keystrokeShiftProcessed = TRUE;

				if (keystrokeCapsProcessed == TRUE)
				{
					ToggleCapsLockState();
					if (settings.popup)
					{
						PressKey(VK_LWIN);
						PressKey(VK_SPACE);
						ReleaseKey(VK_SPACE);
						winPressed = TRUE;
					}

					return 0;
				}
			}
			return 0;
		}
	}

	return CallNextHookEx(hHook, nCode, wParam, lParam);
}

/*
void EnsureLanguageMode(int simplified_chinese_flag)
{
	HKL hkl = GetKeyboardLayout(0);
    DWORD dwLayout = (DWORD)(UINT_PTR)hkl & 0xFFFFFFFF;
	//check https://learn.microsoft.com/en-us/windows-hardware/manufacture/desktop/windows-language-pack-default-values?view=windows-11 for full list of keyboard identifier


//#if _DEBUG
	printf("Current keyboard layout: %s\n", layoutName);
//#endif
	//Simplified Chinese
  	if (dwLayout == 0x08040804 || dwLayout == 0x00000804)
    {
#if _DEBUG
        printf("Simplified Chinese layout detected.\n");
#endif

		HWND hWnd = GetForegroundWindow();
		if (hWnd == NULL)
		{
#if _DEBUG
			printf("Failed to get foreground window. \n");
#endif		
			return;
		}

		HIMC hIMC = ImmGetContext(hWnd);
        if (hIMC == NULL)
        {
#if _DEBUG
            printf("Failed to get input context.\n");
#endif
            return;
        }

		DWORD fdwConversion = 0;
        DWORD fdwSentence = 0;

		        // 获取转换状态
        if (ImmGetConversionStatus(hIMC, &fdwConversion, &fdwSentence))
        {
#if _DEBUG
			printf("Current conversion mode: 0x%08X\n", fdwConversion);
#endif
            // 判断是否为中文输入模式
            if (!(fdwConversion & IME_CMODE_NATIVE))
            {
    #if _DEBUG
                printf("Current mode is English, switching to Chinese mode.\n");
    #endif
				if (simplified_chinese_flag == SIMPLIFIED_CHINESE_SWITCH_METHOD_SHIFT)
				{
					PressKey(VK_SHIFT);
					ReleaseKey(VK_SHIFT);
				} 
				else if (simplified_chinese_flag == SIMPLIFIED_CHINESE_SWITCH_METHOD_CTRL)
				{
					PressKey(VK_CONTROL);
					ReleaseKey(VK_CONTROL);
				}

				else if (simplified_chinese_flag == SIMPLIFIED_CHINESE_SWITCH_METHOD_CTRL_SPACE)
				{
		//#if _DEBUG
					printf("Switching language mode using Ctrl + Space method.\n");
		//#endif
					PressKey(VK_CONTROL);
					PressKey(VK_SPACE);
					ReleaseKey(VK_SPACE);
					ReleaseKey(VK_CONTROL);
				}

				else
				{
					printf("Unsupported switch method.\n");
				}
			}
	
			else
			{
				printf("Unsupported layout: %s\n", layoutName);
			}
			
			ImmReleaseContext(hWnd, hIMC);
		}

		else
		{
			printf("Current layout is not Simplified Chinese.\n");
		}
	}
}
*/

void EnsureLanguageMode(int simplified_chinese_flag)
{
    // 获取前台窗口句柄
    HWND hWnd = GetForegroundWindow();
    if (hWnd == NULL)
    {
    #ifdef _DEBUG
        printf("Failed to get foreground window.\n");
    #endif
        return;
    }

    // 获取前台窗口所属的线程 ID
    DWORD dwThreadId = GetWindowThreadProcessId(hWnd, NULL);

    // 将当前线程附加到前台窗口的线程
    DWORD dwCurrentThreadId = GetCurrentThreadId();
    BOOL bAttached = AttachThreadInput(dwCurrentThreadId, dwThreadId, TRUE);

    // 获取前台线程的键盘布局
    HKL hkl = GetKeyboardLayout(dwThreadId);
    DWORD dwLayout = (DWORD)(UINT_PTR)hkl & 0xFFFFFFFF;

    char layoutName[KL_NAMELENGTH];
    snprintf(layoutName, KL_NAMELENGTH, "%08X", dwLayout);

#ifdef _DEBUG
    printf("Current keyboard layout: %s\n", layoutName);
#endif

    // 判断是否为简体中文输入法
    if (dwLayout == 0x08040804 || dwLayout == 0x00000804)
    {
    #ifdef _DEBUG
        printf("Simplified Chinese layout detected.\n");
    #endif

        HIMC hIMC = ImmGetContext(hWnd);
        if (hIMC == NULL)
        {
    #ifdef _DEBUG
            printf("Failed to get input context.\n");
    #endif
            // 解除线程附加
            if (bAttached)
                AttachThreadInput(dwCurrentThreadId, dwThreadId, FALSE);
            return;
        }

        DWORD fdwConversion = 0;
        DWORD fdwSentence = 0;

        // 获取转换状态
        if (ImmGetConversionStatus(hIMC, &fdwConversion, &fdwSentence))
        {
    #ifdef _DEBUG
            printf("Current conversion mode: 0x%08X\n", fdwConversion);
    #endif
            // 判断是否为中文输入模式
            if (!(fdwConversion & IME_CMODE_NATIVE))
            {
    #ifdef _DEBUG
                printf("Current mode is English, switching to Chinese mode.\n");
    #endif
                // 切换到中文输入模式
                // 发送 Ctrl+Space 按键组合
                PressKey(VK_CONTROL);
                PressKey(VK_SPACE);
                ReleaseKey(VK_SPACE);
                ReleaseKey(VK_CONTROL);
            }
            else
            {
    #ifdef _DEBUG
                printf("Already in Chinese input mode.\n");
    #endif
            }
        }
        else
        {
    #ifdef _DEBUG
            printf("Failed to get conversion status.\n");
    #endif
        }

        // 释放输入法上下文句柄
        ImmReleaseContext(hWnd, hIMC);
    }
    else
    {
    #ifdef _DEBUG
        printf("Current layout is not Simplified Chinese.\n");
    #endif
    }

    // 解除线程附加
    if (bAttached)
        AttachThreadInput(dwCurrentThreadId, dwThreadId, FALSE);
}
