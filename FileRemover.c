// FileRemover.cpp : Defines the entry point for the application.
//

#define WIN32_LEAN_AND_MEAN
#pragma optimize("gsy",on)
//#pragma comment(linker,"/merge:.text=.data")
#pragma comment(linker,"/opt:nowin98")

#include <pcp_includes.h>
#include <pcp_definitions.h>

#include "resource.h"

#include <pcp_filenotify.h>
#include <pcp_systray.h>
#include <pcp_string.h>
#include <pcp_registry.h>
#include <pcp_browse.h>
#include <pcp_mem.h>

#define IDC_TRAYICON		666

#define REG_ROOT			HKEY_CURRENT_USER
#define REG_FILEREMOVER		_T("Software\\da.box Software Division\\FileRemover")

BOOL CALLBACK FileRemover_DialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void FileRemover_ReadSettings(HWND hwnd, BOOL bRead);
void FileRemover_FileNotifyCallback(LPFILENOTIFYDATA lpfnd, DWORD dwChange);

LRESULT CALLBACK FileRemover_ListWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

void Path_TraverseAndRemoveDirectory(LPSTR pszPath);

static LPFILENOTIFYSTRUCT s_pfns;
static HWND s_hwndDaMainDialog;
static BOOL s_bStartHidden;
static HINSTANCE g_hInstance;

int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
{
	MSG msg;

	g_hInstance = hInstance;

	FileNotify_CreateNotifier(&s_pfns, FileRemover_FileNotifyCallback);

	s_hwndDaMainDialog = CreateDialog(g_hInstance, MAKEINTRESOURCE(IDD_DAMAINDIALOG), NULL, FileRemover_DialogProc);

	if (!s_bStartHidden)
		ShowWindow(s_hwndDaMainDialog, SW_SHOW);

	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (!IsDialogMessage(s_hwndDaMainDialog, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	FileNotify_DestroyNotifier(s_pfns);

	return (msg.wParam);
}

BOOL CALLBACK FileRemover_DialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_INITDIALOG:
		Systray_AddIcon(hwnd, IDC_TRAYICON, LoadIcon(g_hInstance, MAKEINTRESOURCE(IDI_MAIN)), _T("pcppopper's FileRemover"));
		FileRemover_ReadSettings(hwnd, TRUE);
		FileNotify_StartMonitoring(s_pfns);
		CheckDlgButton(hwnd, IDC_CHECK_HIDEATSTARTUP, s_bStartHidden);
	return (TRUE);
	case WM_TRAYNOTIFY:
		switch (lParam)
		{
		case WM_LBUTTONUP:
			ShowWindow(hwnd, IsWindowVisible(hwnd) ? SW_HIDE : SW_SHOW);
			if (IsWindowVisible(hwnd))
			{
				SetForegroundWindow(hwnd);
				BringWindowToTop(hwnd);
				SetFocus(hwnd);
			}
		break;
		case WM_RBUTTONUP:
		{
			POINT pt;
			HMENU hMenu = LoadMenu(g_hInstance, MAKEINTRESOURCE(IDR_TRAYMENU));
			HMENU hSubMenu = GetSubMenu(hMenu, 0);

			GetCursorPos(&pt);

			SetForegroundWindow(hwnd);
			SetMenuDefaultItem(hSubMenu, IDM_TRAYMENU_SHOW, FALSE);
			TrackPopupMenu(hSubMenu, TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RIGHTBUTTON, pt.x, pt.y, 0, s_hwndDaMainDialog, NULL);
			DestroyMenu(hMenu);
		}
		break;
		}
	break;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
			FileRemover_ReadSettings(hwnd, FALSE);
		case IDCANCEL:
			ShowWindow(hwnd, SW_HIDE);
		return (0);
		case IDC_BUTTON_ADDDIR:
		{
			BROWSEINFOEX bix;
			TCHAR szDir[MAX_PATH] = _T("");

			INITSTRUCT(bix, FALSE);
			bix.hwndOwner		= hwnd;
			bix.pszPath			= szDir;
			bix.pszTitle		= _T("Browse for directory to monitor");

			if (Browse_ForPath(&bix))
			{
				TCHAR szMsg[MAX_PATH + 40];

				_stprintf(szMsg, _T("Are you sure you wish to add the directory '%s' to the list of directories to remove on creation?"), szDir);

				if (MessageBox(hwnd, szMsg, _T("Add Directory"), MB_YESNO | MB_ICONEXCLAMATION | MB_APPLMODAL) == IDYES)
					ListBox_AddString(GetDlgItem(hwnd, IDC_LIST_FILES), szDir);
			}
		}
		return (0);
		case IDC_BUTTON_ADDFILE:
		{
			BROWSEINFOEX bix;
			TCHAR szFile[MAX_PATH] = _T("");

			INITSTRUCT(bix, FALSE);
			bix.hwndOwner		= hwnd;
			bix.pszFile			= szFile;
			bix.pszFilter		= _T("All Files (*.*)\0*.*");
			bix.pszTitle		= _T("Browse for file to monitor");

			if (Browse_ForFile(&bix))
				ListBox_AddString(GetDlgItem(hwnd, IDC_LIST_FILES), szFile);
		}
		return (0);
		case IDC_BUTTON_REMOVE:
		{
			HWND hwndList = GetDlgItem(hwnd, IDC_LIST_FILES);
			int nIndex = ListBox_GetCurSel(hwndList);

			if (nIndex != LB_ERR)
				ListBox_DeleteString(hwndList, nIndex);
			else
				MessageBox(hwnd, _T("Please select a file in the listbox before trying to remove it"), _T("Usage Tip"), MB_ICONEXCLAMATION | MB_OK);
		}
		return (0);
		case IDM_TRAYMENU_SHOW:
			ShowWindow(hwnd, IsWindowVisible(hwnd) ? SW_HIDE : SW_SHOW);
			if (IsWindowVisible(hwnd))
			{
				SetForegroundWindow(hwnd);
				BringWindowToTop(hwnd);
				SetFocus(hwnd);
			}
		return (0);
		case IDM_TRAYMENU_EXIT:
			Systray_DeleteIcon(hwnd, IDC_TRAYICON);
			DestroyWindow(hwnd);
		return (0);
		}
	break;
	case WM_CLOSE:
		ShowWindow(hwnd, SW_HIDE);
	break;
	case WM_QUERYENDSESSION:
		Systray_DeleteIcon(hwnd, IDC_TRAYICON);
		DestroyWindow(hwnd);
	return (TRUE);
	case WM_DESTROY:
		PostQuitMessage(0);
	break;
	}

	return (FALSE);
}

