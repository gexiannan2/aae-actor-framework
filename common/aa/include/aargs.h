#pragma once
#include <stddef.h>   // size_t
#include <stdbool.h>  // bool
#include <stdint.h>   // std::int64_t

#include <string>
#include <vector>
#include <cstdint>

#include "aos.h"

/* 生命周期 */
AA_API void aargs_parse(void* r, int argc, char** argv);
AA_API void aargs_load(void* r, const char* file);
AA_API void aargs_reload(void* r);
AA_API void aargs_save(void* r, const char* file);
AA_API void aargs_help(void* r);

/*--------------------------------------------------
* reg：基础类型
*--------------------------------------------------*/
AA_API bool aargs_regi(void* r, const char* key, const char* remark, std::int64_t def, std::int64_t* var_addr);
AA_API bool aargs_regb(void* r, const char* key, const char* remark, bool def, bool* var_addr);
AA_API bool aargs_regs(void* r, const char* key, const char* remark, const char* def, const char** var_addr);

/*--------------------------------------------------
* reg：vector
* 注意：def/var_addr 均为“指向首元素的指针”，len 为元素个数
*--------------------------------------------------*/
AA_API bool aargs_regvi(void* r, const char* key, const char* remark,
                           const std::int64_t* def, size_t def_len,
                           std::int64_t** var_addr, size_t* var_len);
AA_API bool aargs_regvb(void* r, const char* key, const char* remark,
                            const bool* def, size_t def_len,
                            bool** var_addr, size_t* var_len);
AA_API bool aargs_regvs(void* r, const char* key, const char* remark,
                           const char* const* def, size_t def_len,
                           const char*** var_addr, size_t* var_len);

/*--------------------------------------------------
* set：基础类型
*--------------------------------------------------*/
AA_API bool aargs_seti(void* r, const char* key, std::int64_t value);
AA_API bool aargs_setb(void* r, const char* key, bool value);
AA_API bool aargs_sets(void* r, const char* key, const char* value);

/*--------------------------------------------------
* set：vector
*--------------------------------------------------*/
AA_API bool aargs_setvi(void* r, const char* key, const std::int64_t* value, size_t len);
AA_API bool aargs_setvb(void* r, const char* key, const bool* value, size_t len);
AA_API bool aargs_setvs(void* r, const char* key, const char* const* value, size_t len);

/*--------------------------------------------------
* get：基础类型
* 字符串返回值为**内部常量缓冲区**，生命周期到下次调用或模块卸载
*--------------------------------------------------*/
AA_API std::int64_t aargs_geti(void* r, const char* key, std::int64_t def);
AA_API bool    aargs_getb(void* r, const char* key, bool def);
AA_API const char* aargs_gets(void* r, const char* key, const char* def);

/*--------------------------------------------------
* get：vector
* 返回**内部连续存储的首指针**，生命周期到下次调用或模块卸载
* 长度通过输出参数返回
*--------------------------------------------------*/
AA_API const std::int64_t* aargs_getvi(void* r, const char* key, size_t* out_len);
AA_API const bool*    aargs_getvb(void* r, const char* key, size_t* out_len);
AA_API const char* const* aargs_getvs(void* r, const char* key, size_t* out_len);
AA_API void* aargs_newarger();
AA_API void* aargs_getrunarger();

/* 所有封装函数均为 inline，不生成外部符号，仅在本编译单元使用 */
namespace aargs
{

    class arger
    {
    public:
        /* 生命周期 */
        inline void parse(int argc, char** argv) { aargs_parse(this, argc, argv); }
        inline void load(const std::string& file) { aargs_load(this, file.c_str()); }
        inline void save(const std::string& file) { aargs_save(this, file.c_str()); }
        inline void help() { aargs_help(this); }
        inline void reload() { aargs_reload(this); }

        /*--------------------------------------------------
        * reg：基础类型
        *--------------------------------------------------*/
        inline bool reg(const char* key, const char* remark, std::int64_t def, std::int64_t* var_addr = nullptr)
        {
            return aargs_regi(this, key, remark, def, var_addr);
        }

        inline bool reg(const char* key, const char* remark, bool def, bool* var_addr = nullptr)
        {
            return aargs_regb(this, key, remark, def, var_addr);
        }

        inline bool reg(const char* key, const char* remark, const char* def, const char** var_addr = nullptr)
        {
            return aargs_regs(this, key, remark, def, var_addr);
        }

        /*--------------------------------------------------
        * reg：vector
        * 直接拿 std::vector.data() 当 C 数组，零拷贝
        *--------------------------------------------------*/
        inline bool reg(const char* key, const char*remark,
                        const std::vector<std::int64_t>& def,
                        std::vector<std::int64_t>** var_addr = nullptr, size_t* var_len = nullptr)
        {
            std::int64_t* addr_ptr = nullptr;
            size_t   len_ptr  = 0;
            bool ok = aargs_regvi(this, key, remark,
                                    def.data(), def.size(),
                                    var_addr ? &addr_ptr : nullptr,
                                    var_len  ? &len_ptr  : nullptr);
            if (var_addr) *var_addr = ok ? const_cast<std::vector<std::int64_t>*>(&def) : nullptr;
            if (var_len)  *var_len  = ok ? def.size() : 0;
            return ok;
        }

