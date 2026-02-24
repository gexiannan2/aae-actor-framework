#include "ahcpp.h"

#include <unordered_map>
#include <string>
#include <vector>
#include <mutex>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

#include "a.args.h"
#include "a.log.h"

namespace aargs
{

    AA_API void* aargs_newarger()
    {
        return (void*)(new arger_impl);
    }

    AA_API void* aargs_getrunarger()
    {
        static arger_impl rgr_;   // 首次调用时构造，顺序可预测
        return &rgr_;
    }
    auto _ = aargs_getrunarger();

    // 工具：小写 & 去横线
    inline std::string normalize(const char* k)
    {
        std::string s(k ? k : "");
        s.erase(std::remove(s.begin(), s.end(), '-'), s.end());
        std::transform(s.begin(), s.end(), s.begin(),
                    [](unsigned char c){ return std::tolower(c); });
        return s;
    }

    // 工具：trim
    inline std::string trim(const std::string& sv)
    {
        const size_t b = sv.find_first_not_of(" \t\r\n");
        if (b == std::string::npos) return {};
        const size_t e = sv.find_last_not_of(" \t\r\n");
        return sv.substr(b, e - b + 1);
    }

    // 工具：字符串转 bool
    inline bool to_bool(const std::string& sv)
    {
        std::string t(sv);
        std::transform(t.begin(), t.end(), t.begin(),
                    [](unsigned char c){ return std::tolower(c); });
        return t == "true" || t == "1";
    }

    /*--------------------------------------------------
    * 内部：写入值 + 同步已注册指针
    *--------------------------------------------------*/
    template<typename T>
    static void write_all(void* r, const std::string& k, const T& value)
    {
        auto rgr = static_cast<aargs::arger_impl*>(r);
        std::lock_guard<std::mutex> lk(rgr->buf_mtx_);
        if constexpr (std::is_same_v<T, std::int64_t>)
        {
            rgr->i64_[k] = value;
            for (auto* p : rgr->vars_i64_[k]) *p = value;
        }
        else if constexpr (std::is_same_v<T, bool>)
        {
            rgr->b_[k] = value;
            for (auto* p : rgr->vars_b_[k]) *p = value;
        }
        else if constexpr (std::is_same_v<T, std::string>)
        {
            rgr->s_[k] = value;
            for (auto* p : rgr->vars_s_[k]) *p = value;
        }
        else if constexpr (std::is_same_v<T, std::vector<std::int64_t>>)
        {
            rgr->vi64_[k] = value;
            for (auto* p : rgr->vars_vi64_[k]) *p = value;
        }
        else if constexpr (std::is_same_v<T, std::vector<bool>>)
        {
            rgr->vb_[k] = value;
            for (auto* p : rgr->vars_vb_[k]) *p = value;
        }
        else if constexpr (std::is_same_v<T, std::vector<std::string>>)
        {
            rgr->vs_[k] = value;
            for (auto* p : rgr->vars_vs_[k]) *p = value;
        }
    }

}

AA_API void aargs_parse(void* r, int argc, char** argv)
{
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help")
        {
            aargs_help(r);
            return;
        }
        if (arg.find('=') != std::string::npos)
        {
            const size_t p = arg.find('=');
            const std::string k = aargs::normalize(aargs::trim(arg.substr(0, p)).c_str());
            const std::string v = aargs::trim(arg.substr(p + 1));
            try
            {
                aargs_seti(r, k.c_str(), static_cast<std::int64_t>(std::stoll(v)));
            }
            catch (...)
            {
                if (v == "true" || v == "false" || v == "1" || v == "0")
                    aargs_setb(r, k.c_str(), v == "true" || v == "1");
                else
                    aargs_sets(r, k.c_str(), v.c_str());
            }
            continue;
        }
        if (i + 1 < argc)
        {
            const std::string k = aargs::normalize(aargs::trim(argv[i]).c_str());
            const std::string v = aargs::trim(argv[++i]);
            try
            {
                aargs_seti(r, k.c_str(), static_cast<std::int64_t>(std::stoll(v)));
            }
            catch (...)
            {
                if (v == "true" || v == "false" || v == "1" || v == "0")
                    aargs_setb(r, k.c_str(), v == "true" || v == "1");
                else
                    aargs_sets(r, k.c_str(), v.c_str());
            }
        }
    }
}