void FileRemover_ReadSettings(HWND hwnd, BOOL bRead)
{
	if (bRead)
	{
		HKEY hRootKey;
		HKEY hSubKey;
		DWORD dwType;
		DWORD dwSize;
		LPTSTR pszFiles;
		LPTSTR psz;
		HWND hwndList = GetDlgItem(hwnd, IDC_LIST_FILES);

		RegOpenKey(REG_ROOT, REG_FILEREMOVER, &hRootKey);
		RegOpenKey(hRootKey, _T("General Settings"), &hSubKey);

		s_bStartHidden = Registry_GetDW(hSubKey, _T("Start Hidden"), 1);

		RegCloseKey(hSubKey);

		if (RegOpenKey(hRootKey, _T("Files"), &hSubKey) != ERROR_SUCCESS)
		{
			RegCloseKey(hRootKey);

			return;
		}

		RegQueryValueEx(hSubKey, _T("Files To Monitor"), 0, &dwType, NULL, &dwSize);

		if (dwType != REG_MULTI_SZ)
		{
			RegCloseKey(hSubKey);
			RegCloseKey(hRootKey);

			return;
		}

		pszFiles = Mem_AllocStr(dwSize);

		RegQueryValueEx(hSubKey, _T("Files To Monitor"), NULL, &dwType, pszFiles, &dwSize);

		for (psz = pszFiles; *psz; )
		{
			WIN32_FIND_DATA wfd;
			HANDLE hSearch = FindFirstFile(psz, &wfd);

			FindClose(hSearch);

			if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				Path_TraverseAndRemoveDirectory(psz);
				FileNotify_AddFile(s_pfns, psz, FN_NOTIFY_DEFAULT | FILE_NOTIFY_CHANGE_DIR_NAME);
			}
			else
			{
				DeleteFile(psz);
				FileNotify_AddFile(s_pfns, psz, FN_NOTIFY_DEFAULT);
			}

			ListBox_AddString(hwndList, psz);

			for ( ; *psz++; )
				; /* Empty Body */
		}

		Mem_Free(pszFiles);
		
		RegCloseKey(hSubKey);
		RegCloseKey(hRootKey);
	}
	else
	{
		HKEY hRootKey;
		HKEY hSubKey;
		LPTSTR pszFiles;
		LPTSTR psz;
		DWORD dwSize = 0;
		int i;
		HWND hwndList = GetDlgItem(hwnd, IDC_LIST_FILES);
		int nCount = ListBox_GetCount(hwndList);

		RegCreateKey(REG_ROOT, REG_FILEREMOVER, &hRootKey);
		RegCreateKey(hRootKey, _T("General Settings"), &hSubKey);

		Registry_SetDW(hSubKey, _T("Start Hidden"), s_bStartHidden);
		RegCloseKey(hSubKey);

		for (i = 0; i < nCount; i++)
			dwSize += ListBox_GetTextLen(hwndList, i);

		psz = pszFiles = Mem_AllocStr(dwSize + nCount * sizeof(TCHAR));

		for (i = 0; i < nCount; i++)
		{
			DWORD dwItem = ListBox_GetTextLen(hwndList, i);

			ListBox_GetText(hwndList, i, psz);

			psz += dwItem + SZ;
		}

		RegCreateKey(hRootKey, _T("Files"), &hSubKey);
		RegSetValueEx(hSubKey, _T("Files To Monitor"), 0, REG_MULTI_SZ, pszFiles, dwSize + nCount * sizeof(TCHAR));

		Mem_Free(pszFiles);
		RegCloseKey(hSubKey);
		RegCloseKey(hRootKey);
	}
}

