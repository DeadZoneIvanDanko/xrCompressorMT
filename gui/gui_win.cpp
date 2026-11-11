// xrCompressorMT - single exe: native Win32 window (no arguments) + console packer (with arguments).
// The window runs the packer by starting this same exe with command-line arguments.
// datapack.ltx next to the exe is used automatically when packing.
#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shlobj.h>
#include <shellapi.h>
#include <algorithm>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

int packer_main(int argc, char** argv);   // src/main.cpp built with -DXR_EMBED

// ------------------------------------------------------------------ strings
enum Sid {
    S_TITLE, S_TAB_PACK, S_TAB_UNPACK, S_ABOUT, S_BROWSE, S_GRP_PATHS, S_SRC, S_OUT, S_NAME, S_GRP_PARAMS,
    S_XDB, S_BEST, S_FAST, S_STORE, S_VERIFY, S_MAXSIZE, S_THREADS, S_MEM, S_PACK, S_STOP, S_CHECK, S_OPEN,
    S_READY, S_WORKING, S_DLG_CHECK, S_WARN_SRC, S_WARN_OUT, S_WARN_NAME, S_NO_PACKER, S_GRP_ARCH, S_ADD,
    S_ADDDIR, S_REMOVE, S_CLEAR, S_UNPACK, S_DLG_DIR, S_NO_ARCH_DIR, S_WARN_ADD, S_WARN_MISSING, S_ARCHIVE_N,
    S_PROGRESS, S_DONE_OK, S_ERR, S_STOPPED, S_LOG_STOPPED_USER, S_LOG_DONE, S_LOG_ERROR, S_LOG_STOPPEDL, S_LOG_BEGIN,
    S_LOG_END, S_LOG_SAVED, S_LOG_FAIL, S_ABOUT_TITLE, S_ABOUT_SUB, S_ABOUT_TEXT, S_ABOUT_GAMES, S_ABOUT_X64,
    S_ABOUT_MT, S_ABOUT_VERIFY, S_AUTHOR, S_CHANNEL, S_FILTER, S_DLG_UNPACK, S_COUNT
};
static const wchar_t* TBL[S_COUNT][2] = {
    {L"xrCompressorMT — пакувальник .xdb", L"xrCompressorMT — .xdb packer"},
    {L"Пакування ресурсів", L"Pack resources"},
    {L"Розпакування ресурсів", L"Unpack resources"},
    {L"Опис програми", L"About"},
    {L"Огляд…", L"Browse…"},
    {L"Шляхи", L"Paths"},
    {L"Папка з ресурсами:", L"Resources folder:"},
    {L"Папка виводу:", L"Output folder:"},
    {L"Ім'я архіву:", L"Archive name:"},
    {L"Параметри", L"Options"},
    {L"Формат .xdb (інакше .db)", L".xdb format (otherwise .db)"},
    {L"Максимальне стиснення (lzo1x_999)", L"Maximum compression (lzo1x_999)"},
    {L"Швидке стиснення (-fast)", L"Fast compression (-fast)"},
    {L"Без стиснення (-store)", L"No compression (-store)"},
    {L"Перевіряти архіви після запису (рекомендовано)", L"Verify archives after writing (recommended)"},
    {L"Макс. розмір архіву, МБ:", L"Max archive size, MB:"},
    {L"Потоків (0 = авто):", L"Threads (0 = auto):"},
    {L"Пам'ять під файли, МБ:", L"Memory for files, MB:"},
    {L"Запакувати", L"Pack"},
    {L"Стоп", L"Stop"},
    {L"Перевірити архіви…", L"Verify archives…"},
    {L"Відкрити папку виводу", L"Open output folder"},
    {L"Готово до роботи", L"Ready"},
    {L"Виконується…", L"Working…"},
    {L"Архіви для перевірки", L"Archives to verify"},
    {L"Вкажи папку з ресурсами.", L"Specify the resources folder."},
    {L"Вкажи папку виводу.", L"Specify the output folder."},
    {L"Вкажи ім'я архіву.", L"Specify the archive name."},
    {L"Не знайдено пакувальник:\n%s\n\nПоклади його поруч із цією програмою.", L"Packer not found:\n%s\n\nPut it next to this program."},
    {L"Архіви для розпакування", L"Archives to unpack"},
    {L"Додати архіви…", L"Add archives…"},
    {L"Додати всі з папки (з підпапками)…", L"Add all from folder (with subfolders)…"},
    {L"Прибрати вибрані", L"Remove selected"},
    {L"Очистити", L"Clear"},
    {L"Розпакувати", L"Unpack"},
    {L"Папка з архівами", L"Archives folder"},
    {L"У цій папці та її підпапках немає архівів .xdb / .db.", L"There are no .xdb / .db archives in this folder or its subfolders."},
    {L"Додай хоча б один архів.", L"Add at least one archive."},
    {L"Не знайдено файли:\n", L"Files not found:\n"},
    {L"Архів %d з %d…", L"Archive %d of %d…"},
    {L"%d/%d файлів, %s з %s МБ, %s МБ/с", L"%d/%d files, %s of %s MB, %s MB/s"},
    {L"Готово: помилок немає", L"Done: no errors"},
    {L"ПОМИЛКА (невдалих запусків: %d) — див. журнал", L"ERROR (failed runs: %d) — see log"},
    {L"Зупинено користувачем", L"Stopped by user"},
    {L"! зупинено користувачем (недописаний результат використовувати не можна)", L"! stopped by user (the incomplete result must not be used)"},
    {L"=== ГОТОВО ===", L"=== DONE ==="},
    {L"=== ПОМИЛКА, невдалих запусків: %d ===", L"=== ERROR, failed runs: %d ==="},
    {L"=== ЗУПИНЕНО ===", L"=== STOPPED ==="},
    {L"=== Початок: %s ===", L"=== Started: %s ==="},
    {L"=== Кінець: %s ===", L"=== Finished: %s ==="},
    {L"Лог збережено: %s", L"Log saved: %s"},
    {L"! не вдалося створити лог-файл", L"! could not create the log file"},
    {L"Опис програми", L"About"},
    {L"Багатопотоковий пакувальник і розпакувальник ігрових ресурсів S.T.A.L.K.E.R.", L"Multithreaded packer and unpacker for S.T.A.L.K.E.R. game resources"},
    {L"Швидко створює та розпаковує архіви ресурсів .xdb / .db з перевіркою цілісності.", L"Quickly creates and unpacks .xdb / .db resource archives with integrity checking."},
    {L"Сумісність: COC, COP, Monolith, OpenXRay, OGSR (.xdb у папці мода), SOC", L"Compatibility: COC, COP, Monolith, OpenXRay, OGSR (.xdb in the mod folder), SOC"},
    {L"Підтримка 64 біт", L"64-bit support"},
    {L"Багатопотоковість — стиснення на всіх ядрах процесора", L"Multithreading — compression on all CPU cores"},
    {L"Перевірка архівів після запису", L"Archive verification after writing"},
    {L"Автор", L"Author"},
    {L"Telegram-канал", L"Telegram channel"},
    {L"Архіви", L"Archives"},
    {L"Архіви для розпакування", L"Archives to unpack"},
};
static int LANG = 0;
static const wchar_t* T(Sid s) { return TBL[s][LANG]; }
static std::wstring Fmt(Sid s, ...)
{
    wchar_t buf[1024];
    va_list ap;
    va_start(ap, s);
    _vsnwprintf(buf, 1024, T(s), ap);
    va_end(ap);
    return buf;
}

