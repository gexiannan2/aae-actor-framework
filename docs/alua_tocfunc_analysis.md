# alua::tocfunc 包装机制深度分析

## 问题背景

`athd::getctname()` 是一个普通的 C++ 函数：
```cpp
const char* athd::getctname();  // 无参数，返回字符串
```

但 Lua C API 要求所有注册的函数必须符合以下签名：
```cpp
typedef int (*lua_CFunction)(lua_State* L);  // 必须接受 lua_State*，返回 int
```

**问题：`alua::tocfunc<athd::getctname>()` 是如何自动添加 `lua_State* L` 参数的？**

---

## 包装流程分析

### 第 1 步：tocfunc 模板函数

```cpp
// 位置：alua.h:701-705
template<auto f>
inline lua_CFunction tocfunc()
{
    return pvt::lua_to_c_wrapper<f>::call;
}
```

**关键点：**
- `template<auto f>`: C++17 的非类型模板参数，可以直接传递函数指针
- 返回 `pvt::lua_to_c_wrapper<f>::call`，这是一个静态成员函数

---

### 第 2 步：lua_to_c_wrapper 包装器类

```cpp
// 位置：alua.h:436-477
template<auto f> struct lua_to_c_wrapper;

// 模板特化：解析函数类型
template<typename R, typename... A, R(*f)(A...)> 
struct lua_to_c_wrapper<f>
{
    static int call(lua_State* l)  // ← 这里自动添加了 lua_State* 参数！
    {
        constexpr int k_expect = sizeof...(A);
        const int argc = lua_gettop(l);
        
        // 1. 参数个数检查
        if(argc != static_cast<int>(k_expect))
        {
            luaL_error(l, "expect %d args, got %d", k_expect, argc);
        }
        
        try
        {
            // 2. 从 Lua 栈提取参数
            auto args = tuple_popper<A...>::get(l, 1);
            
            // 3. 调用原始 C++ 函数
            if constexpr(std::is_same_v<R, void>)
            {
                std::apply(f, args);  // 无返回值
                return 0;
            }
            else
            {
                R ret = std::apply(f, args);  // 有返回值
                return ret_helper<R>::push(l, ret);  // 压入 Lua 栈
            }
        }
        catch(const std::exception& e)
        {
            // 异常处理
            luaL_where(l, 1);
            lua_pushfstring(l, "C++ exception: %s", e.what());
            lua_concat(l, 2);
            lua_error(l);
        }
        catch(...)
        {
            luaL_where(l, 1);
            lua_pushstring(l, "unknown C++ exception");
            lua_concat(l, 2);
            lua_error(l);
        }
        return 0;
    }
};
```

**关键机制：**

1. **模板特化解析函数类型**
   ```cpp
   template<typename R, typename... A, R(*f)(A...)>
   ```
   - `R`: 返回值类型
   - `A...`: 参数类型包
   - `f`: 函数指针

2. **静态 call 方法签名**
   ```cpp
   static int call(lua_State* l)
   ```
   这就是符合 Lua C API 要求的签名！

3. **参数自动提取和转换**
   ```cpp
   auto args = tuple_popper<A...>::get(l, 1);
   ```
   从 Lua 栈中按类型提取参数，构造成 `std::tuple`

4. **调用原始函数**
   ```cpp
   std::apply(f, args);
   ```
   将 tuple 展开为函数参数，调用原始 C++ 函数

---

## 具体示例：athd::getctname 的包装过程

### 原始函数
```cpp
const char* athd::getctname();  // 位于 athd.h:260-263
```

### 模板实例化

当调用 `alua::tocfunc<athd::getctname>()` 时：

```cpp
// 1. tocfunc 模板实例化
template<>
lua_CFunction tocfunc<athd::getctname>()
{
    return pvt::lua_to_c_wrapper<athd::getctname>::call;
}

// 2. lua_to_c_wrapper 特化实例化
template<>
struct lua_to_c_wrapper<athd::getctname>
{
    // R = const char*
    // A... = (空，无参数)
    
    static int call(lua_State* l)
    {
        constexpr int k_expect = 0;  // sizeof...(A) = 0
        const int argc = lua_gettop(l);
        
        if(argc != 0)
        {
            luaL_error(l, "expect 0 args, got %d", argc);
        }
        
        try
        {
            // args = std::tuple<>  (空 tuple)
            auto args = tuple_popper<>::get(l, 1);
            
            // 调用原始函数
            const char* ret = std::apply(athd::getctname, args);
            // 等价于：const char* ret = athd::getctname();
            
            // 将返回值压入 Lua 栈
            return ret_helper<const char*>::push(l, ret);
        }
        catch(...)
        {
            // 异常处理...
        }
        return 0;
    }
};
```