AA_API void aargs_load(void* r, const char* file)
{
    std::ifstream in(file);
    if (!in)
    {
        aargs_save(r, file);
        return;
    }
    std::string line;
    while (std::getline(in, line))
    {
        line = aargs::trim(line);
        if (line.empty() || line.front() == '#') continue;
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string k = aargs::normalize(aargs::trim(line.substr(0, eq)).c_str());
        const std::string v = aargs::trim(line.substr(eq + 1));
        try
        {
            aargs_seti(r, k.c_str(), static_cast<std::int64_t>(std::stoll(v)));
        }
        catch (...)
        {
            if (v == "true" || v == "false" || v == "1" || v == "0")
                aargs_setb(r, k.c_str(), v == "true" || v == "1");
            else
                aargs_sets(r, k.c_str(), v.c_str());
        }
    }
}

AA_API void aargs_save(void* r, const char* file)
{
    auto rgr = static_cast<aargs::arger_impl*>(r);
    std::ofstream out(file ? file : "args.txt");
    if (!out) return;
    out << "# args config\n# key=value\n\n";
    for (const auto& [key, remark] : rgr->remark_)
    {
        std::istringstream iss(remark);
        std::string ln;
        while (std::getline(iss, ln)) out << "# " << ln << '\n';
        if (rgr->i64_.count(key))
        {
            out << key << " = " << rgr->i64_[key] << "\n\n";
            continue;
        }
        if (rgr->b_.count(key))
        {
            out << key << " = " << (rgr->b_[key] ? "true" : "false") << "\n\n";
            continue;
        }
        if (rgr->s_.count(key))
        {
            out << key << " = " << rgr->s_[key] << "\n\n";
            continue;
        }
        if (rgr->vi64_.count(key))
        {
            out << key << " = ";
            for (size_t i = 0; i < rgr->vi64_[key].size(); ++i)
            {
                if (i) out << ',';
                out << rgr->vi64_[key][i];
            }
            out << "\n\n";
            continue;
        }
        if (rgr->vb_.count(key))
        {
            out << key << " = ";
            for (size_t i = 0; i < rgr->vb_[key].size(); ++i)
            {
                if (i) out << ',';
                out << (rgr->vb_[key][i] ? "true" : "false");
            }
            out << "\n\n";
            continue;
        }
        if (rgr->vs_.count(key))
        {
            out << key << " = ";
            for (size_t i = 0; i < rgr->vs_[key].size(); ++i)
            {
                if (i) out << ',';
                out << rgr->vs_[key][i];
            }
            out << "\n\n";
        }
    }
}

AA_API void aargs_help(void* r)
{
    
    aargs_save(r, nullptr);
}

AA_API void aargs_reload(void* r)
{
    auto rgr = static_cast<aargs::arger_impl*>(r);
    for (auto f : rgr->loaded_files_)
    {
        aargs_load(r, f.c_str());
    }
}

/*--------------------------------------------------
* reg：基础类型
*--------------------------------------------------*/
AA_API bool aargs_regi(void* r, const char* key, const char* remark, std::int64_t def, std::int64_t* var_addr)
{
    auto rgr = static_cast<aargs::arger_impl*>(r);
    const std::string k = aargs::normalize(key);
    rgr->remark_[k] = remark ? remark : "";
    if (var_addr) rgr->vars_i64_[k].push_back(var_addr);
    aargs::write_all(r, k, def);
    return true;
}

AA_API bool aargs_regb(void* r, const char* key, const char* remark, bool def, bool* var_addr)
{
    auto rgr = static_cast<aargs::arger_impl*>(r);
    const std::string k = aargs::normalize(key);
    rgr->remark_[k] = remark ? remark : "";
    if (var_addr) rgr->vars_b_[k].push_back(var_addr);
    aargs::write_all(r, k, def);
    return true;
}