// ------------------------------------------------------------------ state
enum {
    ID_TAB = 100, ID_LANG_EN, ID_LANG_UA, ID_ABOUT,
    // pack page
    ID_P_SRC = 200, ID_P_OUT, ID_P_NAME, ID_P_BSRC, ID_P_BOUT, ID_P_XDB, ID_P_BEST, ID_P_FAST, ID_P_STORE, ID_P_VERIFY,
    ID_P_MAX, ID_P_MAXUD, ID_P_THR, ID_P_THRUD, ID_P_MEM, ID_P_MEMUD, ID_P_START, ID_P_STOP, ID_P_CHECK, ID_P_OPEN,
    // unpack page
    ID_U_LIST = 300, ID_U_ADD, ID_U_ADDDIR, ID_U_RM, ID_U_CLR, ID_U_OUT, ID_U_BOUT, ID_U_START, ID_U_STOP, ID_U_OPEN
};
static const UINT WM_LINE = WM_APP + 1, WM_DONE = WM_APP + 2;
static const COLORREF ACCENT = RGB(0x2d, 0x8f, 0xd5);

struct Page
{
    HWND hwnd = nullptr, bar = nullptr, status = nullptr, log = nullptr, start = nullptr, stop = nullptr, open = nullptr;
    std::function<std::wstring()> statusFn;
    std::vector<std::wstring> jobs;
    int jobTotal = 0, jobIndex = 0, failed = 0;
    bool stopped = false, running = false;
    HANDLE hProc = nullptr;
    std::wstring logPath;
    std::vector<std::wstring> logLines;
    SYSTEMTIME started{};
};
static Page pk, up;
static Page* pages[2] = {&pk, &up};

struct TrItem { HWND h; Sid s; };
static std::vector<TrItem> trItems;

static HWND hMain, hTab, hBtnAbout, hBtnEn, hBtnUa, hAboutWnd;
static HWND hSrc, hOut, hName, hXdb, hBest, hFast, hStore, hVerify, hMax, hThr, hMem, hUOut, hList;
static HWND hUd1, hUd2, hUd3;
static HWND hLbMax, hLbThr, hLbMem, hLbSrc, hLbOut, hLbName, hBSrc, hBOut, hBCheck;
static int hGrpPaths, hGrpParams, hUGrpList, hUGrpPaths;   // group frames are painted by the page, not separate windows
static HWND hULbOut, hUBOut, hUAdd, hUAddDir, hURm, hUClr;
static std::wstring exeDir, iniPath, selfPath;
static HFONT uiFont, monoFont, bigFont;
static HICON hIconSmall, hIconBig;
static HINSTANCE hInst;
static int dpi = 96;
static int S(int v) { return MulDiv(v, dpi, 96); }

static std::wstring GetText(HWND h, bool trimq = true)
{
    int n = GetWindowTextLengthW(h);
    std::wstring s(n + 1, 0);
    GetWindowTextW(h, &s[0], n + 1);
    s.resize(n);
    if (!trimq) return s;
    size_t a = s.find_first_not_of(L" \t\""), b = s.find_last_not_of(L" \t\"");
    return a == std::wstring::npos ? L"" : s.substr(a, b - a + 1);
}
static std::string U(const std::wstring& w)
{
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}
static std::wstring W(const std::string& s)
{
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], n);
    return w;
}
static std::wstring Base(std::wstring p)
{
    while (!p.empty() && (p.back() == L'\\' || p.back() == L'/')) p.pop_back();
    size_t i = p.find_last_of(L"\\/");
    std::wstring b = (i == std::wstring::npos) ? p : p.substr(i + 1);
    if (b.size() == 2 && b[1] == L':') b.clear();
    return b;
}
static std::wstring Arg(std::wstring p)
{
    if (p.size() > 3 && (p.back() == L'\\' || p.back() == L'/')) p.pop_back();
    else if (p.size() == 3 && p[1] == L':' && p[2] == L'\\') p += L".";
    return L"\"" + p + L"\"";
}
static int GetInt(HWND h, int def)
{
    std::wstring s = GetText(h);
    return s.empty() ? def : _wtoi(s.c_str());
}
static void SetInt(HWND h, int v) { SetWindowTextW(h, std::to_wstring(v).c_str()); }

// ------------------------------------------------------------------ control helpers
static HWND Mk(HWND parent, const wchar_t* cls, const wchar_t* txt, DWORD style, int id, DWORD ex = 0)
{
    HWND h = CreateWindowExW(ex, cls, txt, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | style, 0, 0, 10, 10, parent, (HMENU)(INT_PTR)id, nullptr, nullptr);
    SendMessageW(h, WM_SETFONT, (WPARAM)uiFont, TRUE);
    return h;
}
static HWND MkT(HWND parent, const wchar_t* cls, Sid s, DWORD style, int id, DWORD ex = 0)
{
    HWND h = Mk(parent, cls, T(s), style, id, ex);
    trItems.push_back({h, s});
    return h;
}
struct Grp { HWND page; Sid sid; RECT r; };
static std::vector<Grp> groups;
static int MkGroup(HWND parent, Sid sid)
{
    groups.push_back({parent, sid, {0, 0, 0, 0}});
    return (int)groups.size() - 1;
}
static void PlaceGrp(int g, int x, int y, int w, int hgt)
{
    groups[g].r = {x, y, x + w, y + hgt};
    InvalidateRect(groups[g].page, nullptr, TRUE);
}
static HWND MkSpin(HWND parent, int editId, int udId, int lo, int hi, int val, HWND& ud)
{
    HWND e = Mk(parent, L"EDIT", L"", WS_TABSTOP | ES_NUMBER | ES_AUTOHSCROLL, editId, WS_EX_CLIENTEDGE);
    ud = CreateWindowExW(0, UPDOWN_CLASSW, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | UDS_SETBUDDYINT | UDS_ALIGNRIGHT | UDS_ARROWKEYS | UDS_NOTHOUSANDS,
                         0, 0, 0, 0, parent, (HMENU)(INT_PTR)udId, nullptr, nullptr);
    SendMessageW(ud, UDM_SETBUDDY, (WPARAM)e, 0);
    SendMessageW(ud, UDM_SETRANGE32, lo, hi);
    SendMessageW(ud, UDM_SETPOS32, 0, val);
    return e;
}
static void Place(HWND h, int x, int y, int w, int hgt) { MoveWindow(h, x, y, w, hgt, TRUE); }

