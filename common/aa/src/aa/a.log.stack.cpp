#include "ahcpp.h"

#include "a.log.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <cinttypes>

// 平台头
#ifdef _WIN32
#   include <windows.h>
#   include <dbghelp.h>
#   include <psapi.h>
#   pragma comment(lib, "dbghelp.lib")
#else
#   include <cxxabi.h>
#   include <dlfcn.h>
#   include <execinfo.h>
#include <unistd.h>
#endif

namespace alog
{
    namespace
    {
        // ------------------------------------------------------------------------------// 模块数据对象（仅数据）
        // ------------------------------------------------------------------------------
        class mdata
        {
        public:
            mdata()  = default;
            ~mdata() = default;

            std::unordered_map<std::string, std::uint64_t> mod_base_map;
            std::mutex                                     mtx;
        };

        mdata md_;

        // ------------------------------------------------------------------------------// 工具
        // ------------------------------------------------------------------------------
        inline std::string to_hex(std::uint64_t v)
        {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%" PRIx64, v);
            return buf;
        }

        inline std::uint64_t from_hex(std::string_view sv)
        {
            return std::strtoull(sv.data(), nullptr, 16);
        }

        // ------------------------------------------------------------------------------// Linux 模块基址
        // ------------------------------------------------------------------------------
        #ifndef _WIN32
        std::uint64_t get_module_base(const std::string& name)
        {
            auto* md = &md_;

            {
                std::lock_guard<std::mutex> lock(md->mtx);
                auto it = md->mod_base_map.find(name);
                if (it != md->mod_base_map.end())
                {
                    return it->second;
                }
            }

            std::ifstream maps("/proc/self/maps");
            std::string line;
            std::uint64_t ret = 0;

            while (std::getline(maps, line))
            {
                if (line.find(name) == std::string::npos)
                {
                    continue;
                }

                auto dash = line.find('-');
                if (dash == std::string::npos)
                {
                    continue;
                }

                std::uint64_t base = from_hex(line.substr(0, dash));

                {
                    std::lock_guard<std::mutex> lock(md->mtx);
                    md->mod_base_map.emplace(name, base);
                }

                ret = base;
                break;
            }

            return ret;
        }
        #endif

        // ------------------------------------------------------------------------------// Windows 模块基址
        // ------------------------------------------------------------------------------
        #ifdef _WIN32
        std::uint64_t get_module_base(const std::string& name)
        {
            auto md = &md_;

            {
                std::lock_guard<std::mutex> lock(md->mtx);
                auto it = md->mod_base_map.find(name);
                if (it != md->mod_base_map.end())
                {
                    return it->second;
                }
            }

            HANDLE proc = GetCurrentProcess();
            HMODULE mods[128];
            DWORD needed;
            if (!EnumProcessModules(proc, mods, sizeof(mods), &needed))
            {
                return 0;
            }

            int count = needed / sizeof(HMODULE);
            std::uint64_t ret = 0;

            for (int i = 0; i < count; ++i)
            {
                char buf[MAX_PATH];
                if (!GetModuleFileNameA(mods[i], buf, sizeof(buf)))
                {
                    continue;
                }

                std::string path(buf);
                if (!path.ends_with(name))
                {
                    continue;
                }

                ret = reinterpret_cast<std::uint64_t>(mods[i]);

                {
                    std::lock_guard<std::mutex> lock(md->mtx);
                    md->mod_base_map.emplace(name, ret);
                }
                break;
            }

            return ret;
        }
        #endif

        // 零拷贝切分：start/len 输入，out 输出 token {ptr,len}
        static void split_view(char* start,
                            std::size_t len,
                            std::vector<std::pair<char*, std::size_t>>& out)
        {
            out.clear();
            if (len == 0)
            {
                return;
            }
            char* p = start;
            char* end = start + len;
            char* tok = p;

            auto is_sep = [](char c)
            {
                return c == ' ' || c == '\r' || c == '\n';
            };

            while (p < end)
            {
                if (is_sep(*p))
                {
                    std::size_t tok_len = p - tok;
                    if (tok_len > 0)
                    {
                        out.emplace_back(tok, tok_len);
                    }
                    ++p;
                    tok = p;
                }
                else
                {
                    ++p;
                }
            }
            // 最后一段
            std::size_t tok_len = p - tok;
            if (tok_len > 0)
            {
                out.emplace_back(tok, tok_len);
            }
        }