        inline bool reg(const char* key, const char*remark,
                        const std::vector<bool>& def,
                        std::vector<bool>** var_addr = nullptr, size_t* var_len = nullptr)
        {
            // vector<bool> 无 data()，先拷到连续缓冲区
            thread_local std::vector<char> buf;   // char 可安全取地址
            buf.resize(def.size());
            for (size_t i = 0; i < def.size(); ++i) buf[i] = def[i] ? 1 : 0;
            bool* addr_ptr = nullptr;
            size_t len_ptr = 0;
            bool ok = aargs_regvb(this, key, remark,
                                    reinterpret_cast<bool*>(buf.data()), def.size(),
                                    var_addr ? &addr_ptr : nullptr,
                                    var_len  ? &len_ptr  : nullptr);
            if (var_addr) *var_addr = ok ? const_cast<std::vector<bool>*>(&def) : nullptr;
            if (var_len)  *var_len  = ok ? def.size() : 0;
            return ok;
        }

        inline bool reg(const char* key, const char*remark,
                        const std::vector<std::string>& def,
                        std::vector<std::string>** var_addr = nullptr, size_t* var_len = nullptr)
        {
            // 构造临时 const char* 数组（零拷贝指向 string::c_str()）
            thread_local std::vector<const char*> tmp;
            tmp.clear();
            tmp.reserve(def.size());
            for (auto& s : def) tmp.push_back(s.c_str());
            const char** addr_ptr = nullptr;
            size_t       len_ptr  = 0;
            bool ok = aargs_regvs(this, key, remark,
                                    tmp.data(), tmp.size(),
                                    var_addr ? &addr_ptr : nullptr,
                                    var_len  ? &len_ptr  : nullptr);
            if (var_addr) *var_addr = ok ? const_cast<std::vector<std::string>*>(&def) : nullptr;
            if (var_len)  *var_len  = ok ? def.size() : 0;
            return ok;
        }

        /*--------------------------------------------------
        * set：基础类型
        *--------------------------------------------------*/
        inline bool set(const char* key, std::int64_t value)
        {
            return aargs_seti(this, key, value);
        }

        inline bool set(const char* key, bool value)
        {
            return aargs_setb(this, key, value);
        }

        inline bool set(const char* key, const char* value)
        {
            return aargs_sets(this, key, value);
        }

        /*--------------------------------------------------
        * set：vector
        *--------------------------------------------------*/
        inline bool set(const char* key, const std::vector<std::int64_t>& value)
        {
            return aargs_setvi(this, key, value.data(), value.size());
        }

        inline bool set(const char* key, const std::vector<bool>& value)
        {
            // vector<bool> 无 data()，先拷到连续缓冲区
            thread_local std::vector<char> buf;
            buf.resize(value.size());
            for (size_t i = 0; i < value.size(); ++i) buf[i] = value[i] ? 1 : 0;
            return aargs_setvb(this, key, reinterpret_cast<bool*>(buf.data()), value.size());
        }

        inline bool set(const char* key, const std::vector<std::string>& value)
        {
            thread_local std::vector<const char*> tmp;
            tmp.clear();
            tmp.reserve(value.size());
            for (auto& s : value) tmp.push_back(s.c_str());
            return aargs_setvs(this, key, tmp.data(), tmp.size());
        }

        /*--------------------------------------------------
        * get：基础类型
        *--------------------------------------------------*/
        inline std::int64_t get(const char* key, std::int64_t def)
        {
            return aargs_geti(this, key, def);
        }

        inline bool get(const char* key, bool def)
        {
            return aargs_getb(this, key, def);
        }

        inline std::string get(const char* key, const char* def)
        {
            const char* p = aargs_gets(this, key, def);
            return p ? std::string(p) : def;
        }

        /*--------------------------------------------------
        * get：vector
        * 返回 std::vector 的**拷贝**，调用方可自由修改/延长生命期
        *--------------------------------------------------*/
        inline std::vector<std::int64_t> get(const char* key, const std::vector<std::int64_t>& def)
        {
            size_t len = 0;
            const std::int64_t* p = aargs_getvi(this, key, &len);
            if (!p) return def;
            return std::vector<std::int64_t>(p, p + len);
        }

        inline std::vector<bool> get(const char* key, const std::vector<bool>& def)
        {
            size_t len = 0;
            aargs_getvb(this, key, &len);
            std::vector<bool> v(len);
            // 内部无连续存储，只能逐个拿
            for (size_t i = 0; i < len; ++i) v[i] = aargs_getb(this, key, false);
            return v;
        }

        inline std::vector<std::string> get(const char* key, const std::vector<std::string>& def)
        {
            size_t len = 0;
            const char* const* p = aargs_getvs(this, key, &len);
            if (!p) return def;
            return std::vector<std::string>(p, p + len);
        }
    };

    inline arger* newarger()
    {
        return static_cast<arger*>(aargs_newarger());
    }

    inline arger* getrunarger()
    {
        return static_cast<arger*>(aargs_getrunarger());
    }

} // namespace aargs