// ------------------------------------------------------------------ logging / status
static void SetStatus(Page& p, std::function<std::wstring()> fn)
{
    p.statusFn = fn;
    SetWindowTextW(p.status, fn().c_str());
}
static void SetStatusKey(Page& p, Sid s)
{
    SetStatus(p, [s] { return std::wstring(T(s)); });
}
static void AppendLog(Page& p, const std::wstring& line)
{
    p.logLines.push_back(line);
    std::wstring t = line + L"\r\n";
    int len = GetWindowTextLengthW(p.log);
    SendMessageW(p.log, EM_SETSEL, len, len);
    SendMessageW(p.log, EM_REPLACESEL, FALSE, (LPARAM)t.c_str());
}
static std::wstring Now()
{
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t b[64];
    swprintf(b, 64, L"%04d-%02d-%02d %02d:%02d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return b;
}

// ------------------------------------------------------------------ settings
static void SaveSettings()
{
    auto w = [](const wchar_t* k, const std::wstring& v) { WritePrivateProfileStringW(L"gui", k, v.c_str(), iniPath.c_str()); };
    w(L"src", GetText(hSrc));
    w(L"out", GetText(hOut));
    w(L"name", GetText(hName));
    w(L"maxsize", std::to_wstring(GetInt(hMax, 1900)));
    w(L"threads", std::to_wstring(GetInt(hThr, 0)));
    w(L"mem", std::to_wstring(GetInt(hMem, 1024)));
    w(L"xdb", SendMessageW(hXdb, BM_GETCHECK, 0, 0) == BST_CHECKED ? L"1" : L"0");
    w(L"verify", SendMessageW(hVerify, BM_GETCHECK, 0, 0) == BST_CHECKED ? L"1" : L"0");
    w(L"mode", SendMessageW(hFast, BM_GETCHECK, 0, 0) == BST_CHECKED ? L"fast" : SendMessageW(hStore, BM_GETCHECK, 0, 0) == BST_CHECKED ? L"store" : L"best");
    w(L"unpack_out", GetText(hUOut));
    w(L"lang", LANG ? L"en" : L"ua");
}

// ------------------------------------------------------------------ process runner
struct ReaderCtx { HANDLE pipe, proc; int page; };
static DWORD WINAPI Reader(LPVOID pv)
{
    ReaderCtx* c = (ReaderCtx*)pv;
    std::string buf;
    char tmp[4096];
    DWORD n;
    auto flush = [&](bool all) {
        for (;;)
        {
            size_t i = buf.find_first_of("\r\n");
            if (i == std::string::npos) break;
            std::string line = buf.substr(0, i);
            buf.erase(0, i + 1);
            if (!line.empty()) PostMessageW(hMain, WM_LINE, c->page, (LPARAM) new std::string(line));
        }
        if (all && !buf.empty()) PostMessageW(hMain, WM_LINE, c->page, (LPARAM) new std::string(buf));
    };
    while (ReadFile(c->pipe, tmp, sizeof tmp, &n, nullptr) && n)
    {
        buf.append(tmp, n);
        flush(false);
    }
    flush(true);
    WaitForSingleObject(c->proc, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(c->proc, &code);
    CloseHandle(c->pipe);
    CloseHandle(c->proc);
    int pg = c->page;
    delete c;
    PostMessageW(hMain, WM_DONE, pg, (LPARAM)code);
    return 0;
}

static std::wstring ExePath() { return selfPath; }

static void SetRunning(Page& p, bool on)
{
    p.running = on;
    EnableWindow(p.start, !on);
    EnableWindow(p.stop, on);
    if (&p == &pk) EnableWindow(hBCheck, !on);
    if (&p == &up) for (HWND b : {hUAdd, hUAddDir, hURm, hUClr}) EnableWindow(b, !on);
}

static void NextJob(Page& p, int idx);
static void Finish(Page& p, int idx)
{
    p.hProc = nullptr;
    SetRunning(p, false);
    if (p.stopped)
    {
        SetStatusKey(p, S_STOPPED);
        AppendLog(p, T(S_LOG_STOPPEDL));
    }
    else if (p.failed == 0)
    {
        SendMessageW(p.bar, PBM_SETPOS, 100, 0);
        SetStatusKey(p, S_DONE_OK);
        AppendLog(p, T(S_LOG_DONE));
    }
    else
    {
        int n = p.failed;
        SetStatus(p, [n] { return Fmt(S_ERR, n); });
        AppendLog(p, Fmt(S_LOG_ERROR, n));
    }
    if (!p.logPath.empty())
    {
        FILE* f = _wfopen(p.logPath.c_str(), L"wb");
        if (f)
        {
            auto wr = [&](const std::wstring& s) { std::string u = U(s) + "\r\n"; fwrite(u.data(), 1, u.size(), f); };
            wr(Fmt(S_LOG_BEGIN, Now().c_str()));
            for (auto& l : p.logLines) wr(l);
            wr(Fmt(S_LOG_END, Now().c_str()));
            fclose(f);
            AppendLog(p, Fmt(S_LOG_SAVED, p.logPath.c_str()));
        }
        else AppendLog(p, T(S_LOG_FAIL));
    }
    (void)idx;
}

static void NextJob(Page& p, int idx)
{
    if (p.jobs.empty()) { Finish(p, idx); return; }
    std::wstring args = p.jobs.front();
    p.jobs.erase(p.jobs.begin());
    ++p.jobIndex;
    if (p.jobTotal > 1)
    {
        int i = p.jobIndex, n = p.jobTotal;
        SetStatus(p, [i, n] { return Fmt(S_ARCHIVE_N, i, n); });
    }
    std::wstring cmd = L"\"" + ExePath() + L"\" " + args;
    AppendLog(p, L"> " + cmd);

    SECURITY_ATTRIBUTES sa{sizeof sa, nullptr, TRUE};
    HANDLE rd, wr;
    if (!CreatePipe(&rd, &wr, &sa, 0)) { ++p.failed; NextJob(p, idx); return; }
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
    HANDLE nul = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);
    STARTUPINFOW si{};
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = nul;
    si.hStdOutput = wr;
    si.hStdError = wr;
    PROCESS_INFORMATION pi{};
    std::vector<wchar_t> cl(cmd.begin(), cmd.end());
    cl.push_back(0);
    BOOL ok = CreateProcessW(nullptr, cl.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, exeDir.c_str(), &si, &pi);
    CloseHandle(wr);
    if (nul != INVALID_HANDLE_VALUE) CloseHandle(nul);
    if (!ok)
    {
        CloseHandle(rd);
        AppendLog(p, L"! CreateProcess failed");
        ++p.failed;
        NextJob(p, idx);
        return;
    }
    CloseHandle(pi.hThread);
    DuplicateHandle(GetCurrentProcess(), pi.hProcess, GetCurrentProcess(), &p.hProc, 0, FALSE, DUPLICATE_SAME_ACCESS);
    HANDLE th = CreateThread(nullptr, 0, Reader, new ReaderCtx{rd, pi.hProcess, idx}, 0, nullptr);
    if (th) CloseHandle(th);
}

static void RunJobs(int idx, const std::vector<std::wstring>& jobs, const std::wstring& logPath)
{
    Page& p = *pages[idx];
    p.jobs = jobs;
    p.jobTotal = (int)jobs.size();
    p.jobIndex = 0;
    p.failed = 0;
    p.stopped = false;
    p.logPath = logPath;
    p.logLines.clear();
    SetWindowTextW(p.log, L"");
    SendMessageW(p.bar, PBM_SETPOS, 0, 0);
    SetStatusKey(p, S_WORKING);
    SetRunning(p, true);
    NextJob(p, idx);
}

static void StopRun(int idx)
{
    Page& p = *pages[idx];
    p.stopped = true;
    p.jobs.clear();
    if (p.running && p.hProc)
    {
        AppendLog(p, T(S_LOG_STOPPED_USER));
        TerminateProcess(p.hProc, 1);
    }
}

// ------------------------------------------------------------------ dialogs
static int CALLBACK BrowseCb(HWND h, UINT m, LPARAM, LPARAM data)
{
    if (m == BFFM_INITIALIZED && data) SendMessageW(h, BFFM_SETSELECTIONW, TRUE, data);
    return 0;
}
static std::wstring BrowseDir(const std::wstring& cur, const wchar_t* title)
{
    BROWSEINFOW bi{};
    bi.hwndOwner = hMain;
    bi.lpszTitle = title;
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    bi.lpfn = BrowseCb;
    bi.lParam = cur.empty() ? 0 : (LPARAM)cur.c_str();
    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
    if (!pidl) return L"";
    wchar_t path[MAX_PATH * 2];
    std::wstring r;
    if (SHGetPathFromIDListW(pidl, path)) r = path;
    CoTaskMemFree(pidl);
    return r;
}
static void BrowseInto(HWND edit, const wchar_t* title)
{
    std::wstring r = BrowseDir(GetText(edit), title);
    if (!r.empty()) SetWindowTextW(edit, r.c_str());
}
static std::vector<std::wstring> PickArchives(const std::wstring& startDir, Sid titleId)
{
    std::vector<wchar_t> buf(65536, 0);
    std::wstring filter;
    auto add = [&](const std::wstring& a) { filter += a; filter.push_back(0); };
    add(std::wstring(T(S_FILTER)) + L" (*.xdb*, *.db*)");
    add(L"*.xdb*;*.db*");
    add(LANG ? L"All files (*.*)" : L"Усі файли (*.*)");
    add(L"*.*");
    filter.push_back(0);
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof ofn;
    ofn.hwndOwner = hMain;
    ofn.lpstrFile = buf.data();
    ofn.nMaxFile = (DWORD)buf.size();
    ofn.lpstrTitle = T(titleId);
    ofn.lpstrInitialDir = startDir.empty() ? nullptr : startDir.c_str();
    ofn.lpstrFilter = filter.c_str();
    ofn.Flags = OFN_ALLOWMULTISELECT | OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    std::vector<std::wstring> out;
    if (!GetOpenFileNameW(&ofn)) return out;
    std::wstring first = buf.data();
    const wchar_t* p = buf.data() + first.size() + 1;
    if (*p == 0) { out.push_back(first); return out; }
    std::wstring dir = first;
    if (!dir.empty() && dir.back() != L'\\') dir += L'\\';
    while (*p)
    {
        out.push_back(dir + p);
        p += wcslen(p) + 1;
    }
    return out;
}
static bool IsArchiveName(const std::wstring& n)
{
    size_t d = n.find_last_of(L'.');
    if (d == std::wstring::npos) return false;
    std::wstring e = n.substr(d);
    for (auto& c : e) c = (wchar_t)towlower(c);
    size_t k = e.compare(0, 4, L".xdb") == 0 ? 4 : e.compare(0, 3, L".db") == 0 ? 3 : 0;
    if (!k) return false;
    for (size_t i = k; i < e.size(); ++i)
        if (!((e[i] >= L'0' && e[i] <= L'9') || (e[i] >= L'a' && e[i] <= L'z'))) return false;
    return true;
}

// ------------------------------------------------------------------ actions
static void DoPack()
{
    std::wstring src = GetText(hSrc), out = GetText(hOut), name = GetText(hName);
    DWORD at = GetFileAttributesW(src.c_str());
    if (src.empty() || at == INVALID_FILE_ATTRIBUTES || !(at & FILE_ATTRIBUTE_DIRECTORY)) { MessageBoxW(hMain, T(S_WARN_SRC), L"xrCompressorMT", MB_ICONWARNING); return; }
    if (out.empty()) { MessageBoxW(hMain, T(S_WARN_OUT), L"xrCompressorMT", MB_ICONWARNING); return; }
    bool xdb = SendMessageW(hXdb, BM_GETCHECK, 0, 0) == BST_CHECKED;
    std::wstring ext = xdb ? L".xdb" : L".db";
    if (name.empty()) name = Base(src);
    if (name.empty()) name = L"archive";
    if (name.size() > ext.size() && _wcsicmp(name.c_str() + name.size() - ext.size(), ext.c_str()) == 0) name.resize(name.size() - ext.size());
    SetWindowTextW(hName, name.c_str());
    SHCreateDirectoryExW(nullptr, out.c_str(), nullptr);
    std::wstring outDir = out;
    while (outDir.size() > 3 && (outDir.back() == L'\\' || outDir.back() == L'/')) outDir.pop_back();
    if (outDir.back() != L'\\') outDir += L'\\';

    int mx = std::min(std::max(GetInt(hMax, 1900), 16), 2047);
    std::wstring args = Arg(src);
    std::wstring ltx = exeDir + L"datapack.ltx";
    if (GetFileAttributesW(ltx.c_str()) != INVALID_FILE_ATTRIBUTES) args += L" -ltx " + Arg(ltx);
    if (xdb) args += L" -xdb";
    args += L" -max_size " + std::to_wstring(mx);
    if (SendMessageW(hFast, BM_GETCHECK, 0, 0) == BST_CHECKED) args += L" -fast";
    else if (SendMessageW(hStore, BM_GETCHECK, 0, 0) == BST_CHECKED) args += L" -store";
    int thr = GetInt(hThr, 0);
    if (thr > 0) args += L" -threads " + std::to_wstring(thr);
    args += L" -mem " + std::to_wstring(std::max(GetInt(hMem, 1024), 64));
    if (SendMessageW(hVerify, BM_GETCHECK, 0, 0) != BST_CHECKED) args += L" -noverify";
    args += L" -out \"" + outDir + name + ext + L"\"";
    SaveSettings();
    RunJobs(0, {args}, outDir + name + L".log");
}
static void DoCheck()
{
    auto files = PickArchives(GetText(hOut), S_DLG_CHECK);
    if (files.empty()) return;
    std::wstring args = L"-check";
    for (auto& f : files) args += L" " + Arg(f);
    RunJobs(0, {args}, L"");
}
static void ListAdd(const std::wstring& p)
{
    if (SendMessageW(hList, LB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)p.c_str()) == LB_ERR)
        SendMessageW(hList, LB_ADDSTRING, 0, (LPARAM)p.c_str());
}
static void DoAddFiles()
{
    std::wstring start;
    if (SendMessageW(hList, LB_GETCOUNT, 0, 0) > 0)
    {
        wchar_t b[1024];
        SendMessageW(hList, LB_GETTEXT, 0, (LPARAM)b);
        start = b;
        size_t i = start.find_last_of(L'\\');
        start = (i == std::wstring::npos) ? L"" : start.substr(0, i);
    }
    auto files = PickArchives(start, S_DLG_UNPACK);
    std::sort(files.begin(), files.end(), [](const std::wstring& a, const std::wstring& b) { return _wcsicmp(Base(a).c_str(), Base(b).c_str()) < 0; });
    for (auto& f : files) ListAdd(f);
}
// Recursively collects *.xdb* / *.db* archives from dir (which ends with '\\') and all of its subfolders.
static void ScanArchives(const std::wstring& dir, std::vector<std::wstring>& found, int depth = 0)
{
    if (depth > 32) return;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((dir + L"*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do
    {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..")) continue;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;   // avoid junction/symlink loops
            ScanArchives(dir + fd.cFileName + L"\\", found, depth + 1);
        }
        else if (IsArchiveName(fd.cFileName))
            found.push_back(dir + fd.cFileName);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}
static void DoAddDir()
{
    std::wstring d = BrowseDir(L"", T(S_DLG_DIR));
    if (d.empty()) return;
    if (d.back() != L'\\') d += L'\\';
    std::vector<std::wstring> found;
    ScanArchives(d, found);
    if (found.empty()) MessageBoxW(hMain, T(S_NO_ARCH_DIR), L"xrCompressorMT", MB_ICONINFORMATION);
    // sort by full path: archives of one folder stay together and a series (x.xdb0, x.xdb1...) keeps its order
    std::sort(found.begin(), found.end(), [](const std::wstring& a, const std::wstring& b) { return _wcsicmp(a.c_str(), b.c_str()) < 0; });
    for (auto& f : found) ListAdd(f);
}
static void DoRemove()
{
    int n = (int)SendMessageW(hList, LB_GETSELCOUNT, 0, 0);
    if (n <= 0) return;
    std::vector<int> sel(n);
    SendMessageW(hList, LB_GETSELITEMS, n, (LPARAM)sel.data());
    for (int i = n - 1; i >= 0; --i) SendMessageW(hList, LB_DELETESTRING, sel[i], 0);
}
static void DoUnpack()
{
    std::wstring out = GetText(hUOut);
    int cnt = (int)SendMessageW(hList, LB_GETCOUNT, 0, 0);
    if (cnt == 0) { MessageBoxW(hMain, T(S_WARN_ADD), L"xrCompressorMT", MB_ICONWARNING); return; }
    if (out.empty()) { MessageBoxW(hMain, T(S_WARN_OUT), L"xrCompressorMT", MB_ICONWARNING); return; }
    std::vector<std::wstring> archives;
    std::wstring missing;
    for (int i = 0; i < cnt; ++i)
    {
        int len = (int)SendMessageW(hList, LB_GETTEXTLEN, i, 0);
        std::wstring s(len + 1, 0);
        SendMessageW(hList, LB_GETTEXT, i, (LPARAM)&s[0]);
        s.resize(len);
        archives.push_back(s);
        if (GetFileAttributesW(s.c_str()) == INVALID_FILE_ATTRIBUTES) missing += s + L"\n";
    }
    if (!missing.empty()) { MessageBoxW(hMain, (std::wstring(T(S_WARN_MISSING)) + missing).c_str(), L"xrCompressorMT", MB_ICONWARNING); return; }
    SHCreateDirectoryExW(nullptr, out.c_str(), nullptr);
    std::vector<std::wstring> jobs;
    for (auto& a : archives) jobs.push_back(L"-unpack " + Arg(a) + L" " + Arg(out));
    SaveSettings();
    std::wstring od = out;
    while (od.size() > 3 && (od.back() == L'\\' || od.back() == L'/')) od.pop_back();
    if (od.back() != L'\\') od += L'\\';
    RunJobs(1, jobs, od + L"unpack.log");
}

// ------------------------------------------------------------------ layout
static void LayoutPage(Page& p, int W, int H, bool pack)
{
    const int m = S(10), rh = S(24), lw = S(150), bw = S(90);
    int y = m;
    if (pack)
    {
        int gh = S(30) + 3 * S(32) - S(4);
        PlaceGrp(hGrpPaths, m, y, W - 2 * m, gh);
        int yy = y + S(22);
        int ex = m + S(12) + lw, ew = W - 2 * m - S(24) - lw - bw - S(6);
        Place(hLbSrc, m + S(12), yy + 4, lw, S(18)); Place(hSrc, ex, yy, ew, rh); Place(hBSrc, W - m - S(12) - bw, yy, bw, rh);
        yy += S(32);
        Place(hLbOut, m + S(12), yy + 4, lw, S(18)); Place(hOut, ex, yy, ew, rh); Place(hBOut, W - m - S(12) - bw, yy, bw, rh);
        yy += S(32);
        Place(hLbName, m + S(12), yy + 4, lw, S(18)); Place(hName, ex, yy, W - 2 * m - S(24) - lw, rh);
        y += gh + S(8);
        int ph = S(22) + 5 * S(22) + S(8);
        PlaceGrp(hGrpParams, m, y, W - 2 * m, ph);
        int cy = y + S(20), cw = (W - 2 * m) * 55 / 100;
        for (HWND c : {hXdb, hBest, hFast, hStore, hVerify}) { Place(c, m + S(12), cy, cw - S(12), S(20)); cy += S(22); }
        int rx = m + cw + S(4), rl = (W - m) - rx - S(12) - S(90) - S(6);
        int ry = y + S(18);
        Place(hLbMax, rx, ry + 4, rl, S(18)); Place(hMax, W - m - S(12) - S(90), ry, S(90), rh);
        ry += S(32);
        Place(hLbThr, rx, ry + 4, rl, S(18)); Place(hThr, W - m - S(12) - S(90), ry, S(90), rh);
        ry += S(32);
        Place(hLbMem, rx, ry + 4, rl, S(18)); Place(hMem, W - m - S(12) - S(90), ry, S(90), rh);
        for (auto pr : {std::make_pair(hUd1, hMax), std::make_pair(hUd2, hThr), std::make_pair(hUd3, hMem)})
            SendMessageW(pr.first, UDM_SETBUDDY, (WPARAM)pr.second, 0);   // re-align arrows to the moved edit
        y += ph + S(8);
        int bh = S(30), bx = m;
        Place(p.start, bx, y, S(130), bh); bx += S(138);
        Place(p.stop, bx, y, S(80), bh); bx += S(88);
        Place(hBCheck, bx, y, S(170), bh); bx += S(178);
        Place(p.open, bx, y, S(190), bh);
        y += bh + S(10);
    }
    else
    {
        int lh = std::max(S(90), (H - S(330)) / 2 + S(40));
        int gh = lh + S(28) + S(36);
        PlaceGrp(hUGrpList, m, y, W - 2 * m, gh);
        Place(hList, m + S(10), y + S(20), W - 2 * m - S(20), lh);
        int bx = m + S(10), by = y + S(20) + lh + S(6);
        int bws[4] = {S(130), S(160), S(150), S(90)};
        HWND bs[4] = {hUAdd, hUAddDir, hURm, hUClr};
        for (int i = 0; i < 4; ++i) { Place(bs[i], bx, by, bws[i], S(26)); bx += bws[i] + S(8); }
        y += gh + S(8);
        int pgh = S(56);
        PlaceGrp(hUGrpPaths, m, y, W - 2 * m, pgh);
        int ex = m + S(12) + lw;
        Place(hULbOut, m + S(12), y + S(22) + 4, lw, S(18));
        Place(hUOut, ex, y + S(22), W - 2 * m - S(24) - lw - bw - S(6), rh);
        Place(hUBOut, W - m - S(12) - bw, y + S(22), bw, rh);
        y += pgh + S(8);
        int bh = S(30), bx2 = m;
        Place(p.start, bx2, y, S(130), bh); bx2 += S(138);
        Place(p.stop, bx2, y, S(80), bh); bx2 += S(88);
        Place(p.open, bx2, y, S(190), bh);
        y += bh + S(10);
    }
    Place(p.bar, m, y, W - 2 * m, S(18)); y += S(24);
    Place(p.status, m, y, W - 2 * m, S(18)); y += S(24);
    Place(p.log, m, y, W - 2 * m, std::max(S(50), H - y - m));
}

static void LayoutAll()
{
    RECT rc;
    GetClientRect(hMain, &rc);
    int W = rc.right, H = rc.bottom;
    // the tab control only covers the strip; pages and buttons never overlap it
    int stripH = S(36) + S(10);
    SetWindowPos(hTab, HWND_BOTTOM, 0, 0, W, stripH, SWP_NOACTIVATE);
    RECT disp{0, stripH, W, H};
    int sel = TabCtrl_GetCurSel(hTab);
    for (int i = 0; i < 2; ++i)
    {
        Place(pages[i]->hwnd, disp.left, disp.top, disp.right - disp.left, disp.bottom - disp.top);
        ShowWindow(pages[i]->hwnd, i == sel ? SW_SHOW : SW_HIDE);
    }
    LayoutPage(pk, disp.right - disp.left, disp.bottom - disp.top, true);
    LayoutPage(up, disp.right - disp.left, disp.bottom - disp.top, false);
    int bh = S(28), y = S(4);
    Place(hBtnUa, W - S(6) - S(40), y, S(40), bh);
    Place(hBtnEn, W - S(6) - S(40) - S(4) - S(40), y, S(40), bh);
    Place(hBtnAbout, W - S(6) - S(40) - S(4) - S(40) - S(4) - S(130), y, S(130), bh);
}

static void Retranslate()
{
    for (auto& t : trItems) SetWindowTextW(t.h, T(t.s));
    SetWindowTextW(hMain, T(S_TITLE));
    TCITEMW ti{};
    ti.mask = TCIF_TEXT;
    ti.pszText = (LPWSTR)T(S_TAB_PACK);
    TabCtrl_SetItem(hTab, 0, &ti);
    ti.pszText = (LPWSTR)T(S_TAB_UNPACK);
    TabCtrl_SetItem(hTab, 1, &ti);
    SendMessageW(hBtnEn, BM_SETCHECK, LANG == 1 ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(hBtnUa, BM_SETCHECK, LANG == 0 ? BST_CHECKED : BST_UNCHECKED, 0);
    for (Page* p : pages) if (p->statusFn) SetWindowTextW(p->status, p->statusFn().c_str());
    if (hAboutWnd) DestroyWindow(hAboutWnd);
    LayoutAll();
    RedrawWindow(hMain, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
}

// ------------------------------------------------------------------ page window class (forwards to main, white background)
static LRESULT CALLBACK PageProc(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    switch (m)
    {
    case WM_COMMAND:
    case WM_NOTIFY:
        return SendMessageW(hMain, m, wp, lp);
    case WM_CTLCOLORSTATIC:
    {
        HDC dc = (HDC)wp;
        SetBkMode(dc, OPAQUE);
        SetBkColor(dc, GetSysColor(COLOR_WINDOW));
        return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
    }
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        HGDIOBJ of = SelectObject(dc, uiFont);
        for (const Grp& g : groups)
        {
            if (g.page != h || g.r.right <= g.r.left) continue;
            const wchar_t* t = T(g.sid);
            SIZE sz{};
            GetTextExtentPoint32W(dc, t, (int)wcslen(t), &sz);
            RECT fr = g.r;
            fr.top += sz.cy / 2;
            DrawEdge(dc, &fr, EDGE_ETCHED, BF_RECT);
            RECT tr{g.r.left + S(10), g.r.top, g.r.left + S(10) + sz.cx + S(8), g.r.top + sz.cy};
            FillRect(dc, &tr, GetSysColorBrush(COLOR_WINDOW));
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, GetSysColor(COLOR_WINDOWTEXT));
            DrawTextW(dc, t, -1, &tr, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
        }
        SelectObject(dc, of);
        EndPaint(h, &ps);
        return 0;
    }
    }
    return DefWindowProcW(h, m, wp, lp);
}

static void MakeRunArea(Page& p)
{
    p.bar = CreateWindowExW(0, PROGRESS_CLASSW, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 0, 0, 10, 10, p.hwnd, nullptr, nullptr, nullptr);
    SendMessageW(p.bar, PBM_SETRANGE32, 0, 100);
    p.status = Mk(p.hwnd, L"STATIC", L"", 0, 0);
    p.log = Mk(p.hwnd, L"EDIT", L"", WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL, 0, WS_EX_CLIENTEDGE);
    SendMessageW(p.log, EM_SETLIMITTEXT, 8 * 1024 * 1024, 0);
    SendMessageW(p.log, WM_SETFONT, (WPARAM)monoFont, TRUE);
    SetStatusKey(p, S_READY);
}

// ------------------------------------------------------------------ About window
static LRESULT CALLBACK AboutProc(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    switch (m)
    {
    case WM_CREATE:
    {
        int x = S(22), w = S(420), y = S(16);
        HICON ic = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(1), IMAGE_ICON, S(64), S(64), LR_DEFAULTCOLOR);
        if (ic)
        {
            HWND ci = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_ICON, x, y, S(64), S(64), h, nullptr, nullptr, nullptr);
            SendMessageW(ci, STM_SETICON, (WPARAM)ic, 0);
        }
        auto st = [&](const std::wstring& t, int hgt, HFONT f, DWORD style = 0, int id = 0) {
            HWND c = CreateWindowExW(0, L"STATIC", t.c_str(), WS_CHILD | WS_VISIBLE | style, x, y, w, hgt, h, (HMENU)(INT_PTR)id, nullptr, nullptr);
            SendMessageW(c, WM_SETFONT, (WPARAM)f, TRUE);
            y += hgt + S(8);
            return c;
        };
        {
            int sx = x, sw = w;
            x += S(76); w -= S(76);
            st(L"xrCompressorMT", S(30), bigFont, 0, 900);
            st(T(S_ABOUT_SUB), S(34), uiFont);
            x = sx; w = sw;
            if (y < S(16) + S(72)) y = S(16) + S(72);
        }
        CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ, x, y, w, 2, h, nullptr, nullptr, nullptr);
        y += S(10);
        st(T(S_ABOUT_TEXT), S(38), uiFont);
        for (Sid s : {S_ABOUT_GAMES, S_ABOUT_X64, S_ABOUT_MT, S_ABOUT_VERIFY})
            st(std::wstring(L"✔  ") + T(s), S(s == S_ABOUT_GAMES ? 38 : 20), uiFont);
        CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ, x, y, w, 2, h, nullptr, nullptr, nullptr);
        y += S(10);
        st(std::wstring(T(S_AUTHOR)) + L": Ivan Danko, 2026", S(20), uiFont);
        st(std::wstring(T(S_CHANNEL)) + L": t.me/frozen_zone", S(20), uiFont, SS_NOTIFY, 901);
        RECT r{0, 0, x * 2 + w, y + S(10)};
        AdjustWindowRectEx(&r, WS_POPUP | WS_CAPTION | WS_SYSMENU, FALSE, 0);
        SetWindowPos(h, nullptr, 0, 0, r.right - r.left, r.bottom - r.top, SWP_NOMOVE | SWP_NOZORDER);
        return 0;
    }
    case WM_CTLCOLORSTATIC:
    {
        HDC dc = (HDC)wp;
        int id = GetDlgCtrlID((HWND)lp);
        SetBkMode(dc, OPAQUE);
        SetBkColor(dc, GetSysColor(COLOR_WINDOW));
        if (id == 900 || id == 901) SetTextColor(dc, ACCENT);
        return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
    }
    case WM_COMMAND:
        if (LOWORD(wp) == 901) ShellExecuteW(h, L"open", L"https://t.me/frozen_zone", nullptr, nullptr, SW_SHOWNORMAL);
        return 0;
    case WM_SETCURSOR:
    {
        POINT pt;
        GetCursorPos(&pt);
        HWND c = ChildWindowFromPoint(h, (ScreenToClient(h, &pt), pt));
        if (c && GetDlgCtrlID(c) == 901) { SetCursor(LoadCursorW(nullptr, IDC_HAND)); return TRUE; }
        break;
    }
    case WM_CLOSE:
        DestroyWindow(h);
        return 0;
    case WM_DESTROY:
        EnableWindow(hMain, TRUE);
        SetForegroundWindow(hMain);
        hAboutWnd = nullptr;
        return 0;
    }
    return DefWindowProcW(h, m, wp, lp);
}
static void ShowAbout()
{
    if (hAboutWnd) { SetForegroundWindow(hAboutWnd); return; }
    hAboutWnd = CreateWindowExW(WS_EX_DLGMODALFRAME, L"xrCompressorMT_About", T(S_ABOUT_TITLE), WS_POPUP | WS_CAPTION | WS_SYSMENU,
                                0, 0, 100, 100, hMain, nullptr, nullptr, nullptr);
    RECT pr, ar;
    GetWindowRect(hMain, &pr);
    GetWindowRect(hAboutWnd, &ar);
    SetWindowPos(hAboutWnd, nullptr, pr.left + ((pr.right - pr.left) - (ar.right - ar.left)) / 2, pr.top + ((pr.bottom - pr.top) - (ar.bottom - ar.top)) / 2, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    EnableWindow(hMain, FALSE);
    ShowWindow(hAboutWnd, SW_SHOW);
}

// ------------------------------------------------------------------ main window
static void CreateControls()
{
    pk.hwnd = CreateWindowExW(0, L"xrCompressorMT_Page", L"", WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, 0, 0, 10, 10, hMain, nullptr, nullptr, nullptr);
    up.hwnd = CreateWindowExW(0, L"xrCompressorMT_Page", L"", WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, 0, 0, 10, 10, hMain, nullptr, nullptr, nullptr);
    HWND P = pk.hwnd;
    hGrpPaths = MkGroup(P, S_GRP_PATHS);
    hLbSrc = MkT(P, L"STATIC", S_SRC, 0, 0);
    hLbOut = MkT(P, L"STATIC", S_OUT, 0, 0);
    hLbName = MkT(P, L"STATIC", S_NAME, 0, 0);
    hSrc = Mk(P, L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL, ID_P_SRC, WS_EX_CLIENTEDGE);
    hOut = Mk(P, L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL, ID_P_OUT, WS_EX_CLIENTEDGE);
    hName = Mk(P, L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL, ID_P_NAME, WS_EX_CLIENTEDGE);
    hBSrc = MkT(P, L"BUTTON", S_BROWSE, WS_TABSTOP, ID_P_BSRC);
    hBOut = MkT(P, L"BUTTON", S_BROWSE, WS_TABSTOP, ID_P_BOUT);
    hGrpParams = MkGroup(P, S_GRP_PARAMS);
    hXdb = MkT(P, L"BUTTON", S_XDB, WS_TABSTOP | BS_AUTOCHECKBOX, ID_P_XDB);
    hBest = MkT(P, L"BUTTON", S_BEST, WS_TABSTOP | WS_GROUP | BS_AUTORADIOBUTTON, ID_P_BEST);
    hFast = MkT(P, L"BUTTON", S_FAST, BS_AUTORADIOBUTTON, ID_P_FAST);
    hStore = MkT(P, L"BUTTON", S_STORE, BS_AUTORADIOBUTTON, ID_P_STORE);
    hVerify = MkT(P, L"BUTTON", S_VERIFY, WS_TABSTOP | WS_GROUP | BS_AUTOCHECKBOX, ID_P_VERIFY);
    hLbMax = MkT(P, L"STATIC", S_MAXSIZE, 0, 0);
    hLbThr = MkT(P, L"STATIC", S_THREADS, 0, 0);
    hLbMem = MkT(P, L"STATIC", S_MEM, 0, 0);
    hMax = MkSpin(P, ID_P_MAX, ID_P_MAXUD, 16, 2047, 1900, hUd1);
    hThr = MkSpin(P, ID_P_THR, ID_P_THRUD, 0, 64, 0, hUd2);
    hMem = MkSpin(P, ID_P_MEM, ID_P_MEMUD, 64, 32768, 1024, hUd3);
    pk.start = MkT(P, L"BUTTON", S_PACK, WS_TABSTOP | BS_DEFPUSHBUTTON, ID_P_START);
    pk.stop = MkT(P, L"BUTTON", S_STOP, WS_TABSTOP | WS_DISABLED, ID_P_STOP);
    hBCheck = MkT(P, L"BUTTON", S_CHECK, WS_TABSTOP, ID_P_CHECK);
    pk.open = MkT(P, L"BUTTON", S_OPEN, WS_TABSTOP, ID_P_OPEN);
    MakeRunArea(pk);

    HWND Q = up.hwnd;
    hUGrpList = MkGroup(Q, S_GRP_ARCH);
    hList = Mk(Q, L"LISTBOX", L"", WS_TABSTOP | WS_VSCROLL | WS_HSCROLL | LBS_EXTENDEDSEL | LBS_NOINTEGRALHEIGHT | LBS_NOTIFY, ID_U_LIST, WS_EX_CLIENTEDGE);
    hUAdd = MkT(Q, L"BUTTON", S_ADD, WS_TABSTOP, ID_U_ADD);
    hUAddDir = MkT(Q, L"BUTTON", S_ADDDIR, WS_TABSTOP, ID_U_ADDDIR);
    hURm = MkT(Q, L"BUTTON", S_REMOVE, WS_TABSTOP, ID_U_RM);
    hUClr = MkT(Q, L"BUTTON", S_CLEAR, WS_TABSTOP, ID_U_CLR);
    hUGrpPaths = MkGroup(Q, S_GRP_PATHS);
    hULbOut = MkT(Q, L"STATIC", S_OUT, 0, 0);
    hUOut = Mk(Q, L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL, ID_U_OUT, WS_EX_CLIENTEDGE);
    hUBOut = MkT(Q, L"BUTTON", S_BROWSE, WS_TABSTOP, ID_U_BOUT);
    up.start = MkT(Q, L"BUTTON", S_UNPACK, WS_TABSTOP | BS_DEFPUSHBUTTON, ID_U_START);
    up.stop = MkT(Q, L"BUTTON", S_STOP, WS_TABSTOP | WS_DISABLED, ID_U_STOP);
    up.open = MkT(Q, L"BUTTON", S_OPEN, WS_TABSTOP, ID_U_OPEN);
    MakeRunArea(up);

    hBtnAbout = MkT(hMain, L"BUTTON", S_ABOUT, WS_TABSTOP, ID_ABOUT);
    hBtnEn = Mk(hMain, L"BUTTON", L"EN", BS_CHECKBOX | BS_PUSHLIKE, ID_LANG_EN);
    hBtnUa = Mk(hMain, L"BUTTON", L"UA", BS_CHECKBOX | BS_PUSHLIKE, ID_LANG_UA);

    // load settings
    wchar_t b[2048];
    auto rd = [&](const wchar_t* k, HWND e) { GetPrivateProfileStringW(L"gui", k, L"", b, 2048, iniPath.c_str()); SetWindowTextW(e, b); };
    rd(L"src", hSrc); rd(L"out", hOut); rd(L"name", hName); rd(L"unpack_out", hUOut);
    SetInt(hMax, std::min(std::max((int)GetPrivateProfileIntW(L"gui", L"maxsize", 1900, iniPath.c_str()), 16), 2047));
    SetInt(hThr, GetPrivateProfileIntW(L"gui", L"threads", 0, iniPath.c_str()));
    SetInt(hMem, GetPrivateProfileIntW(L"gui", L"mem", 1024, iniPath.c_str()));
    SendMessageW(hXdb, BM_SETCHECK, GetPrivateProfileIntW(L"gui", L"xdb", 1, iniPath.c_str()) ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(hVerify, BM_SETCHECK, GetPrivateProfileIntW(L"gui", L"verify", 1, iniPath.c_str()) ? BST_CHECKED : BST_UNCHECKED, 0);
    GetPrivateProfileStringW(L"gui", L"mode", L"best", b, 64, iniPath.c_str());
    SendMessageW(wcscmp(b, L"fast") == 0 ? hFast : wcscmp(b, L"store") == 0 ? hStore : hBest, BM_SETCHECK, BST_CHECKED, 0);
}

static LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        hMain = h;
        hTab = CreateWindowExW(0, WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP | TCS_FIXEDWIDTH | TCS_FOCUSNEVER, 0, 0, 10, 10, h, (HMENU)(INT_PTR)ID_TAB, nullptr, nullptr);
        SendMessageW(hTab, WM_SETFONT, (WPARAM)uiFont, TRUE);
        TCITEMW ti{};
        ti.mask = TCIF_TEXT;
        ti.pszText = (LPWSTR)L"-";
        TabCtrl_InsertItem(hTab, 0, &ti);
        TabCtrl_InsertItem(hTab, 1, &ti);
        TabCtrl_SetItemSize(hTab, S(210), S(36));
        wchar_t lg[16];
        GetPrivateProfileStringW(L"gui", L"lang", L"ua", lg, 16, iniPath.c_str());
        LANG = wcscmp(lg, L"en") == 0 ? 1 : 0;
        CreateControls();
        Retranslate();
        return 0;
    }
    case WM_SIZE:
        LayoutAll();
        return 0;
    case WM_GETMINMAXINFO:
    {
        RECT wa;
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
        ((MINMAXINFO*)lp)->ptMinTrackSize = {std::min(S(720), (int)(wa.right - wa.left)), std::min(S(500), (int)(wa.bottom - wa.top))};
    }
        return 0;
    case WM_NOTIFY:
    {
        NMHDR* n = (NMHDR*)lp;
        if (n->hwndFrom == hTab)
        {
            if (n->code == TCN_SELCHANGING) return (pk.running || up.running) ? TRUE : FALSE;
            if (n->code == TCN_SELCHANGE) LayoutAll();
        }
        return 0;
    }
    case WM_COMMAND:
        switch (LOWORD(wp))
        {
        case ID_ABOUT: ShowAbout(); break;
        case ID_LANG_EN: case ID_LANG_UA:
            LANG = LOWORD(wp) == ID_LANG_EN ? 1 : 0;
            WritePrivateProfileStringW(L"gui", L"lang", LANG ? L"en" : L"ua", iniPath.c_str());
            Retranslate();
            break;
        case ID_P_BSRC: BrowseInto(hSrc, T(S_SRC)); break;
        case ID_P_BOUT: BrowseInto(hOut, T(S_OUT)); break;
        case ID_U_BOUT: BrowseInto(hUOut, T(S_OUT)); break;
        case ID_P_START: if (!pk.running) DoPack(); break;
        case ID_P_STOP: StopRun(0); break;
        case ID_P_CHECK: if (!pk.running) DoCheck(); break;
        case ID_P_OPEN: { std::wstring o = GetText(hOut); if (!o.empty()) ShellExecuteW(h, L"open", o.c_str(), nullptr, nullptr, SW_SHOWNORMAL); break; }
        case ID_U_ADD: DoAddFiles(); break;
        case ID_U_ADDDIR: DoAddDir(); break;
        case ID_U_RM: DoRemove(); break;
        case ID_U_CLR: SendMessageW(hList, LB_RESETCONTENT, 0, 0); break;
        case ID_U_START: if (!up.running) DoUnpack(); break;
        case ID_U_STOP: StopRun(1); break;
        case ID_U_OPEN: { std::wstring o = GetText(hUOut); if (!o.empty()) ShellExecuteW(h, L"open", o.c_str(), nullptr, nullptr, SW_SHOWNORMAL); break; }
        }
        return 0;
    case WM_LINE:
    {
        Page& p = *pages[(int)wp];
        std::string* s = (std::string*)lp;
        int a, b2;
        double x, y2, sp;
        if (sscanf(s->c_str(), " %d/%d files %lf/%lf MB %lf MB/s", &a, &b2, &x, &y2, &sp) == 5)
        {
            SendMessageW(p.bar, PBM_SETPOS, b2 ? (WPARAM)(100.0 * a / b2) : 0, 0);
            wchar_t xs[32], ys[32], ss[32];
            swprintf(xs, 32, L"%.1f", x); swprintf(ys, 32, L"%.1f", y2); swprintf(ss, 32, L"%.1f", sp);
            std::wstring X = xs, Y = ys, SS = ss;
            SetStatus(p, [a, b2, X, Y, SS] { return Fmt(S_PROGRESS, a, b2, X.c_str(), Y.c_str(), SS.c_str()); });
        }
        else AppendLog(p, W(*s));
        delete s;
        return 0;
    }
    case WM_DONE:
    {
        Page& p = *pages[(int)wp];
        DWORD code = (DWORD)lp;
        if (p.hProc) { CloseHandle(p.hProc); p.hProc = nullptr; }
        if (code != 0 && !p.stopped) ++p.failed;
        if (p.jobTotal > 1 && !p.stopped) SendMessageW(p.bar, PBM_SETPOS, 100 * p.jobIndex / p.jobTotal, 0);
        NextJob(p, (int)wp);
        return 0;
    }
    case WM_CLOSE:
        for (Page* p : pages) { p->jobs.clear(); p->stopped = true; if (p->running && p->hProc) TerminateProcess(p->hProc, 1); }
        SaveSettings();
        DestroyWindow(h);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hi, HINSTANCE, LPSTR, int show)
{
    // ---- console mode: any command-line argument -> run the packer (same exe)
    {
        int argc = 0;
        LPWSTR* argvw = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (argvw && argc > 1)
        {
            HANDLE so = GetStdHandle(STD_OUTPUT_HANDLE);
            if (!so || so == INVALID_HANDLE_VALUE)
            {
                // started from a console without redirection: borrow the parent's console
                if (AttachConsole(ATTACH_PARENT_PROCESS))
                {
                    freopen("CONOUT$", "w", stdout);
                    freopen("CONOUT$", "w", stderr);
                }
            }
            std::vector<std::string> a8;
            std::vector<char*> ap;
            for (int i = 0; i < argc; ++i) a8.push_back(U(argvw[i]));
            for (auto& x : a8) ap.push_back(&x[0]);
            ap.push_back(nullptr);
            return packer_main(argc, ap.data());
        }
    }
    SetProcessDPIAware();
    wchar_t p[MAX_PATH * 2];
    GetModuleFileNameW(nullptr, p, MAX_PATH * 2);
    selfPath = p;
    exeDir = p;
    exeDir.resize(exeDir.find_last_of(L'\\') + 1);
    iniPath = exeDir + L"xrCompressorMT_GUI.ini";
    HDC sdc = GetDC(nullptr);
    dpi = GetDeviceCaps(sdc, LOGPIXELSX);
    ReleaseDC(nullptr, sdc);

    OleInitialize(nullptr);
    INITCOMMONCONTROLSEX ic{sizeof ic, ICC_PROGRESS_CLASS | ICC_TAB_CLASSES | ICC_UPDOWN_CLASS};
    InitCommonControlsEx(&ic);

    NONCLIENTMETRICSW ncm{sizeof ncm};
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof ncm, &ncm, 0);
    uiFont = CreateFontIndirectW(&ncm.lfMessageFont);
    LOGFONTW big = ncm.lfMessageFont;
    big.lfHeight = big.lfHeight * 22 / 10;
    big.lfWeight = FW_SEMIBOLD;
    bigFont = CreateFontIndirectW(&big);
    monoFont = CreateFontW(ncm.lfMessageFont.lfHeight, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, FIXED_PITCH, L"Consolas");

    WNDCLASSW wc{};
    wc.hInstance = hi;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = (HICON)LoadImageW(hi, MAKEINTRESOURCEW(1), IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);
    wc.lpfnWndProc = WndProc;
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"xrCompressorMT_GUI";
    RegisterClassW(&wc);
    hIconSmall = (HICON)LoadImageW(hi, MAKEINTRESOURCEW(1), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
    hIconBig = wc.hIcon;
    hInst = hi;
    WNDCLASSW pc{};
    pc.hInstance = hi;
    pc.lpfnWndProc = PageProc;
    pc.hCursor = wc.hCursor;
    pc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    pc.lpszClassName = L"xrCompressorMT_Page";
    RegisterClassW(&pc);
    WNDCLASSW ac{};
    ac.hInstance = hi;
    ac.lpfnWndProc = AboutProc;
    ac.hCursor = wc.hCursor;
    ac.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    ac.lpszClassName = L"xrCompressorMT_About";
    RegisterClassW(&ac);

    RECT wa;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    int waW = wa.right - wa.left, waH = wa.bottom - wa.top;
    int ww = std::min(S(780), waW - S(20)), wh = std::min(S(600), waH - S(20));
    HWND w = CreateWindowExW(0, wc.lpszClassName, L"xrCompressorMT", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, wa.left + (waW - ww) / 2, wa.top + (waH - wh) / 2, ww, wh, nullptr, nullptr, hi, nullptr);
    if (hIconSmall) SendMessageW(w, WM_SETICON, ICON_SMALL, (LPARAM)hIconSmall);
    if (hIconBig) SendMessageW(w, WM_SETICON, ICON_BIG, (LPARAM)hIconBig);
    ShowWindow(w, show);
    UpdateWindow(w);
    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0) > 0)
    {
        if (!IsDialogMessageW(w, &m))
        {
            TranslateMessage(&m);
            DispatchMessageW(&m);
        }
    }
    return 0;
}