AA_API bool aargs_regs(void* r, const char* key, const char* remark, const char* def, const char** var_addr)
{
    auto rgr = static_cast<aargs::arger_impl*>(r);
    const std::string k = aargs::normalize(key);
    rgr->remark_[k] = remark ? remark : "";
    if (var_addr)
    {
        rgr->s_[k] = def ? def : "";
        rgr->vars_s_[k].push_back(const_cast<std::string*>(&rgr->s_[k]));
        *var_addr = rgr->s_[k].c_str();
    }
    else
    {
        aargs::write_all(r, k, std::string(def ? def : ""));
    }
    return true;
}

/*--------------------------------------------------
* reg：vector
*--------------------------------------------------*/
AA_API bool aargs_regvi(void* r, const char* key, const char* remark,
                                      const std::int64_t* def, size_t def_len,
                                      std::int64_t** var_addr, size_t* var_len)
{
    auto rgr = static_cast<aargs::arger_impl*>(r);
    const std::string k = aargs::normalize(key);
    rgr->remark_[k] = remark ? remark : "";
    std::vector<std::int64_t> v(def, def + def_len);
    aargs::write_all(r, k, v);
    if (var_addr && var_len)
    {
        rgr->vi64_[k] = v;   // 确保地址稳定
        *var_addr = rgr->vi64_[k].data();
        *var_len  = rgr->vi64_[k].size();
        rgr->vars_vi64_[k].push_back(&rgr->vi64_[k]);
    }
    return true;
}

AA_API bool aargs_regvb(void* r, const char* key, const char* remark,
                                       const bool* def, size_t def_len,
                                       bool** var_addr, size_t* var_len)
{
    auto rgr = static_cast<aargs::arger_impl*>(r);
    const std::string k = aargs::normalize(key);
    rgr->remark_[k] = remark ? remark : "";
    std::vector<bool> v(def, def + def_len);
    aargs::write_all(r, k, v);
    if (var_addr && var_len)
    {
        rgr->vb_[k] = std::move(v);
        *var_addr = nullptr;          // vector<bool> 不连续，禁止直接指针
        *var_len  = rgr->vb_[k].size();
        rgr->vars_vb_[k].push_back(&rgr->vb_[k]);
    }
    return true;
}

AA_API bool aargs_regvs(void* r, const char* key, const char* remark,
                                      const char* const* def, size_t def_len,
                                      const char*** var_addr, size_t* var_len)
{
    auto rgr = static_cast<aargs::arger_impl*>(r);
    const std::string k = aargs::normalize(key);
    rgr->remark_[k] = remark ? remark : "";
    std::vector<std::string> v(def, def + def_len);
    aargs::write_all(r, k, v);
    if (var_addr && var_len)
    {
        rgr->vs_[k] = std::move(v);
        rgr->buf_vstr_.clear();
        rgr->buf_vstr_.reserve(rgr->vs_[k].size());
        for (auto& s : rgr->vs_[k]) rgr->buf_vstr_.push_back(s.c_str());
        *var_addr = rgr->buf_vstr_.data();
        *var_len  = rgr->buf_vstr_.size();
        rgr->vars_vs_[k].push_back(&rgr->vs_[k]);
    }
    return true;
}

/*--------------------------------------------------
* set：基础类型
*--------------------------------------------------*/
AA_API bool aargs_seti(void* r, const char* key, std::int64_t value)
{
    aargs::write_all(r, aargs::normalize(key), value);
    return true;
}

AA_API bool aargs_setb(void* r, const char* key, bool value)
{
    aargs::write_all(r, aargs::normalize(key), value);
    return true;
}

AA_API bool aargs_sets(void* r, const char* key, const char* value)
{
    aargs::write_all(r, aargs::normalize(key), std::string(value ? value : ""));
    return true;
}

/*--------------------------------------------------
* set：vector
*--------------------------------------------------*/
AA_API bool aargs_setvi(void* r, const char* key, const std::int64_t* value, size_t len)
{
    aargs::write_all(r, aargs::normalize(key), std::vector<std::int64_t>(value, value + len));
    return true;
}

AA_API bool aargs_setvb(void* r, const char* key, const bool* value, size_t len)
{
    aargs::write_all(r, aargs::normalize(key), std::vector<bool>(value, value + len));
    return true;
}