        static std::size_t append_raw(char* buf,
                                    std::size_t buf_size,
                                    std::size_t offset,
                                    const std::string& raw)
        {
            if (offset >= buf_size)
            {
                return offset;
            }

            // 找最后一个 [ 和 ]
            const char* p0 = raw.c_str();
            const char* p1 = (const char*)memchr(p0, '[', raw.size());
            const char* p2 = (const char*)memchr(p0, ']', raw.size());
            if (!p1 || !p2 || p1 >= p2)
            {
                // 找不到括号 → 原样输出
                std::size_t len = raw.size();
                if (offset + len + 1 < buf_size)
                {
                    std::memcpy(buf + offset, raw.data(), len);
                    offset += len;
                    buf[offset++] = '\n';
                }
                return offset;
            }

            // 提取地址（去掉 []）
            std::size_t addr_len = p2 - p1 - 1;
            const char* addr = p1 + 1;

            // 提取符号部分（去掉末尾 [] 整个）
            std::size_t sym_len = p1 - p0 - 1;
            const char* sym = p0;

            // 写入：<addr>: <symbol>\n
            std::size_t need = addr_len + 2 + sym_len + 1; // ":" + " " + "\n"
            if (offset + need >= buf_size)
            {
                return offset;
            }
            std::memcpy(buf + offset, addr, addr_len);
            offset += addr_len;
            buf[offset++] = ':';
            buf[offset++] = ' ';
            std::memcpy(buf + offset, sym, sym_len);
            offset += sym_len;
            buf[offset++] = '\n';

            return offset;
        }

        static std::size_t append_buf(char* buf,
                                        std::size_t buf_size,
                                        std::size_t offset,
                                        char* src,
                                        std::size_t src_size,
                                        int flag)
        {
            if (offset + src_size >= buf_size)
            {
                return offset;
            }
            
            std::memcpy(buf + offset, src, src_size);
            offset += src_size;
            if (flag== 1)
            {
                buf[offset++] = ' ';
            }
            else if (flag== 2)
            {
                buf[offset++] = '\n';
            }
            return offset;
        }

        void find_valid_addr(char* in_buf, std::size_t in_size, char** out_buf, std::size_t* out_size)
        {
            *out_buf = in_buf;
            *out_size = in_size;

            in_buf += 2;
            in_size -= 2;

            while (in_size--)
            {
                if (*in_buf == '0')
                {
                    in_buf++;
                    continue;
                }        
                *out_size = in_size + 1;
                *out_buf = in_buf;
                return;
            }    
        }

        // ------------------------------------------------------------------------------// Linux：addr2line -a -f -p -i 直接写 buf
        // ------------------------------------------------------------------------------#ifndef _WIN32
        #ifndef _WIN32
        // ---------------------------------------------------------------------------
        // 主实现：逐行切分，正确提取地址、函数、文件+偏移
        // ---------------------------------------------------------------------------
        std::size_t invoke_addr2line_to_buf(char* buf,
                                            std::size_t buf_size,
                                            std::size_t offset,
                                            const std::string& module,
                                            std::uint64_t addr_offset,
                                            const std::string& raw,
                                            const std::string& addr_str)
        {
            if (offset >= buf_size)
            {
                return offset;
            }

            // 命令：去掉 -p，恢复逐行格式
            std::string cmd = "addr2line -a -f -i -p -e " + module + " 0x" + to_hex(addr_offset);
            FILE* p = popen(cmd.c_str(), "r");
            if (!p)
            {
                return append_raw(buf, buf_size, offset, raw);
            }

            std::array<char, 2048> tmp{};
            bool                     got_valid = false;
            std::size_t              written   = offset;
            std::vector<std::pair<char*, std::size_t>> tok; // 零拷贝 token

            while (fgets(tmp.data(), tmp.size(), p))
            {
                std::size_t len = std::strlen(tmp.data());
                if (len > 0 && tmp[len - 1] == '\n')
                {
                    tmp[len - 1] = '\0';
                    --len;
                }
                if (len == 0 || tmp[0] == '?')
                {
                    continue;
                }

                // 零拷贝切分        
                split_view(tmp.data(), len, tok);
                if (tok.empty() || tok.size() < 4)
                {
                    continue;
                }

                auto pair = tok[0];

                // 有冒号 → 新运行时地址
                if (pair.second >= 2 && pair.first[0] == '0' && pair.first[1] == 'x' && pair.first[pair.second-1] == ':')
                {
                    written = append_buf(buf, buf_size, written, (char*)addr_str.c_str(), addr_str.size(), 0);
                    if (written + 2 < buf_size)
                    {
                        buf[written++] = ':';
                        buf[written++] = ' ';
                    }
                    pair = tok.back();
                    written = append_buf(buf, buf_size, written, pair.first, pair.second, 1);
                    if (written + 1 < buf_size)
                    {
                        buf[written++] = '(';
                    }
                    pair = tok[1];
                    written = append_buf(buf, buf_size, written, pair.first, pair.second, 0);
                    if (written + 3 < buf_size)
                    {
                        buf[written++] = '+';
                        buf[written++] = '0';
                        buf[written++] = 'x';
                    }
                    pair = tok[0];
                    char* o_buf;
                    std::size_t o_size;
                    find_valid_addr(pair.first, pair.second-1, &o_buf, &o_size);
                    written = append_buf(buf, buf_size, written, o_buf, o_size, 0);
                    if (written + 2 < buf_size)
                    {
                        buf[written++] = ')';
                        buf[written++] = '\n';
                    }
                }
                else if (pair.second >= 2 && pair.first[0] == '('&& pair.first[1] == 'i')
                {
                    if (written + 3 < buf_size)
                    {
                        buf[written++] = '>';
                        buf[written++] = '>';
                        buf[written++] = ' ';
                    }
                    pair = tok.back();
                    written = append_buf(buf, buf_size, written, pair.first, pair.second, 1);
                    if (written + 1 < buf_size)
                    {
                        buf[written++] = '(';
                    }
                    pair = tok[tok.size()-3];
                    written = append_buf(buf, buf_size, written, pair.first, pair.second, 0);
                    if (written + 2 < buf_size)
                    {
                        buf[written++] = ')';
                        buf[written++] = '\n';
                    }
                }
                else
                {
                    written = append_buf(buf, buf_size, written, tmp.data(), len, 2);
                }

                got_valid = true;
            }
            pclose(p);

            if (!got_valid)
            {
                return append_raw(buf, buf_size, offset, raw);
            }
            return written;
        }
        #endif

        
    }

} // namespace alog

