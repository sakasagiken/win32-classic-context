#include <climits>
#include <sstream>
#include <string>
#include <vector>

#include <Strsafe.h>
#include <Windows.h>

typedef std::basic_string<WCHAR> WSTRING;
typedef std::basic_stringstream<WCHAR> WSTREAM;

inline static const BOOL IS_JPN() {
    static const BOOL VALUE = (GetUserDefaultUILanguage() == 1041);
    return VALUE;
}

inline static const WSTRING ERROR_STRING(const WCHAR *ERROR_MESSAGE, const DWORD ERROR_CODE) {
    WSTREAM message_stream;
    const WSTRING MESSAGE = (ERROR_MESSAGE == NULL) ? L"An unknown error has occurred." : ERROR_MESSAGE;
    message_stream << MESSAGE << L"\nSystem error code is " << ERROR_CODE << L".";
    return message_stream.str();
}

static const struct {
    const WCHAR *title = L"Classic Context for Windows 11 (Up to 23H2)";
    int client_width = 420;
    int client_height = 160;
    COLORREF bg_color = RGB(232, 232, 232);
} WINDOW;

static const struct {
    HKEY root_key = HKEY_CURRENT_USER;
    const WCHAR *sub_key = L"Software\\Classes\\CLSID\\{86ca1aa0-34aa-4e8b-a509-50c905bae2a2}";
    const WCHAR *ex_sub_key = L"\\InprocServer32";
} REGISTRY;

LRESULT CALLBACK MainWindowProcW(
    HWND window_handle, UINT message, WPARAM word_parameter, LPARAM long_parameter);
void reflesh_classic_context_checkbox(HWND checkbox_handle);
void modifi_classic_context();
void revert_modern_context();

