// xrCompressorMT - multithreaded, x64 packer for X-Ray .db/.xdb archives.
// Format is compatible with CoC/CoP and xray-monolith readers (LocatorAPI::ProcessArchive).
//
// Archive layout:
//   chunk 666  : ini header  ([header] ...)                    (optional)
//   chunk 0    : raw data of all files (lzo1x or stored)
//   chunk 1    : file table, LzHuf-compressed (type | 0x80000000)
// Table record: u16 size | u32 size_real | u32 size_compr | u32 crc32 | name | u32 ptr
// size_real == size_compr  => stored uncompressed.  ptr is absolute offset in the archive.

#include "stdafx.h"
#include "rt_lzo1x.h"
#include "lzhuf.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <filesystem>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <tuple>
#include <vector>


#ifdef _WIN32
extern "C" {
__declspec(dllimport) wchar_t* __stdcall GetCommandLineW(void);
__declspec(dllimport) wchar_t** __stdcall CommandLineToArgvW(const wchar_t*, int*);
__declspec(dllimport) int __stdcall WideCharToMultiByte(unsigned, unsigned long, const wchar_t*, int, char*, int, const char*, int*);
__declspec(dllimport) int __stdcall SetConsoleOutputCP(unsigned);
}
#endif

namespace sfs = std::filesystem;
using Clock = std::chrono::steady_clock;


// ------------------------------------------------------------------ utils

static FILE* fopen_p(const sfs::path& p, const char* mode)
{
#ifdef _WIN32
    std::wstring m;
    for (const char* c = mode; *c; ++c) m.push_back(wchar_t(*c));
    return _wfopen(p.c_str(), m.c_str());
#else
    return fopen(p.c_str(), mode);
#endif
}

static int seek64(FILE* f, long long off, int whence)
{
#ifdef _WIN32
    return _fseeki64(f, off, whence);
#else
    return fseeko(f, off, whence);
#endif
}

static long long tell64(FILE* f)
{
#ifdef _WIN32
    return _ftelli64(f);
#else
    return ftello(f);
#endif
}

static std::string lower(std::string s)
{
    for (auto& c : s) if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
    return s;
}

static std::string trim(const std::string& s)
{
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n')) --b;
    return s.substr(a, b - a);
}

