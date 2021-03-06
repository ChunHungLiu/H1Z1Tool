/*****************************************************************************\
Copyright (C) 2013-2014 <martinjk at outloook dot com>

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any damages
arising from the use of this software.
Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
claim that you wrote the original software. If you use this software
in a product, an acknowledgment in the product documentation would be
appreciated but is not required.

2. Altered source versions must be plainly marked as such, and must not be
misrepresented as being the original software.

3. This notice may not be removed or altered from any source distribution.
\*****************************************************************************/
#include <include/main.h>
#include <include/CH1Z1.h>
#include <include/H1Z1Def.h>

HANDLE proc;

const MARGINS margin = { 0, 0, 1920, 1080 };
int width = 1920;
int height = 1080;
RECT tSize;

HWND hWnd;
HWND tWnd;

CH1Z1* H1Z1 = nullptr;

std::string GetLanguageString(LanguageConfig lang, const std::string& str)
{
	json::Value val = lang.Object();
	json::Value ob = val.ToObject()["H1Z1"];
	if (ob.GetType() == json::ObjectVal)
		if (ob.HasKey(str))
			return ob[str].ToString();

	return "Could not find language string: " + str;
}

std::string GetWorkingDirectory()
{
	wchar_t szExePath[MAX_PATH] = { 0 };
	GetModuleFileNameW(GetModuleHandle(NULL), szExePath, MAX_PATH);

	// Fix path in string
	for (size_t i = wcslen(szExePath); i > 0; --i)
	{
		if (szExePath[i] == L'\\')
		{
			szExePath[i + 1] = L'\0';
			break;
		}
	}

	std::wstring widePath = szExePath;
	std::string path = std::string(widePath.begin(), widePath.end());
	return path;
}

bool EnableDebugPrivilege() 
{ 
    HANDLE hThis = GetCurrentProcess(); 
    HANDLE hToken; 
    OpenProcessToken(hThis, TOKEN_ADJUST_PRIVILEGES, &hToken); 
    LUID luid; 
    LookupPrivilegeValue(0, TEXT("setDebugPrivilege"), &luid); 
    TOKEN_PRIVILEGES priv; 
    priv.PrivilegeCount = 1; 
    priv.Privileges[0].Luid = luid; 
    priv.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED; 
    AdjustTokenPrivileges(hToken, false, &priv, 0, 0, 0); 
    CloseHandle(hToken); 
    CloseHandle(hThis); 
    return true; 
} 

void TCharToChar(const wchar_t* Src, char* Dest, int Size)
{
#pragma warning(push)
#pragma warning(disable: 4267)
	WideCharToMultiByte(CP_ACP, 0, Src, wcslen(Src)+1, Dest , Size, NULL, NULL);
#pragma warning(pop)
} 

std::pair<int, int> GetScreenDimensions()
{
	RECT desktop;
	const HWND hDesktop = GetDesktopWindow();
	GetWindowRect(hDesktop, &desktop);
	auto horizontal = desktop.right;
	auto vertical = desktop.bottom;

	return std::pair<int, int>(horizontal, vertical);
}

LRESULT CALLBACK WinProc(HWND _hWnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	switch (Message)
	{
		case WM_PAINT:
		{
			// Check if we're forced to stop!
			if(Present_Begin())
				break;

			if(H1Z1 != nullptr)
				H1Z1->Process();

			Present_End();

			break;
		}

		case WM_CREATE:
			DwmExtendFrameIntoClientArea(_hWnd, &margin);
			break;

		case WM_DESTROY:
			return 0;

		default:
			return DefWindowProc(_hWnd, Message, wParam, lParam);
		break;
	}
	return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    EnableDebugPrivilege();

	// Create automatic re-move thread
	std::thread td([&] {
		for (;;)
		{
			tWnd = FindWindowExA(NULL, NULL, TARGET_EXECUTABLE_CLASS, NULL);
			if (tWnd)
			{
				GetWindowRect(tWnd, &tSize);
				width = tSize.right - tSize.left;
				height = tSize.bottom - tSize.top;
				DWORD dwStyle = GetWindowLong(tWnd, GWL_STYLE);
				if (dwStyle & WS_BORDER)
				{
					tSize.top += 23;
					height -= 23;
				}
				MoveWindow(hWnd, tSize.left, tSize.top, width, height, true);
			}
			else
			{
				MessageBoxA(NULL, "Failed to find process with H1Z1! Terminating H1Z1 Tool!", (H1Z1_VERSION), MB_OK | MB_ICONERROR);
				exit(0);
			}

			std::this_thread::sleep_for(/*VS2013 FIX*/std::chrono::milliseconds(50ms)); // 100ms? performance?
		}
	});
	td.detach();

	WNDCLASSEX wClass;
	wClass.cbClsExtra = NULL;
	wClass.cbSize = sizeof(WNDCLASSEX);
	wClass.cbWndExtra = NULL;
	wClass.hbrBackground = (HBRUSH)CreateSolidBrush(RGB(0, 0, 0));
	wClass.hCursor = LoadCursor(0, IDC_ARROW);
	wClass.hIcon = LoadIcon(0, IDI_APPLICATION);
	wClass.hIconSm = LoadIcon(0, IDI_APPLICATION);
	wClass.hInstance = hInstance;
	wClass.lpfnWndProc = WinProc;
	wClass.lpszClassName = TEXT(PROGRAM);
	wClass.lpszMenuName = TEXT(PROGRAM);
	wClass.style = CS_VREDRAW | CS_HREDRAW;

	if (!RegisterClassEx(&wClass))
		exit(1);

	tWnd = FindWindowExA(NULL, NULL, TARGET_EXECUTABLE_CLASS, NULL);
	if (tWnd)
	{
		GetWindowRect(tWnd, &tSize);
		width = tSize.right - tSize.left;
		height = tSize.bottom - tSize.top;
		hWnd = CreateWindowEx(WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED, TEXT(PROGRAM), TEXT(PROGRAM), WS_POPUP, 1, 1, width, height, 0, 0, 0, 0);
		SetLayeredWindowAttributes(hWnd, 0, 1.0f, LWA_ALPHA);
		SetLayeredWindowAttributes(hWnd, 0, RGB(0, 0, 0), LWA_COLORKEY);
		ShowWindow(hWnd, SW_SHOW);
	}

	DWORD dwID;
	HWND sa = FindWindowExA(NULL, NULL, TARGET_EXECUTABLE_CLASS, NULL);
	GetWindowThreadProcessId(sa, &dwID);
	proc = OpenProcess(PROCESS_ALL_ACCESS, false, dwID);
	
	if (!proc)
	{
		MessageBoxA(NULL, "Failed to find process with H1Z1! Cannot lauch H1Z1 Tool!", (H1Z1_VERSION), MB_OK | MB_ICONERROR);
		exit(0);
	}

	MSG Message;
	DirectXInit(hWnd);
	H1Z1 = new CH1Z1(proc);

	for (;;)
	{
		if (PeekMessage(&Message, hWnd, 0, 0, PM_REMOVE))
		{
			DispatchMessage(&Message);
			TranslateMessage(&Message);
		}
	}

	return 0;
}