### 生成的包装函数

最终效果等价于手写：

```cpp
int wrapper_getctname(lua_State* L)
{
    // 1. 检查参数（预期 0 个）
    if (lua_gettop(L) != 0) {
        return luaL_error(L, "expect 0 args, got %d", lua_gettop(L));
    }
    
    // 2. 调用原始 C++ 函数
    const char* result = athd::getctname();
    
    // 3. 将结果压入 Lua 栈
    lua_pushstring(L, result);
    
    // 4. 返回值个数
    return 1;
}
```

---

## 更复杂的示例：带参数的函数

假设有这样一个函数：

```cpp
int add(int a, int b) { return a + b; }
```

### 包装后的效果

```cpp
// 调用：alua::tocfunc<add>()

// 生成的包装函数等价于：
int wrapper_add(lua_State* L)
{
    // 1. 检查参数个数
    if (lua_gettop(L) != 2) {
        return luaL_error(L, "expect 2 args, got %d", lua_gettop(L));
    }
    
    // 2. 从 Lua 栈提取参数
    int a = static_cast<int>(luaL_checkinteger(L, 1));
    int b = static_cast<int>(luaL_checkinteger(L, 2));
    
    // 3. 调用原始函数
    int result = add(a, b);
    
    // 4. 压入返回值
    lua_pushinteger(L, result);
    
    // 5. 返回值个数
    return 1;
}
```

---

## 核心技术点

### 1. C++17 非类型模板参数 (auto)

```cpp
template<auto f>  // 可以直接接受函数指针作为模板参数
```

在 C++17 之前需要写成：
```cpp
template<typename R, typename... Args, R(*f)(Args...)>
```

### 2. 模板参数包展开 (Parameter Pack)

```cpp
template<typename... A>  // 可变参数模板
sizeof...(A)             // 获取参数个数
```

### 3. std::apply (C++17)

```cpp
auto args = std::tuple<int, int>{10, 20};
int result = std::apply(add, args);  // 等价于 add(10, 20)
```

将 tuple 展开为函数参数。

### 4. constexpr if (C++17)

```cpp
if constexpr(std::is_same_v<R, void>)
{
    // 编译期分支：无返回值版本
    std::apply(f, args);
    return 0;
}
else
{
    // 编译期分支：有返回值版本
    R ret = std::apply(f, args);
    return ret_helper<R>::push(l, ret);
}
```

### 5. 类型萃取 (Type Traits)

```cpp
template<typename R, typename... A, R(*f)(A...)>
```

通过模板特化自动推导：
- 返回值类型 `R`
- 参数类型 `A...`
- 函数指针 `f`

---

## 参数提取和返回值压栈

### tuple_popper：参数提取

```cpp
// 位置：alua.h:409-418
template<typename H, typename... T> 
struct tuple_popper<H, T...>
{
    static auto get(lua_State* l, int idx)
    {
        return std::tuple_cat(
            std::make_tuple(popper<H>::get(l, idx)),  // 提取第一个
            tuple_popper<T...>::get(l, idx+1)          // 递归提取剩余
        );
    }
};
```

### popper：单个参数提取

```cpp
// 位置：alua.h:267-298
template<> struct popper<int>
{ 
    static int get(lua_State* l, int i) {
        return static_cast<int>(luaL_checkinteger(l, i));
    }
};

template<> struct popper<const char*>
{ 
    static const char* get(lua_State* l, int i) {
        return luaL_checkstring(l, i);
    }
};

// ... 其他类型的特化
```

### ret_helper：返回值压栈

```cpp
// 位置：alua.h:421-433
template<typename R> 
struct ret_helper
{
    static int push(lua_State* l, const R& v) {
        pusher<R>::put(l, v);
        return 1;  // 返回值个数
    }
};

template<> 
struct ret_helper<void> { 
    static int push(lua_State*, ...) {
        return 0;  // void 无返回值
    } 
};
```