void FileRemover_FileNotifyCallback(LPFILENOTIFYDATA lpfnd, DWORD dwChange)
{
	if (dwChange & FN_CREATED)
	{
		/* make sure it's created successfully first */
		Sleep(200);

		if (GetFileAttributes(lpfnd->pszFileName) & FILE_ATTRIBUTE_DIRECTORY)
			Path_TraverseAndRemoveDirectory(lpfnd->pszFileName);
		else
			DeleteFile(lpfnd->pszFileName);
	}
}

void Path_TraverseAndRemoveDirectory(LPTSTR pszPath)
{
	TCHAR szSearch[MAX_PATH];
	TCHAR szPath[MAX_PATH];
	WIN32_FIND_DATA	wfd;
	HANDLE hSearch;

	_tcscpy(szPath, pszPath);

	if (szPath[_tcslen(szPath) - 1] != _T('\\'))
		_tcscat(szPath, _T("\\"));

	wsprintf(szSearch, _T("%s*.*"), szPath);

	if ((hSearch = FindFirstFile(szSearch, &wfd)) != INVALID_HANDLE_VALUE)
	{
		do
		{
			TCHAR szNewPath[MAX_PATH];

			wsprintf(szNewPath, _T("%s%s"), szPath, wfd.cFileName);

			if (String_Equal(wfd.cFileName, _T("."), TRUE) ||
				String_Equal(wfd.cFileName, _T(".."), TRUE))
			{
				continue;
			}
			else if (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				Path_TraverseAndRemoveDirectory(szNewPath);
				RemoveDirectory(szNewPath);
			}
			else
			{
				DeleteFile(szNewPath);
			}
		}
		while (FindNextFile(hSearch, &wfd));
	}

	FindClose(hSearch);

	RemoveDirectory(pszPath);
}