static u32 g_crc_table[256];
static void crc_init()
{
    for (u32 i = 0; i < 256; ++i)
    {
        u32 c = i;
        for (int k = 0; k < 8; ++k) c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        g_crc_table[i] = c;
    }
}
static u32 crc32_buf(const u8* p, size_t n)
{
    u32 c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; ++i) c = g_crc_table[(c ^ p[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

static u64 hash64(const u8* p, size_t n)
{
    u64 h = 0x9E3779B97F4A7C15ull ^ u64(n);
    size_t left = n;
    while (left >= 8)
    {
        u64 v;
        memcpy(&v, p, 8);
        h ^= v * 0xff51afd7ed558ccdull;
        h = ((h << 31) | (h >> 33)) * 0xc4ceb9fe1a85ec53ull;
        p += 8;
        left -= 8;
    }
    u64 v = 0;
    memcpy(&v, p, left);
    h ^= v * 0xff51afd7ed558ccdull;
    h ^= h >> 29;
    return h;
}

static bool wildcard(const char* pat, const char* s)
{
    if (!*pat) return !*s;
    if (*pat == '*')
    {
        for (const char* t = s;; ++t)
        {
            if (wildcard(pat + 1, t)) return true;
            if (!*t) return false;
        }
    }
    if (*s && (*pat == '?' || *pat == *s)) return wildcard(pat + 1, s + 1);
    return false;
}

static std::vector<std::string> split(const std::string& s, char d)
{
    std::vector<std::string> r;
    size_t a = 0;
    for (;;)
    {
        size_t b = s.find(d, a);
        std::string t = trim(s.substr(a, b == std::string::npos ? b : b - a));
        if (!t.empty()) r.push_back(t);
        if (b == std::string::npos) break;
        a = b + 1;
    }
    return r;
}

// ------------------------------------------------------------------ ltx

struct Ltx
{
    std::vector<std::pair<std::string, std::vector<std::pair<std::string, std::string>>>> sections;

    std::vector<std::pair<std::string, std::string>>* find(const std::string& n)
    {
        for (auto& s : sections) if (s.first == n) return &s.second;
        return nullptr;
    }
    std::string get(const std::string& sec, const std::string& key, const std::string& def = "")
    {
        auto* s = find(sec);
        if (!s) return def;
        for (auto& kv : *s) if (kv.first == key) return kv.second;
        return def;
    }
};

static bool load_ltx(const std::string& path, Ltx& out)
{
    FILE* f = fopen_p(sfs::u8path(path), "rb");
    if (!f) return false;
    std::string data;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) data.append(buf, n);
    fclose(f);
    if (data.size() >= 3 && (u8)data[0] == 0xEF && (u8)data[1] == 0xBB && (u8)data[2] == 0xBF) data.erase(0, 3);

    size_t pos = 0;
    while (pos <= data.size())
    {
        size_t e = data.find('\n', pos);
        std::string line = data.substr(pos, e == std::string::npos ? std::string::npos : e - pos);
        pos = (e == std::string::npos) ? data.size() + 1 : e + 1;
        // strip comments (';' and '//') outside of quotes
        bool q = false;
        for (size_t i = 0; i < line.size(); ++i)
        {
            if (line[i] == '"') q = !q;
            if (!q && (line[i] == ';' || (line[i] == '/' && i + 1 < line.size() && line[i + 1] == '/')))
            {
                line.erase(i);
                break;
            }
        }
        line = trim(line);
        if (line.empty()) continue;
        if (line[0] == '[')
        {
            size_t b = line.find(']');
            if (b == std::string::npos) continue;
            out.sections.push_back({trim(line.substr(1, b - 1)), {}});
            continue;
        }
        if (out.sections.empty()) continue;
        size_t eq = line.find('=');
        std::string k = trim(eq == std::string::npos ? line : line.substr(0, eq));
        std::string v = eq == std::string::npos ? "" : trim(line.substr(eq + 1));
        out.sections.back().second.push_back({k, v});
    }
    return true;
}

// ------------------------------------------------------------------ job list

struct Src
{
    std::string rel;     // lowercase, backslashes
    sfs::path full;
    u64 size = 0;
    bool store = false;  // must be kept uncompressed (streamed by the engine)
};

static bool is_true(const std::string& v)
{
    std::string l = lower(v);
    return l == "true" || l == "1" || l == "yes" || l == "on" || l.empty();
}

static std::string to_rel(const sfs::path& root, const sfs::path& p)
{
    std::string r = sfs::relative(p, root).generic_u8string();
    for (auto& c : r) if (c == '/') c = '\\';
    return lower(r);
}

struct Options
{
    std::string folder, ltx, out;
    bool xdb = false, fast = false, store = false, verify = true, verbose = false;
    u64 max_size_mb = 1900;
    unsigned threads = 0;
    u64 mem_mb = 1024;
    std::vector<std::string> check;
};

static bool collect(const Options& o, Ltx& ltx, std::vector<Src>& files)
{
    sfs::path root = sfs::u8path(o.folder);
    std::vector<std::pair<std::string, bool>> inc;
    std::vector<std::string> exc_exts, exc_files, exc_folders, inc_files, store_pats;

    if (!o.ltx.empty())
    {
        if (auto* s = ltx.find("include_folders")) for (auto& kv : *s) inc.push_back({kv.first, is_true(kv.second)});
        if (auto* s = ltx.find("exclude_folders")) for (auto& kv : *s) exc_folders.push_back(lower(kv.first));
        if (auto* s = ltx.find("include_files")) for (auto& kv : *s) inc_files.push_back(lower(kv.first));
        if (auto* s = ltx.find("exclude_files")) for (auto& kv : *s) exc_files.push_back(lower(kv.first));
        exc_exts = split(lower(ltx.get("options", "exclude_exts")), ',');
        if (is_true(ltx.get("options", "store_defaults", "true")))
            for (const char* d : {"*.geom", "*.geomx", "*.ogm", "*.ogg", "level.ai", "game.graph"}) store_pats.push_back(d);
        for (auto& e : split(lower(ltx.get("options", "store_exts")), ',')) store_pats.push_back(e);
        if (auto* sf = ltx.find("store_files")) for (auto& kv : *sf) store_pats.push_back(lower(kv.first));
        for (auto& sec : ltx.sections)
        {
            const std::string& n = sec.first;
            if (n != "options" && n != "include_folders" && n != "exclude_folders" && n != "include_files" &&
                n != "exclude_files" && n != "store_files" && n != "header")
                printf("! note: ltx section [%s] is ignored\n", n.c_str());
        }
    }
    if (o.ltx.empty())
        for (const char* d : {"*.geom", "*.geomx", "*.ogm", "*.ogg", "level.ai", "game.graph"}) store_pats.push_back(d);
    if (inc.empty() && inc_files.empty()) inc.push_back({".\\", true});

    std::map<std::string, Src> uniq;
    auto excluded = [&](const std::string& rel) {
        std::string name = rel.substr(rel.find_last_of('\\') == std::string::npos ? 0 : rel.find_last_of('\\') + 1);
        for (auto& p : exc_exts)
            if (wildcard(p.c_str(), name.c_str())) return true;
        for (auto& p : exc_files)
            if (wildcard(p.c_str(), name.c_str()) || wildcard(p.c_str(), rel.c_str())) return true;
        for (auto& p : exc_folders)
        {
            std::string pp = p;
            if (!pp.empty() && pp.back() != '\\') pp += '\\';
            if (rel.compare(0, pp.size(), pp) == 0) return true;
            if (rel.find("\\" + pp) != std::string::npos) return true;
        }
        return false;
    };
    auto add = [&](const sfs::path& p) {
        std::error_code ec;
        u64 sz = sfs::file_size(p, ec);
        if (ec) { printf("! cannot stat %s\n", p.u8string().c_str()); return; }
        std::string rel = to_rel(root, p);
        if (excluded(rel)) return;
        std::string nm = rel.substr(rel.find_last_of('\\') == std::string::npos ? 0 : rel.find_last_of('\\') + 1);
        bool st = false;
        for (auto& sp : store_pats)
            if (wildcard(sp.c_str(), nm.c_str()) || wildcard(sp.c_str(), rel.c_str())) { st = true; break; }
        uniq[rel] = Src{rel, p, sz, st};
    };

    for (auto& inf : inc)
    {
        std::string k = inf.first;
        for (auto& c : k) if (c == '\\') c = '/';
        sfs::path d = (k == "./" || k == ".") ? root : root / sfs::u8path(k);
        std::error_code ec;
        if (!sfs::is_directory(d, ec)) { printf("! include folder not found: %s\n", d.u8string().c_str()); continue; }
        if (inf.second)
        {
            for (sfs::recursive_directory_iterator it(d, sfs::directory_options::skip_permission_denied, ec), end; it != end; it.increment(ec))
                if (!ec && it->is_regular_file(ec)) add(it->path());
        }
        else
        {
            for (sfs::directory_iterator it(d, ec), end; it != end; it.increment(ec))
                if (!ec && it->is_regular_file(ec)) add(it->path());
        }
    }
    for (auto& f : inc_files)
    {
        std::string k = f;
        for (auto& c : k) if (c == '\\') c = '/';
        std::error_code ec;
        sfs::path p = root / sfs::u8path(k);
        if (sfs::is_regular_file(p, ec)) add(p);
        else printf("! include file not found: %s\n", p.u8string().c_str());
    }
    for (auto& kv : uniq) files.push_back(kv.second);
    return true;
}

// ------------------------------------------------------------------ compression pipeline

struct Result
{
    std::vector<u8> data;   // bytes to be written (compressed or raw)
    u32 real = 0, comp = 0, crc = 0;
    u64 hash = 0;
    bool done = false, fail = false;
    std::string err;
};

struct Pipeline
{
    const std::vector<Src>& files;
    const Options& opt;
    std::vector<Result> res;
    std::mutex m;
    std::condition_variable cv;
    size_t next = 0, written = 0;
    u64 inflight = 0, cap = 0;
    size_t window = 0;
    bool abort = false;
    std::vector<std::thread> pool;

    Pipeline(const std::vector<Src>& f, const Options& o) : files(f), opt(o), res(f.size()) {}

    void worker()
    {
        std::vector<lzo_align_t> wrk((LZO1X_999_MEM_COMPRESS + sizeof(lzo_align_t) - 1) / sizeof(lzo_align_t) + 4);
        for (;;)
        {
            size_t idx;
            {
                std::unique_lock<std::mutex> lk(m);
                for (;;)
                {
                    if (abort || next >= files.size()) return;
                    bool head = (next == written);
                    if (head || (next - written < window && inflight + files[next].size <= cap)) break;
                    cv.wait(lk);
                }
                idx = next++;
                inflight += files[idx].size;
            }
            Result r;
            const Src& s = files[idx];
            do
            {
                if (s.size > 0xFFFFFFF0ull) { r.fail = true; r.err = "file is larger than 4 GB: " + s.rel; break; }
                std::vector<u8> raw(size_t(s.size));
                FILE* f = fopen_p(s.full, "rb");
                if (!f) { r.fail = true; r.err = "cannot open " + s.full.u8string(); break; }
                size_t got = raw.empty() ? 0 : fread(raw.data(), 1, raw.size(), f);
                fclose(f);
                if (got != raw.size()) { r.fail = true; r.err = "read error (" + std::to_string(got) + "/" + std::to_string(raw.size()) + ") " + s.rel; break; }
                r.real = u32(raw.size());
                r.crc = crc32_buf(raw.data(), raw.size());
                r.hash = hash64(raw.data(), raw.size());
                bool stored = opt.store || s.store || raw.empty();
                if (!stored)
                {
                    std::vector<u8> out(raw.size() + raw.size() / 16 + 64 + 3);
                    lzo_uint osz = out.size();
                    int rc = opt.fast ? lzo1x_1_compress(raw.data(), raw.size(), out.data(), &osz, wrk.data())
                                      : lzo1x_999_compress(raw.data(), raw.size(), out.data(), &osz, wrk.data());
                    if (rc != 0) { r.fail = true; r.err = "lzo error on " + s.rel; break; }
                    if (osz < raw.size())
                    {
                        out.resize(osz);
                        r.comp = u32(osz);
                        r.data.swap(out);
                    }
                    else stored = true;
                }
                if (stored)
                {
                    r.comp = r.real;
                    r.data.swap(raw);
                }
            } while (false);
            r.done = true;
            {
                std::lock_guard<std::mutex> lk(m);
                res[idx] = std::move(r);
            }
            cv.notify_all();
        }
    }

    void start(unsigned n)
    {
        for (unsigned i = 0; i < n; ++i) pool.emplace_back([this] { worker(); });
    }
    Result take(size_t i)
    {
        std::unique_lock<std::mutex> lk(m);
        cv.wait(lk, [&] { return res[i].done; });
        Result r = std::move(res[i]);
        return r;
    }
    void release(size_t i)
    {
        {
            std::lock_guard<std::mutex> lk(m);
            written = i + 1;
            inflight -= files[i].size;
        }
        cv.notify_all();
    }
    void stop()
    {
        {
            std::lock_guard<std::mutex> lk(m);
            abort = true;
        }
        cv.notify_all();
        for (auto& t : pool) t.join();
        pool.clear();
    }
};

// ------------------------------------------------------------------ archive writer

struct Entry
{
    std::string name;
    u32 real = 0, comp = 0, crc = 0, ptr = 0;
};

static char index_char(unsigned i)
{
    if (i < 10) return char('0' + i);
    if (i < 36) return char('a' + (i - 10));
    return 0;
}

static void put32(std::vector<u8>& v, u32 x) { v.insert(v.end(), (u8*)&x, (u8*)&x + 4); }
static void put16(std::vector<u8>& v, u16 x) { v.insert(v.end(), (u8*)&x, (u8*)&x + 2); }

static std::string build_header(Ltx& ltx)
{
    // The header chunk is ALWAYS written: OpenXRay treats an archive without it as an encrypted ShoC .db.
    std::map<std::string, std::string> sorted;
    if (auto* s = ltx.find("header"))
        for (auto& kv : *s) sorted[kv.first] = kv.second;
    if (!sorted.count("auto_load")) { sorted["auto_load"] = "true"; printf("! note: [header] auto_load missing, using true\n"); }
    if (!sorted.count("entry_point")) { sorted["entry_point"] = "$fs_root$\\gamedata\\"; printf("! note: [header] entry_point missing, using $fs_root$\\gamedata\\\n"); }
    std::string r = "[header]\r\n";
    for (auto& kv : sorted) r += kv.first + " = " + kv.second + "\r\n";
    return r;
}

struct Writer
{
    std::string base;          // output path incl. extension (.xdb / .db)
    std::string header;
    u64 limit = 0;
    FILE* f = nullptr;
    unsigned idx = 0;
    std::string cur_path;
    u64 pos = 0, data_start = 0, chunk0_pos = 0, table_est = 0;
    std::vector<Entry> entries;
    std::set<std::string> dirs;
    std::map<std::tuple<u32, u32, u64>, std::pair<u32, u32>> dedup;   // (real,crc,hash) -> (ptr,comp)
    std::vector<std::string> done_paths;

    bool open_next()
    {
        char ic = index_char(idx);
        if (!ic) { printf("! too many archives (36 max)\n"); return false; }
        cur_path = base + ic;
        ++idx;
        {
            std::error_code ec;
            sfs::path parent = sfs::u8path(cur_path).parent_path();
            if (!parent.empty()) sfs::create_directories(parent, ec);
        }
        f = fopen_p(sfs::u8path(cur_path), "wb");
        if (!f) { printf("! cannot create %s\n", cur_path.c_str()); return false; }
        static char big[1 << 20];
        setvbuf(f, big, _IOFBF, sizeof big);
        pos = 0;
        entries.clear();
        dirs.clear();
        dedup.clear();
        table_est = 64;
        if (!header.empty())
        {
            u32 t = 666, s = u32(header.size());
            fwrite(&t, 4, 1, f);
            fwrite(&s, 4, 1, f);
            fwrite(header.data(), 1, header.size(), f);
            pos += 8 + header.size();
        }
        chunk0_pos = pos;
        u32 t0 = 0, s0 = 0;
        fwrite(&t0, 4, 1, f);
        fwrite(&s0, 4, 1, f);
        pos += 8;
        data_start = pos;
        return true;
    }

    bool close_current()
    {
        if (!f) return true;
        u64 dsz = pos - data_start;
        if (dsz > 0xFFFFFFFFull) { printf("! archive data exceeds 4 GB\n"); return false; }
        // table
        std::vector<u8> raw;
        auto rec = [&](const Entry& e) {
            put16(raw, u16(16 + e.name.size()));
            put32(raw, e.real);
            put32(raw, e.comp);
            put32(raw, e.crc);
            raw.insert(raw.end(), e.name.begin(), e.name.end());
            put32(raw, e.ptr);
        };
        for (auto& d : dirs) rec(Entry{d, 0, 0, 0, 0});
        for (auto& e : entries) rec(e);
        u8* cdata = nullptr;
        unsigned csz = 0;
        _compressLZ(&cdata, &csz, raw.data(), unsigned(raw.size()));
        u32 t1 = 0x80000001u, s1 = csz;
        fwrite(&t1, 4, 1, f);
        fwrite(&s1, 4, 1, f);
        fwrite(cdata, 1, csz, f);
        free(cdata);
        pos += 8 + csz;
        if (pos < raw.size() + 64)
        {
            // OpenXRay/OGSR reject a table whose unpacked size exceeds the archive size: pad with an ignored chunk
            u32 pad = u32(raw.size() + 64 - pos);
            u32 tp = 3, sp = pad;
            std::vector<u8> zeros(pad, 0);
            fwrite(&tp, 4, 1, f);
            fwrite(&sp, 4, 1, f);
            fwrite(zeros.data(), 1, pad, f);
            pos += 8 + pad;
        }
        u32 ds = u32(dsz);
        fflush(f);
        seek64(f, (long long)(chunk0_pos + 4), SEEK_SET);
        fwrite(&ds, 4, 1, f);
        bool ok = fclose(f) == 0;
        f = nullptr;
        if (pos > 0xFFFFFFFFull) { printf("! archive exceeds 4 GB\n"); return false; }
        if (!ok) { printf("! write error (disk full?) on %s\n", cur_path.c_str()); return false; }
        done_paths.push_back(cur_path);
        printf("\n  -> %s  %.1f MB, %zu files\n", cur_path.c_str(), double(pos) / 1048576.0, entries.size());
        return true;
    }

    // returns false on fatal error
    bool add(const std::string& rel, const Result& r)
    {
        auto key = std::make_tuple(r.real, r.crc, r.hash);
        bool dup = false;
        std::pair<u32, u32> dv{0, 0};
        if (f)
        {
            auto it = dedup.find(key);
            if (it != dedup.end() && r.real > 0) { dup = true; dv = it->second; }
        }
        u64 add_bytes = dup ? 0 : r.data.size();
        u64 name_bytes = rel.size() + 24;
        if (!f || (!entries.empty() && pos + add_bytes + table_est + name_bytes + 64 > limit))
        {
            if (f && !close_current()) return false;
            if (!open_next()) return false;
            dup = false;
            add_bytes = r.data.size();
        }
        Entry e;
        e.name = rel;
        e.real = r.real;
        e.crc = r.crc;
        if (dup)
        {
            e.ptr = dv.first;
            e.comp = dv.second;
        }
        else
        {
            e.ptr = u32(pos);
            e.comp = r.comp;
            if (!r.data.empty() && fwrite(r.data.data(), 1, r.data.size(), f) != r.data.size())
            {
                printf("! write error (disk full?) on %s\n", cur_path.c_str());
                return false;
            }
            pos += r.data.size();
            if (r.real > 0) dedup[key] = {e.ptr, e.comp};
        }
        table_est += name_bytes;
        entries.push_back(e);
        for (size_t p = rel.find('\\'); p != std::string::npos; p = rel.find('\\', p + 1))
        {
            std::string d = rel.substr(0, p + 1);
            if (dirs.insert(d).second) table_est += d.size() + 24;
        }
        return true;
    }
};

// ------------------------------------------------------------------ checker

static bool read_all(const std::string& path, std::vector<u8>& out)
{
    FILE* f = fopen_p(sfs::u8path(path), "rb");
    if (!f) return false;
    seek64(f, 0, SEEK_END);
    long long n = tell64(f);
    seek64(f, 0, SEEK_SET);
    out.resize(size_t(n));
    size_t got = n ? fread(out.data(), 1, size_t(n), f) : 0;
    fclose(f);
    return got == size_t(n);
}

// returns number of problems (0 = archive is fine)
static int check_archive(const std::string& path, unsigned threads, bool quiet)
{
    std::vector<u8> d;
    if (!read_all(path, d)) { printf("[CHECK] %s: cannot read file\n", path.c_str()); return 1; }
    int problems = 0;
    auto bad = [&](const char* fmt, auto... a) {
        ++problems;
        if (problems <= 20) { printf("[CHECK] %s: ", path.c_str()); printf(fmt, a...); printf("\n"); }
    };
    u64 p = 0;
    bool have_table = false;
    u64 table_off = 0, table_size = 0;
    u64 data_lo = 0, data_hi = 0;
    while (p < d.size())
    {
        if (p + 8 > d.size()) { bad("trailing %llu bytes: chunk header truncated at %llu", (unsigned long long)(d.size() - p), (unsigned long long)p); break; }
        u32 t, s;
        memcpy(&t, &d[p], 4);
        memcpy(&s, &d[p + 4], 4);
        u32 id = t & 0x7FFFFFFFu;
        if (p + 8 + u64(s) > d.size())
        {
            bad("chunk %u at %llu declares %u bytes but only %llu remain (file truncated or size field overflowed)", id,
                (unsigned long long)p, s, (unsigned long long)(d.size() - p - 8));
            break;
        }
        if (id == 0) { data_lo = p + 8; data_hi = p + 8 + s; }
        if (id == 1) { have_table = true; table_off = p + 8; table_size = s; }
        p += 8 + u64(s);
    }
    if (!have_table) { bad("no file table chunk found"); return problems; }
    {
        bool has_hdr = false;
        for (u64 q2 = 0; q2 + 8 <= d.size();)
        {
            u32 t2, s2;
            memcpy(&t2, &d[q2], 4);
            memcpy(&s2, &d[q2 + 4], 4);
            if ((t2 & 0x7FFFFFFFu) == 666) has_hdr = true;
            q2 += 8 + u64(s2);
        }
        std::string lp = lower(path);
        bool is_xdb = lp.find(".xdb") != std::string::npos;
        if (!has_hdr && !is_xdb) printf("[CHECK] %s: WARNING no [header] chunk in a .db archive: OpenXRay will treat it as an encrypted ShoC archive and fail\n", path.c_str());
        if (d.size() >= 0x80000000ull) printf("[CHECK] %s: WARNING archive is >= 2 GiB: OpenXRay cannot read it (size is cast to int)\n", path.c_str());
        if (table_size >= 4)
        {
            u32 ts = u32(d[table_off]) | u32(d[table_off + 1]) << 8 | u32(d[table_off + 2]) << 16 | u32(d[table_off + 3]) << 24;
            if (u64(ts) > d.size()) printf("[CHECK] %s: WARNING unpacked table (%u) is larger than the archive: OpenXRay/OGSR will reject it\n", path.c_str(), ts);
        }
    }

    u8* tb = nullptr;
    unsigned tsz = 0;
    _decompressLZ(&tb, &tsz, &d[table_off], unsigned(table_size));
    struct Rec { std::string name; u32 real, comp, crc, ptr; };
    std::vector<Rec> recs;
    u64 q = 0;
    while (q + 2 <= tsz)
    {
        u16 bs;
        memcpy(&bs, tb + q, 2);
        q += 2;
        if (bs < 16 || q + bs > tsz) { bad("corrupt table record at table offset %llu", (unsigned long long)q); break; }
        Rec r;
        memcpy(&r.real, tb + q, 4);
        memcpy(&r.comp, tb + q + 4, 4);
        memcpy(&r.crc, tb + q + 8, 4);
        r.name.assign((char*)tb + q + 12, bs - 16);
        memcpy(&r.ptr, tb + q + bs - 4, 4);
        q += bs;
        recs.push_back(std::move(r));
    }
    free(tb);

    std::vector<size_t> todo;
    std::set<std::tuple<u32, u32, u32, u32>> seen;
    for (size_t i = 0; i < recs.size(); ++i)
    {
        auto& r = recs[i];
        if (r.real == 0 && r.comp == 0 && r.ptr == 0) continue;   // directory
        if (!seen.insert({r.ptr, r.comp, r.real, r.crc}).second) continue;
        if (u64(r.ptr) < data_lo || u64(r.ptr) + r.comp > data_hi)
        {
            bad("'%s' ptr=%u size=%u lies outside of the data chunk [%llu..%llu]", r.name.c_str(), r.ptr, r.comp,
                (unsigned long long)data_lo, (unsigned long long)data_hi);
            continue;
        }
        todo.push_back(i);
    }
    std::atomic<size_t> cursor{0};
    std::atomic<int> errs{0};
    std::mutex pm;
    auto work = [&] {
        std::vector<u8> buf;
        for (;;)
        {
            size_t k = cursor++;
            if (k >= todo.size()) return;
            const Rec& r = recs[todo[k]];
            const u8* src = &d[r.ptr];
            u32 crc;
            if (r.comp == r.real) crc = crc32_buf(src, r.real);
            else
            {
                buf.resize(r.real);
                lzo_uint os = r.real;
                int rc = lzo1x_decompress_safe(src, r.comp, buf.data(), &os, nullptr);
                if (rc != 0 || os != r.real)
                {
                    std::lock_guard<std::mutex> lk(pm);
                    ++errs;
                    if (errs <= 20) printf("[CHECK] %s: '%s' lzo decompress failed (rc=%d out=%lu expected=%u)\n", path.c_str(), r.name.c_str(), rc, (unsigned long)os, r.real);
                    continue;
                }
                crc = crc32_buf(buf.data(), buf.size());
            }
            if (crc != r.crc)
            {
                std::lock_guard<std::mutex> lk(pm);
                ++errs;
                if (errs <= 20) printf("[CHECK] %s: '%s' crc mismatch (%08x != %08x)\n", path.c_str(), r.name.c_str(), crc, r.crc);
            }
        }
    };
    std::vector<std::thread> th;
    for (unsigned i = 0; i < std::max(1u, threads); ++i) th.emplace_back(work);
    for (auto& t : th) t.join();
    problems += errs;
    if (!quiet || problems)
        printf("[CHECK] %s: %s (%zu records, %zu unique blobs verified)\n", path.c_str(), problems ? "PROBLEMS FOUND" : "OK", recs.size(), todo.size());
    return problems;
}

// extracts every file of an archive into outdir (used for round-trip tests)
static int unpack_archive(const std::string& path, const std::string& outdir)
{
    std::vector<u8> d;
    if (!read_all(path, d)) { printf("cannot read %s\n", path.c_str()); return 1; }
    u64 p = 0, table_off = 0, table_size = 0;
    bool have = false;
    while (p + 8 <= d.size())
    {
        u32 t, s;
        memcpy(&t, &d[p], 4);
        memcpy(&s, &d[p + 4], 4);
        if (p + 8 + u64(s) > d.size()) break;
        if ((t & 0x7FFFFFFFu) == 1) { have = true; table_off = p + 8; table_size = s; }
        p += 8 + u64(s);
    }
    if (!have) { printf("no table\n"); return 1; }
    u8* tb = nullptr;
    unsigned tsz = 0;
    _decompressLZ(&tb, &tsz, &d[table_off], unsigned(table_size));
    u64 q = 0;
    int n = 0, err = 0;
    while (q + 2 <= tsz)
    {
        u16 bs;
        memcpy(&bs, tb + q, 2);
        q += 2;
        u32 real, comp, crc, ptr;
        memcpy(&real, tb + q, 4);
        memcpy(&comp, tb + q + 4, 4);
        memcpy(&crc, tb + q + 8, 4);
        std::string name((char*)tb + q + 12, bs - 16);
        memcpy(&ptr, tb + q + bs - 4, 4);
        q += bs;
        if (!name.empty() && name.back() == '\\') continue;
        for (auto& c : name) if (c == '\\') c = '/';
        sfs::path out = sfs::u8path(outdir) / sfs::u8path(name);
        std::error_code ec;
        sfs::create_directories(out.parent_path(), ec);
        std::vector<u8> buf(real);
        if (comp == real) { if (real) memcpy(buf.data(), &d[ptr], real); }
        else
        {
            lzo_uint os = real;
            if (lzo1x_decompress_safe(&d[ptr], comp, buf.data(), &os, nullptr) != 0 || os != real) { printf("bad: %s\n", name.c_str()); ++err; continue; }
        }
        FILE* f = fopen_p(out, "wb");
        if (!f) { ++err; continue; }
        if (real) fwrite(buf.data(), 1, real, f);
        fclose(f);
        ++n;
    }
    free(tb);
    printf("unpacked %d files, %d errors\n", n, err);
    return err ? 1 : 0;
}

// ------------------------------------------------------------------ main

static void usage()
{
    printf(
        "xrCompressorMT - multithreaded x64 packer for X-Ray .db/.xdb archives\n"
        "Compatibility: COC, COP, Monolith, OpenXRay, OGSR (.xdb in the mod folder), SOC\n\n"
        "  xrCompressorMT <folder> [-ltx file.ltx] [-xdb] [-max_size MB] [-fast | -store] -out <name.xdb>\n"
        "               [-threads N] [-mem MB] [-noverify] [-v]\n"
        "  xrCompressorMT -check <archive> [archive ...]     verify existing archives (CRC of every file)\n\n"
        "  -max_size  per-archive limit in MB (default 1900, hard cap 2047)\n"
        "  -fast      lzo1x_1 instead of lzo1x_999 (much faster, larger)\n"
        "  -store     no compression\n"
        "  -threads   worker threads (default: all logical cores)\n"
        "  -mem       max MB of file data held in memory by the pipeline (default 1024)\n"
        "  -noverify  skip the automatic check after writing each archive\n");
}

#ifdef XR_EMBED
int packer_main(int argc, char** argv)
#else
int main(int argc, char** argv)
#endif
{
#ifdef _WIN32
    // arguments as UTF-8 (the narrow argv uses the ANSI code page and breaks Cyrillic paths)
    std::vector<std::string> u8args;
    std::vector<char*> u8ptrs;
    {
        int n = 0;
        wchar_t** w = CommandLineToArgvW(GetCommandLineW(), &n);
        if (w)
        {
            for (int i = 0; i < n; ++i)
            {
                int len = WideCharToMultiByte(65001, 0, w[i], -1, nullptr, 0, nullptr, nullptr);
                std::string t(len > 0 ? len - 1 : 0, '\0');
                if (len > 1) WideCharToMultiByte(65001, 0, w[i], -1, &t[0], len, nullptr, nullptr);
                u8args.push_back(t);
            }
            for (auto& t : u8args) u8ptrs.push_back(&t[0]);
            argc = n;
            argv = u8ptrs.data();
        }
        SetConsoleOutputCP(65001);
    }
#endif
    setvbuf(stdout, nullptr, _IONBF, 0);   // lines must reach a GUI pipe immediately
    Options o;
    bool check_mode = false, unpack_mode = false;
    for (int i = 1; i < argc; ++i)
    {
        std::string a = argv[i];
        auto need = [&](const char* n) -> std::string {
            if (i + 1 >= argc) { printf("! %s needs a value\n", n); exit(2); }
            return argv[++i];
        };
        if (a == "-ltx") o.ltx = need("-ltx");
        else if (a == "-out") o.out = need("-out");
        else if (a == "-xdb") o.xdb = true;
        else if (a == "-fast") o.fast = true;
        else if (a == "-store") o.store = true;
        else if (a == "-noverify") o.verify = false;
        else if (a == "-v") o.verbose = true;
        else if (a == "-nosize") {}
        else if (a == "-max_size") o.max_size_mb = strtoull(need("-max_size").c_str(), nullptr, 10);
        else if (a == "-threads") o.threads = unsigned(strtoul(need("-threads").c_str(), nullptr, 10));
        else if (a == "-mem") o.mem_mb = strtoull(need("-mem").c_str(), nullptr, 10);
        else if (a == "-check") check_mode = true;
        else if (a == "-unpack") unpack_mode = true;
        else if (a == "-h" || a == "-help" || a == "--help") { usage(); return 0; }
        else if (!a.empty() && a[0] == '-') printf("! unknown option %s ignored\n", a.c_str());
        else if (check_mode || unpack_mode) o.check.push_back(a);
        else if (o.folder.empty()) o.folder = a;
        else printf("! extra argument %s ignored\n", a.c_str());
    }

    crc_init();
    lzo_init();
    unsigned hw = std::thread::hardware_concurrency();
    if (!hw) hw = 4;
    if (!o.threads) o.threads = hw;
    o.threads = std::min(o.threads, 64u);

    if (unpack_mode)
    {
        if (o.check.size() != 2) { printf("usage: -unpack <archive> <outdir>\n"); return 2; }
        return unpack_archive(o.check[0], o.check[1]);
    }
    if (check_mode)
    {
        if (o.check.empty()) { usage(); return 2; }
        int bad = 0;
        for (auto& p : o.check) bad += check_archive(p, o.threads, false) ? 1 : 0;
        return bad ? 1 : 0;
    }

    if (o.folder.empty() || o.out.empty()) { usage(); return 2; }
    if (!sfs::is_directory(sfs::u8path(o.folder))) { printf("ERROR: folder not found: %s\n", o.folder.c_str()); return 1; }
    if (o.max_size_mb < 16) { printf("! incorrect value max_size\n"); return 2; }
    if (o.max_size_mb > 2047)
    {
        printf("! max_size %llu MB clamped to 2047: OpenXRay fails to read archives >= 2 GiB\n", (unsigned long long)o.max_size_mb);
        o.max_size_mb = 2047;
    }

    Ltx ltx;
    if (!o.ltx.empty() && !load_ltx(o.ltx, ltx)) { printf("ERROR: Unable to open file list: %s\n", o.ltx.c_str()); return 1; }

    std::vector<Src> files;
    collect(o, ltx, files);
    if (files.empty()) { printf("ERROR: nothing to pack\n"); return 1; }
    u64 total_in = 0;
    for (auto& f : files) total_in += f.size;
    { size_t nst = 0; for (auto& f : files) if (f.store) ++nst; if (nst) printf("Kept uncompressed (streamed by the engine): %zu file(s)\n", nst); }
    printf("Files: %zu, %.1f MB. Threads: %u. Mode: %s. Archive limit: %llu MB\n", files.size(), total_in / 1048576.0, o.threads,
           o.store ? "store" : (o.fast ? "lzo1x_1 (fast)" : "lzo1x_999"), (unsigned long long)o.max_size_mb);

    Writer w;
    w.base = o.out;
    {
        std::string l = lower(w.base);
        std::string ext = o.xdb ? ".xdb" : ".db";
        if (l.size() < ext.size() || l.compare(l.size() - ext.size(), ext.size(), ext) != 0) w.base += ext;
    }
    w.header = build_header(ltx);
    w.limit = o.max_size_mb * 1024ull * 1024ull;

    Pipeline pl(files, o);
    pl.cap = std::max<u64>(o.mem_mb, 64) * 1024ull * 1024ull;
    pl.window = o.threads * 3;
    pl.start(o.threads);

    auto t0 = Clock::now();
    auto last = t0;
    u64 done_in = 0;
    bool fatal = false;
    for (size_t i = 0; i < files.size(); ++i)
    {
        Result r = pl.take(i);
        if (r.fail)
        {
            printf("\nERROR: %s\n", r.err.c_str());
            fatal = true;
            break;
        }
        size_t before = w.entries.size();
        unsigned before_idx = w.idx;
        if (!w.add(files[i].rel, r)) { fatal = true; break; }
        (void)before; (void)before_idx;
        if (o.verbose) printf("%s  %u -> %u\n", files[i].rel.c_str(), r.real, r.comp);
        done_in += files[i].size;
        pl.release(i);
        auto now = Clock::now();
        if (std::chrono::duration<double>(now - last).count() > 0.5)
        {
            last = now;
            double el = std::chrono::duration<double>(now - t0).count();
            printf("\r  %zu/%zu files  %.1f/%.1f MB  %.1f MB/s   ", i + 1, files.size(), done_in / 1048576.0, total_in / 1048576.0,
                   done_in / 1048576.0 / el);
            fflush(stdout);
        }
    }
    pl.stop();
    if (!fatal && !w.close_current()) fatal = true;
    if (w.f) { fclose(w.f); w.f = nullptr; }
    if (fatal)
    {
        printf("\nFAILED. Archives written before the error may be incomplete: do not use them.\n");
        return 1;
    }

    double el = std::chrono::duration<double>(Clock::now() - t0).count();
    u64 total_out = 0;
    for (auto& p : w.done_paths)
    {
        std::error_code ec;
        total_out += sfs::file_size(sfs::u8path(p), ec);
    }
    printf("\nPacked %.1f MB -> %.1f MB in %.1f s (%zu archive(s))\n", total_in / 1048576.0, total_out / 1048576.0, el, w.done_paths.size());

    int bad = 0;
    if (o.verify)
    {
        printf("Verifying archives...\n");
        for (auto& p : w.done_paths) bad += check_archive(p, o.threads, false) ? 1 : 0;
        if (bad) { printf("VERIFY FAILED for %d archive(s)!\n", bad); return 1; }
        printf("All archives verified OK.\n");
    }
    return 0;
}