### pusher：单个值压栈

```cpp
// 位置：alua.h:333-345
template<> struct pusher<int>
{ 
    static void put(lua_State* l, int v) {
        lua_pushinteger(l, v);
    }
};

template<> struct pusher<const char*>
{ 
    static void put(lua_State* l, const char* v) {
        lua_pushstring(l, v);
    }
};

// ... 其他类型的特化
```

---

## 完整的调用链

让我们跟踪一次完整的调用：

### C++ 端注册

```cpp
{"getctname", alua::tocfunc<athd::getctname>()}

// 展开后：
{"getctname", &lua_to_c_wrapper<athd::getctname>::call}
```

### Lua 端调用

```lua
local name = athd.getctname()
```

### 执行流程

```
1. Lua 调用 athd.getctname()
   ↓
2. Lua VM 调用注册的 C 函数：lua_to_c_wrapper<athd::getctname>::call(L)
   ↓
3. call 函数检查参数：lua_gettop(L) == 0 ✓
   ↓
4. 提取参数：tuple_popper<>::get(L, 1) → std::tuple<>{}
   ↓
5. 调用原始函数：std::apply(athd::getctname, std::tuple<>{})
   等价于：const char* ret = athd::getctname();
   ↓
6. 压入返回值：pusher<const char*>::put(L, ret)
   等价于：lua_pushstring(L, ret);
   ↓
7. 返回值个数：return 1
   ↓
8. Lua VM 从栈顶取值，赋给变量 name
```

---

## 支持的类型

### 基础类型
- `int`, `float`, `double`, `bool`
- `std::uint32_t`, `std::uint64_t`, `std::int64_t`
- `const char*`, `std::string`, `std::string_view`
- `void*` (lightuserdata)

### 容器类型
```cpp
std::vector<T>                    // Lua table (数组)
std::unordered_map<K, V>          // Lua table (字典)
std::tuple<T1, T2, ...>           // 多返回值
```

### 示例

```cpp
// 1. 返回 vector
std::vector<int> get_numbers() { return {1, 2, 3}; }
// Lua: local arr = mod.get_numbers()  → {1, 2, 3}

// 2. 返回 map
std::unordered_map<std::string, int> get_config() {
    return {{"width", 800}, {"height", 600}};
}
// Lua: local cfg = mod.get_config()  → {width=800, height=600}

// 3. 返回 tuple（多返回值）
std::tuple<int, std::string> get_pair(int x) {
    return {x * 2, "ok"};
}
// Lua: local num, msg = mod.get_pair(10)  → 20, "ok"
```

---

## 优势总结

### 1. 零手工代码
不需要写任何 Lua C API 的胶水代码：

**传统方式（手写）：**
```cpp
int lua_getctname(lua_State* L) {
    if (lua_gettop(L) != 0) {
        return luaL_error(L, "wrong number of arguments");
    }
    const char* name = athd::getctname();
    lua_pushstring(L, name);
    return 1;
}
```

**现代方式（自动）：**
```cpp
{"getctname", alua::tocfunc<athd::getctname>()}
```

### 2. 类型安全
编译期检查类型匹配，避免运行时错误。

### 3. 自动类型转换
支持复杂类型（vector, map, tuple），自动转换。

### 4. 异常安全
自动捕获并转换 C++ 异常为 Lua 错误。

### 5. 性能优化
- 使用 `constexpr` 和模板，大部分工作在编译期完成
- 零运行时开销（相比手写）

---

## 总结

**`alua::tocfunc<athd::getctname>()` 的核心机制：**

1. **不是"添加" `lua_State* L` 参数**，而是**生成一个新的包装函数**
2. 包装函数签名符合 Lua C API 要求：`int wrapper(lua_State* L)`
3. 包装函数内部：
   - 从 Lua 栈提取参数
   - 调用原始 C++ 函数
   - 将返回值压回 Lua 栈
4. 整个过程通过**模板元编程**在**编译期自动生成**

**关键技术：**
- C++17 非类型模板参数 (`auto`)
- 模板特化与参数包展开
- `std::apply` 和 `std::tuple`
- 编译期类型推导和分支 (`constexpr if`)

这是一个非常优雅的**零成本抽象**（Zero-Cost Abstraction）实现！