// ------------------------------------------------------------------------------// 唯一导出：直接写 buf，无 vector，无分配
// ------------------------------------------------------------------------------
AA_API std::size_t alog_stack_trace_to_buf(char* buf, std::size_t buf_size, int start)
{
    if (!buf || buf_size == 0)
    {
        return 0;
    }

    start++;
    std::size_t written = 0;

    #ifndef _WIN32
    void* bt_buf[64];
    int n = ::backtrace(bt_buf, 64);
    char** syms = ::backtrace_symbols(bt_buf, n);
    if (!syms)
    {
        return 0;
    }

    for (int i = start; i < n; ++i)
    {
        std::string raw(syms[i]);
        auto p0 = raw.rfind('[');
        auto p1 = raw.rfind(']');
        if (p0 == std::string::npos || p1 == std::string::npos || p0 >= p1)
        {
            std::size_t len = raw.size();
            if (written + len + 1 < buf_size)
            {
                std::memcpy(buf + written, raw.data(), len);
                written += len;
                buf[written++] = '\n';
            }
            continue;
        }

        std::string addr_str = raw.substr(p0 + 1, p1 - p0 - 1);
        std::string mod_str  = raw.substr(0, raw.find('('));
        std::uint64_t addr   = alog::from_hex(addr_str);

        std::uint64_t base = alog::get_module_base(mod_str);
        std::uint64_t offs = addr - base;

        written = alog::invoke_addr2line_to_buf(buf, buf_size, written, mod_str, offs, raw, addr_str);
    }
    ::free(syms);
    #endif

    #ifdef _WIN32
        void* stk[64];
        int n = CaptureStackBackTrace(0, 64, stk, nullptr);
        for (int i = start; i < n; ++i)
        {
            DWORD64 addr = reinterpret_cast<DWORD64>(stk[i]);
            DWORD64 disp;
            IMAGEHLP_SYMBOL64* sym =
                reinterpret_cast<IMAGEHLP_SYMBOL64*>(std::malloc(sizeof(IMAGEHLP_SYMBOL64) + 256));
            sym->SizeOfStruct  = sizeof(IMAGEHLP_SYMBOL64);
            sym->MaxNameLength = 255;

            std::string out;
            if (SymGetSymFromAddr64(GetCurrentProcess(), addr, &disp, sym))
            {
                out = sym->Name;        // Windows 暂无内联偏移，保留函数名
            }
            else
            {
                out = "0x" + alog::to_hex(addr);
            }
            std::free(sym);

            std::size_t len = out.size();
            if (written + len + 1 < buf_size)
            {
                std::memcpy(buf + written, out.data(), len);
                written += len;
                buf[written++] = '\n';
            }
        }
    #endif

    if (written < buf_size)
    {
        buf[written] = '\0';
    }
    return written;
}