AA_API bool aargs_set_vstr(void* r, const char* key, const char* const* value, size_t len)
{
    aargs::write_all(r, aargs::normalize(key), std::vector<std::string>(value, value + len));
    return true;
}

/*--------------------------------------------------
* get：基础类型
* 字符串返回**内部常量缓冲区**，生命周期到下次调用或模块卸载
*--------------------------------------------------*/
AA_API std::int64_t aargs_geti(void* r, const char* key, std::int64_t def)
{
    auto rgr = static_cast<aargs::arger_impl*>(r);
    std::lock_guard<std::mutex> lk(rgr->buf_mtx_);
    const std::string k = aargs::normalize(key);
    auto it = rgr->i64_.find(k);
    return it != rgr->i64_.end() ? it->second : def;
}

AA_API bool aargs_getb(void* r, const char* key, bool def)
{
    auto rgr = static_cast<aargs::arger_impl*>(r);
    std::lock_guard<std::mutex> lk(rgr->buf_mtx_);
    const std::string k = aargs::normalize(key);
    auto it = rgr->b_.find(k);
    return it != rgr->b_.end() ? it->second : def;
}

AA_API const char* aargs_gets(void* r, const char* key, const char* def)
{
    auto rgr = static_cast<aargs::arger_impl*>(r);
    std::lock_guard<std::mutex> lk(rgr->buf_mtx_);
    const std::string k = aargs::normalize(key);
    auto it = rgr->s_.find(k);
    if (it != rgr->s_.end())
    {
        return it->second.c_str();
    }
    return def ? def : "";
}

/*--------------------------------------------------
* get：vector
* 返回**内部连续存储的首指针**，生命周期到下次调用或模块卸载
* 长度通过输出参数返回；vector<bool> 特殊处理（无连续存储）
*--------------------------------------------------*/
AA_API const std::int64_t* aargs_getvi(void* r, const char* key, size_t* out_len)
{
    if (!out_len) return nullptr;
    auto rgr = static_cast<aargs::arger_impl*>(r);
    std::lock_guard<std::mutex> lk(rgr->buf_mtx_);
    const std::string k = aargs::normalize(key);
    auto it = rgr->vi64_.find(k);
    if (it == rgr->vi64_.end())
    {
        *out_len = 0;
        return nullptr;
    }    
    rgr->buf_vi64_ = it->second;          // 拷贝到内部缓冲区
    *out_len = rgr->buf_vi64_.size();
    return rgr->buf_vi64_.data();
}

AA_API const bool* aargs_getvb(void* r, const char* key, size_t* out_len)
{
    if (!out_len) return nullptr;
    auto rgr = static_cast<aargs::arger_impl*>(r);
    std::lock_guard<std::mutex> lk(rgr->buf_mtx_);
    const std::string k = aargs::normalize(key);
    auto it = rgr->vb_.find(k);
    if (it == rgr->vb_.end())
    {
        *out_len = 0;
        return nullptr;
    }
    rgr->buf_vb_ = it->second;            // 拷贝到内部缓冲区
    *out_len = rgr->buf_vb_.size();
    // vector<bool> 不连续，返回 nullptr，调用方通过下标接口访问
    return nullptr;
}

AA_API const char* const* aargs_getvs(void* r, const char* key, size_t* out_len)
{
    if (!out_len) return nullptr;
    auto rgr = static_cast<aargs::arger_impl*>(r);
    std::lock_guard<std::mutex> lk(rgr->buf_mtx_);
    const std::string k = aargs::normalize(key);
    auto it = rgr->vs_.find(k);
    if (it == rgr->vs_.end())
    {
        *out_len = 0;
        return nullptr;
    }
    rgr->buf_vs_  = it->second;           // 拷贝到内部缓冲区
    rgr->buf_vstr_.clear();
    rgr->buf_vstr_.reserve(rgr->buf_vs_.size());
    for (auto& s : rgr->buf_vs_) rgr->buf_vstr_.push_back(s.c_str());
    *out_len = rgr->buf_vstr_.size();
    return rgr->buf_vstr_.data();
}