int APIENTRY wWinMain(
    _In_ HINSTANCE instance_handle, _In_opt_ HINSTANCE preview_instance_handle,
    _In_ WCHAR *command_line, _In_ int command_show_number
) {
    if (CHAR_BIT * sizeof(int *) != 64) {
        const WCHAR *ERROR_MESSAGE = L"Running on non-64bit. Must be compiled on x64.";
        MessageBoxW(NULL, ERROR_MESSAGE, L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    const WCHAR *WINDOW_CLASS_NAME = L"MainWindow";
    const WCHAR *ICON_NAME_ID = L"DefaultAppIcon";
    const UINT WINDOW_CLASS_STYLE = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;

    WNDCLASS window_class = {};
    window_class.lpfnWndProc = MainWindowProcW;
    window_class.lpszClassName = WINDOW_CLASS_NAME;
    window_class.style = WINDOW_CLASS_STYLE;
    window_class.hInstance = instance_handle;
    window_class.hbrBackground = CreateSolidBrush(WINDOW.bg_color);
    window_class.hIcon = LoadIconW(instance_handle, ICON_NAME_ID);
    window_class.hCursor = LoadCursorW(NULL, IDC_ARROW);
    window_class.lpszMenuName = NULL;
    window_class.cbClsExtra = 0;
    window_class.cbWndExtra = 0;

    if (RegisterClassW(&window_class) == 0) {
        const WCHAR *ERROR_MESSAGE = L"Failed to execute RegisterClassW().";
        MessageBoxW(NULL, ERROR_MESSAGE, L"Error", MB_OK | MB_ICONERROR);
        return 1;
    } else if (window_class.hIcon == NULL) {
        const WCHAR *WARNING_MESSAGE =
            L"The icon specified in the resources file is invalid. Windows default icon is used.";
        MessageBoxW(NULL, WARNING_MESSAGE, L"Warning", MB_OK | MB_ICONWARNING);
    }

    const DWORD WINDOW_STYLE = WS_CAPTION | WS_OVERLAPPED | WS_SYSMENU;
    const DWORD EXTENDED_WINDOW_STYLE = WS_EX_APPWINDOW;
    const BOOL IS_EXISTS_MENU = FALSE;

    RECT window_rectangle = {};
    window_rectangle.left = 0;
    window_rectangle.right = WINDOW.client_width;
    window_rectangle.top = 0;
    window_rectangle.bottom = WINDOW.client_height;
    if (!AdjustWindowRectEx(&window_rectangle, WINDOW_STYLE, IS_EXISTS_MENU, EXTENDED_WINDOW_STYLE)) {
        const WCHAR *ERROR_MESSAGE = L"Failed to execute AdjustWindowRectEx().";
        MessageBoxW(NULL, ERROR_MESSAGE, L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    const int ADJUSTED_WIDTH = window_rectangle.right - window_rectangle.left;
    const int ADJUSTED_HEIGHT = window_rectangle.bottom - window_rectangle.top;
    const int DESKTOP_WIDTH = GetSystemMetrics(SM_CXSCREEN);
    const int DESKTOP_HEIGHT = GetSystemMetrics(SM_CYSCREEN);
    HWND window_handle = CreateWindowExW(
        EXTENDED_WINDOW_STYLE, WINDOW_CLASS_NAME, WINDOW.title, WINDOW_STYLE,
        (DESKTOP_WIDTH - ADJUSTED_WIDTH) / 2, (DESKTOP_HEIGHT - ADJUSTED_HEIGHT) / 2,
        ADJUSTED_WIDTH, ADJUSTED_HEIGHT, NULL, NULL, instance_handle, NULL);
    if (window_handle == NULL) {
        const WCHAR *ERROR_MESSAGE = L"Failed to execute CreateWindowExW().";
        MessageBoxW(NULL, ERROR_MESSAGE, L"Error", MB_OK | MB_ICONERROR);
        return 1;
    } else {
        ShowWindow(window_handle, SW_SHOW);
        UpdateWindow(window_handle);
    }

    MSG message = {};
    while (GetMessageW(&message, NULL, 0, 0)) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return static_cast<int>(message.wParam);
}

LRESULT CALLBACK MainWindowProcW(
    HWND window_handle, UINT message, WPARAM word_parameter, LPARAM long_parameter
) {
    static const HINSTANCE INSTANCE_HANDLE = static_cast<HINSTANCE>(GetModuleHandleW(NULL));

    static struct {
        RECT window = {};
        RECT client = {};
    } rectangle;

    static struct {
        const HFONT FONT = CreateFontW(
            (!IS_JPN()) ? 17 : 21, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE,
            (!IS_JPN()) ? ANSI_CHARSET : SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, VARIABLE_PITCH | FF_DONTCARE, (!IS_JPN()) ? L"Arial" : L"メイリオ");
        HWND checkbox = NULL;
        HWND button = NULL;
        HWND static_text = NULL;
    } handle;

    static const struct {
        HMENU checkbox = reinterpret_cast<HMENU>(10);
        HMENU button = reinterpret_cast<HMENU>(20);
    } MENU_ID;

    try {
        if (message == WM_CREATE) {
            if (handle.FONT == NULL) {
                throw ERROR_STRING(L"Failed to create default font.", GetLastError());
            }
            struct {
                int x = 0, y = 0, w = 0, h = 0;
            } form;
            GetWindowRect(window_handle, &rectangle.window);
            GetClientRect(window_handle, &rectangle.client);

            const int FORM_MARGIN = 16;
            const int CHILD_WINDOW_HEIGHT = 24;
            const int CHILD_WINDOW_WIDTH = (rectangle.client.right - rectangle.client.left) - FORM_MARGIN * 2;
            const int CHECKBOX_VERTICAL_OFFSET = 2;
            const int CHECKBOX_HORIZONTAL_OFFSET = 4;

            const DWORD CHECKBOX_STYLE = WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BS_LEFT | BS_VCENTER | BS_MULTILINE;
            const WCHAR *CHECKBOX_TEXT = (!IS_JPN()) ?
                L"Use context menu up to Windows 10" : L"Windows 10までのコンテキストメニューを使用する";
            form.x = FORM_MARGIN;
            form.y = FORM_MARGIN;
            form.w = CHILD_WINDOW_WIDTH;
            form.h = CHILD_WINDOW_HEIGHT;
            handle.checkbox = CreateWindowExW(
                NULL, L"BUTTON", CHECKBOX_TEXT, CHECKBOX_STYLE,
                form.x + CHECKBOX_HORIZONTAL_OFFSET, form.y - CHECKBOX_VERTICAL_OFFSET,
                form.w - CHECKBOX_HORIZONTAL_OFFSET * 2, form.h, window_handle, MENU_ID.checkbox, INSTANCE_HANDLE, NULL);
            SendMessageW(handle.checkbox, WM_SETFONT, reinterpret_cast<WPARAM>(handle.FONT), true);

            const DWORD STATIC_TEXT_STYLE = WS_CHILD | WS_VISIBLE | SS_LEFT;
            const WCHAR *STATIC_TEXT_1 = (!IS_JPN()) ?
                L"Note: In Windows 11 version 22H2 or later," : L"補足：バージョンが22H2以降のWindows 11では、";
            form.y += CHILD_WINDOW_HEIGHT + FORM_MARGIN / 2;
            handle.static_text = CreateWindowExW(
                NULL, L"STATIC", STATIC_TEXT_1, STATIC_TEXT_STYLE,
                form.x, form.y, form.w, form.h, window_handle, NULL, INSTANCE_HANDLE, NULL);
            SendMessageW(handle.static_text, WM_SETFONT, reinterpret_cast<WPARAM>(handle.FONT), true);
            const WCHAR *STATIC_TEXT_2 = (!IS_JPN()) ?
                L"\"Right-click while holding down the Shift key\"" : L"「Shiftキーを押しながら右クリック」で、";
            form.y += CHILD_WINDOW_HEIGHT - 4;
            handle.static_text = CreateWindowExW(
                NULL, L"STATIC", STATIC_TEXT_2, STATIC_TEXT_STYLE,
                form.x, form.y, form.w, form.h, window_handle, NULL, INSTANCE_HANDLE, NULL);
            SendMessageW(handle.static_text, WM_SETFONT, reinterpret_cast<WPARAM>(handle.FONT), true);
            const WCHAR *STATIC_TEXT_3 = (!IS_JPN()) ?
                L"the context menu up to Windows 10 is displayed." : L"Windows 10までのコンテキストメニューが表示されます。";
            form.y += CHILD_WINDOW_HEIGHT - 4;
            handle.static_text = CreateWindowExW(
                NULL, L"STATIC", STATIC_TEXT_3, STATIC_TEXT_STYLE,
                form.x, form.y, form.w, form.h, window_handle, NULL, INSTANCE_HANDLE, NULL);
            SendMessageW(handle.static_text, WM_SETFONT, reinterpret_cast<WPARAM>(handle.FONT), true);

            const DWORD BUTTON_STYLE = WS_CHILD | WS_VISIBLE | BS_CENTER | BS_VCENTER | BS_MULTILINE;
            const WCHAR *BUTTON_TEXT = (!IS_JPN()) ? L"Apply" : L"適用";
            form.x += CHILD_WINDOW_WIDTH * 3 / 4;
            form.y += CHILD_WINDOW_HEIGHT + FORM_MARGIN / 2;
            form.w -= CHILD_WINDOW_WIDTH * 3 / 4;
            handle.button = CreateWindowExW(
                NULL, L"BUTTON", BUTTON_TEXT, BUTTON_STYLE,
                form.x, form.y, form.w, form.h, window_handle, MENU_ID.button, INSTANCE_HANDLE, NULL);
            SendMessageW(handle.button, WM_SETFONT, reinterpret_cast<WPARAM>(handle.FONT), true);

            reflesh_classic_context_checkbox(handle.checkbox);
            return static_cast<LRESULT>(0);
        }
        if (message == WM_CTLCOLORBTN || message == WM_CTLCOLORSTATIC) {
            SetBkColor(reinterpret_cast<HDC>(word_parameter), WINDOW.bg_color);
            return reinterpret_cast<LRESULT>(CreateSolidBrush(WINDOW.bg_color));
        }
        if (message == WM_COMMAND) {
            SetFocus(NULL);
            const HMENU COMMAND_ID = reinterpret_cast<HMENU>(LOWORD(word_parameter));
            if (COMMAND_ID == MENU_ID.button) {
                if (SendMessageW(handle.checkbox, BM_GETCHECK, 0, 0) == BST_CHECKED) {
                    const WCHAR *MESSAGE_TEXT = (!IS_JPN()) ?
                        L"Change to the context menu up to Windows 10?" :
                        L"Windows 10までのコンテキストメニューに変更しますか？";
                    const WCHAR *MESSAGE_TITLE = (!IS_JPN()) ? L"Confirm" : L"確認";
                    const int SELECTION = MessageBoxW(
                        window_handle, MESSAGE_TEXT, MESSAGE_TITLE, MB_YESNO | MB_ICONQUESTION);
                    if (SELECTION == IDYES) {
                        modifi_classic_context();
                        const WCHAR *MESSAGE_TEXT = (!IS_JPN()) ?
                            L"You need to sign back in, to apply the changes."
                            L"\nDo you want to sign out?"
                            L"\nSelect \"No\" if you want to sign out later." :
                            L"変更を適用するには、再度サインインする必要があります。"
                            L"\nサインアウトしますか？"
                            L"\n後でサインアウトしたい場合は「いいえ」を選択してください。";
                        const WCHAR *MESSAGE_TITLE = (!IS_JPN()) ? L"Confirm" : L"確認";
                        int selection = MessageBoxW(
                            window_handle, MESSAGE_TEXT, MESSAGE_TITLE, MB_YESNO | MB_ICONINFORMATION);
                        if (selection == IDYES) {
                            ExitWindowsEx(
                                EWX_LOGOFF, SHTDN_REASON_MAJOR_OPERATINGSYSTEM | SHTDN_REASON_MINOR_RECONFIG);
                        }
                    }
                } else {
                    const WCHAR *MESSAGE_TEXT = (!IS_JPN()) ?
                        L"Revert to the context menu in Windows 11?" :
                        L"Windows 11のコンテキストメニューに戻しますか？";
                    const WCHAR *MESSAGE_TITLE = (!IS_JPN()) ? L"Confirm" : L"確認";
                    const int SELECTION = MessageBoxW(
                        window_handle, MESSAGE_TEXT, MESSAGE_TITLE, MB_YESNO | MB_ICONQUESTION);
                    if (SELECTION == IDYES) {
                        revert_modern_context();
                        const WCHAR *MESSAGE_TEXT = (!IS_JPN()) ?
                            L"Reverted to the context menu in Windows 11." :
                            L"Windows 11のコンテキストメニューに戻しました。";
                        const WCHAR *MESSAGE_TITLE = (!IS_JPN()) ? L"Success" : L"完了";
                        const int SELECTION = MessageBoxW(
                            window_handle, MESSAGE_TEXT, MESSAGE_TITLE, MB_OK | MB_ICONINFORMATION);
                    }
                }
                reflesh_classic_context_checkbox(handle.checkbox);
            }
            return static_cast<LRESULT>(0);
        }
        if (message == WM_SIZE) {
            const int CLIENT_WIDTH = LOWORD(long_parameter);
            const int CLIENT_HEIGHT = HIWORD(long_parameter);
            const int INITIAL_WIDTH = rectangle.client.right - rectangle.client.left;
            const int INITIAL_HEIGHT = rectangle.client.bottom - rectangle.client.top;
            if (CLIENT_WIDTH != INITIAL_WIDTH || CLIENT_HEIGHT != INITIAL_HEIGHT) {
                const int WINDOW_WIDTH = rectangle.window.right - rectangle.window.left;
                const int WINDOW_HEIGHT = rectangle.window.bottom - rectangle.window.top;
                SetWindowPos(window_handle, HWND_TOP, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, SWP_NOMOVE);
            }
            return static_cast<LRESULT>(0);
        }
        if (message == WM_CLOSE) {
            DestroyWindow(window_handle);
            return static_cast<LRESULT>(0);
        }
        if (message == WM_DESTROY) {
            DeleteObject(static_cast<HGDIOBJ>(handle.FONT));
            PostQuitMessage(0);
            return static_cast<LRESULT>(0);
        }
    } catch (WSTRING &error) {
        MessageBoxW(window_handle, error.c_str(), L"Error", MB_OK | MB_ICONERROR);
        DestroyWindow(window_handle);
        return static_cast<LRESULT>(0);
    } catch (...) {
        MessageBoxW(window_handle, ERROR_STRING(NULL, GetLastError()).c_str(), L"Error", MB_OK | MB_ICONERROR);
        DestroyWindow(window_handle);
        return static_cast<LRESULT>(0);
    }
    return DefWindowProcW(window_handle, message, word_parameter, long_parameter);
}

void reflesh_classic_context_checkbox(HWND checkbox_handle) {
    HKEY registry_handle = {};
    const WSTRING SUB_KEY = WSTRING(REGISTRY.sub_key).append(REGISTRY.ex_sub_key).c_str();
    const LSTATUS OPEN_RESULT = RegOpenKeyExW(REGISTRY.root_key, SUB_KEY.c_str(), 0, KEY_READ, &registry_handle);
    if (OPEN_RESULT == ERROR_FILE_NOT_FOUND) {
        SendMessageW(checkbox_handle, BM_SETCHECK, BST_UNCHECKED, 0);
    } else if (OPEN_RESULT == ERROR_SUCCESS) {
        SendMessageW(checkbox_handle, BM_SETCHECK, BST_CHECKED, 0);
    } else {
        RegCloseKey(registry_handle);
        throw ERROR_STRING(L"Failed to open registry.", OPEN_RESULT);
    }
    RegCloseKey(registry_handle);
    return;
}

void modifi_classic_context() {
    HKEY registry_handle = {};
    const WSTRING SUB_KEY = WSTRING(REGISTRY.sub_key).append(REGISTRY.ex_sub_key);
    const LSTATUS CREATE_RESULT = RegCreateKeyExW(
        REGISTRY.root_key, SUB_KEY.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &registry_handle, NULL);
    if (CREATE_RESULT != ERROR_SUCCESS) {
        RegCloseKey(registry_handle);
        throw ERROR_STRING(L"Failed to open registry.", CREATE_RESULT);
    } else {
        WCHAR *value_name = NULL;
        WCHAR *value_data = NULL;
        const LSTATUS WRITE_RESULT = RegSetValueExW(
            registry_handle, value_name, 0, REG_SZ, reinterpret_cast<BYTE *>(&value_data), 0);
        if (WRITE_RESULT != ERROR_SUCCESS) {
            RegCloseKey(registry_handle);
            throw ERROR_STRING(L"Failed to write registry value.", WRITE_RESULT);
        }
    }
    RegCloseKey(registry_handle);
    return;
}

void revert_modern_context() {
    std::vector<WSTRING> key_list = {
        WSTRING(REGISTRY.sub_key).append(REGISTRY.ex_sub_key), WSTRING(REGISTRY.sub_key)
    };
    for (int i = 0; i < static_cast<int>(key_list.size()); i++) {
        const LSTATUS DELETE_RESULT = RegDeleteKeyW(REGISTRY.root_key, key_list[i].c_str());
        if (DELETE_RESULT == ERROR_FILE_NOT_FOUND) {
            continue;
        } else if (DELETE_RESULT != ERROR_SUCCESS) {
            throw ERROR_STRING(L"Failed to delete registry key.", DELETE_RESULT);
        }
    }
    return;
}
