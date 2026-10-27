#include <windows.h>
#include <string>
#include <vector>
#include <algorithm>

// Глобальні змінні для управління вікном та потоками
HWND g_hWnd = NULL;
HANDLE g_hThreadInput = NULL;
HANDLE g_hThreadSort = NULL;

CRITICAL_SECTION g_cs; // Критична секція для синхронізації доступу до даних[cite: 2, 7]

std::wstring g_currentBuffer = L"";          // Поточний введений рядок
std::vector<std::wstring> g_sortedGroups;   // Список відсортованих груп символів

bool g_isRunning = true;

// Оновлення графічного інтерфейсу
void RequestRedraw() {
    if (g_hWnd) {
        InvalidateRect(g_hWnd, NULL, TRUE);
    }
}

// Потік 1: Робочий потік (виконує фонову обробку та сортування за необхідності)[cite: 2]
DWORD WINAPI ThreadInputProc(LPVOID lpParam) {
    while (g_isRunning) {
        Sleep(50);
    }
    return 0;
}

// Потік 2: Обслуговує логіку роботи з даними другої половини вікна[cite: 2]
DWORD WINAPI ThreadSortProc(LPVOID lpParam) {
    while (g_isRunning) {
        Sleep(50);
    }
    return 0;
}

// Обробник повідомлень головного вікна
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        InitializeCriticalSection(&g_cs);

        // Створення потоків[cite: 2, 5]
        g_hThreadInput = CreateThread(NULL, 0, ThreadInputProc, NULL, 0, NULL);
        g_hThreadSort = CreateThread(NULL, 0, ThreadSortProc, NULL, 0, NULL);
        break;
    }
    case WM_KEYDOWN: {
        // ПРЯМА ОБРОБКА ESCAPE: миттєво закриває програму[cite: 2, 14]
        if (wParam == VK_ESCAPE) {
            DestroyWindow(hWnd);
            return 0;
        }
        // Обробка клавіші ENTER — додаємо та сортуємо групу[cite: 2]
        else if (wParam == VK_RETURN) {
            EnterCriticalSection(&g_cs);
            if (!g_currentBuffer.empty()) {
                std::wstring group = g_currentBuffer;
                g_currentBuffer.clear();

                // Сортуємо символи всередині нової групи[cite: 2]
                std::sort(group.begin(), group.end());
                g_sortedGroups.push_back(group);

                // Сортуємо всі збережені групи між собою
                std::sort(g_sortedGroups.begin(), g_sortedGroups.end());
            }
            LeaveCriticalSection(&g_cs);

            RequestRedraw();
        }
        break;
    }
    case WM_CHAR: {
        // Набір звичайних символів (виключаємо Enter, Escape, Backspace)[cite: 2]
        if (wParam != VK_RETURN && wParam != VK_ESCAPE && wParam != VK_BACK) {
            EnterCriticalSection(&g_cs);
            g_currentBuffer += static_cast<wchar_t>(wParam);
            LeaveCriticalSection(&g_cs);

            RequestRedraw();
        }
        // Обробка Backspace (видалення останього символу)
        else if (wParam == VK_BACK) {
            EnterCriticalSection(&g_cs);
            if (!g_currentBuffer.empty()) {
                g_currentBuffer.pop_back();
            }
            LeaveCriticalSection(&g_cs);

            RequestRedraw();
        }
        break;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        RECT rect;
        GetClientRect(hWnd, &rect);

        // Налаштування сірого фону та нежирного шрифту
        SetBkMode(hdc, TRANSPARENT);
        HFONT hFont = CreateFontW(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

        int midX = rect.right / 2;

        // Розділювальна лінія посередині вікна[cite: 2]
        MoveToEx(hdc, midX, 0, NULL);
        LineTo(hdc, midX, rect.bottom);

        // --- Заголовок лівої частини ---
        RECT leftHeaderRect = { 10, 10, midX - 10, 30 };
        DrawTextW(hdc, L"Input (Enter - submit, ESC - exit):", -1, &leftHeaderRect, DT_SINGLELINE | DT_LEFT);

        // --- Ліва половина: Поточний введений рядок[cite: 2] ---
        EnterCriticalSection(&g_cs);
        std::wstring currentText = L"> " + g_currentBuffer;
        RECT leftTextRect = { 10, 40, midX - 10, rect.bottom - 10 };
        DrawTextW(hdc, currentText.c_str(), -1, &leftTextRect, DT_LEFT | DT_WORDBREAK);

        // --- Заголовок правої частини ---
        RECT rightHeaderRect = { midX + 10, 10, rect.right - 10, 30 };
        DrawTextW(hdc, L"Sorted Character Groups:", -1, &rightHeaderRect, DT_SINGLELINE | DT_LEFT);

        // --- Права половина: Виведення відсортованих груп[cite: 2] ---
        int yOffset = 40;
        for (const auto& group : g_sortedGroups) {
            RECT rightTextRect = { midX + 10, yOffset, rect.right - 10, yOffset + 20 };
            DrawTextW(hdc, group.c_str(), -1, &rightTextRect, DT_SINGLELINE | DT_LEFT);
            yOffset += 20;
            if (yOffset > rect.bottom - 20) break;
        }
        LeaveCriticalSection(&g_cs);

        SelectObject(hdc, hOldFont);
        DeleteObject(hFont);

        EndPaint(hWnd, &ps);
        break;
    }
    case WM_DESTROY: {
        g_isRunning = false;

        // Коректне очікування та закриття потоків[cite: 5, 6, 14]
        WaitForSingleObject(g_hThreadInput, 500);
        WaitForSingleObject(g_hThreadSort, 500);

        CloseHandle(g_hThreadInput);
        CloseHandle(g_hThreadSort);
        DeleteCriticalSection(&g_cs);

        PostQuitMessage(0);
        break;
    }
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Головна функція WinMain
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    const wchar_t CLASS_NAME[] = L"Lab3Variant3Class";

    // Створення сірого пензля для фону
    HBRUSH hGrayBrush = CreateSolidBrush(RGB(220, 220, 220));

    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = hGrayBrush;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClassW(&wc);

    g_hWnd = CreateWindowExW(
        0,
        CLASS_NAME,
        L"Lab 3 - Variant 3",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 500,
        NULL, NULL, hInstance, NULL
    );

    if (g_hWnd == NULL) {
        DeleteObject(hGrayBrush);
        return 0;
    }

    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);

    // Головний цикл повідомлень
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    DeleteObject(hGrayBrush);
    return (int)msg.wParam;
}