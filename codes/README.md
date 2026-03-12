## Build & Run Commands for `codes/src/` Examples

下面的命令假设：

- 当前工作目录是仓库根目录 `LeetCode-cpp`。
- 已有示例源文件位于 `codes/src/`，头文件位于 `codes/includes/`，并且示例源码中已经通过 `#include "../includes/xxx.h"` 正确包含。
- 使用的是 C++20，并且编译器为 `clang++`：
  - `/opt/homebrew/opt/llvm/bin/clang++`

---

### 1. Memory Pool vs `new/delete`

示例文件：`codes/src/memory_pool_example.cpp`

```bash
mkdir -p build
clang++ -std=c++20 -O2 codes/src/memory_pool_example.cpp -o build/mempool_bench
./build/mempool_bench
```

---

### 2. Object Pool vs `std::make_unique`

示例文件：`codes/src/object_pool_example.cpp`

```bash
mkdir -p build
clang++ -std=c++20 -O2 codes/src/object_pool_example.cpp -o build/objpool_bench
./build/objpool_bench
```

---

### 3. Per-task Threads vs `ThreadPool`

示例文件：`codes/src/thread_pool_example.cpp`

```bash
mkdir -p build
clang++ -std=c++20 -O2 -pthread codes/src/thread_pool_example.cpp -o build/threadpool_bench
./build/threadpool_bench
```

`-pthread`（或等价选项）用于启用线程支持和链接正确的线程库。

---

### 4. Mutex Queue vs SPSC Ring Buffer

示例文件：`codes/src/spsc_example.cpp`

```bash
mkdir -p build
clang++ -std=c++20 -O2 -pthread codes/src/spsc_example.cpp -o build/spsc_bench
./build/spsc_bench
```

同样使用 `-pthread` 以支持线程相关 API。  
你可以在同一个可执行程序中实现“带 mutex 的队列版本”和“SPSC ring buffer 版本”并打印各自耗时以对比性能。

---

### 安全版池（Debug 检查）

- **头文件**
  - [`memory_pool_safe.h`](includes/memory_pool_safe.h)：带 debug 检查的内存池（poison 字节、double-free / 跨池释放检测）；Release 构建（`NDEBUG`）下无额外开销。
  - [`object_pool_safe.h`](includes/object_pool_safe.h)：基于 `MemoryPoolSafe` 的对象池，提供 `create`/`destroy` 与 RAII 的 `create_handle()`/`PooledPtr<T>`。

- **使用示例（ObjectPoolSafe + RAII）**

```cpp
#include "codes/includes/object_pool_safe.h"

struct Widget { int x; Widget(int v) : x(v) {} };

void demo() {
    ObjectPoolSafe<Widget> pool;

    // Debug 下：double free / 跨池释放会抛异常；
    // Release 下：走无检查的快路径。
    auto h = pool.create_handle(42);  // PooledPtr<Widget>
    int v = h->x;
    (void)v;
}   // 离开作用域时自动 destroy + 归还到池
```

---

## Data Structures (toy implementations)

### 1. Vector

示例文件：`codes/src/vector_example.cpp`

```bash
mkdir -p build
clang++ -std=c++20 -O2 codes/src/vector_example.cpp -o build/vector_example
./build/vector_example
```

### 2. Hash Table (open addressing)

示例文件：`codes/src/hash_table_example.cpp`

```bash
mkdir -p build
clang++ -std=c++20 -O2 codes/src/hash_table_example.cpp -o build/hash_table_example
./build/hash_table_example
```

### 3. LRU Cache

示例文件：`codes/src/lru_cache_example.cpp`

```bash
mkdir -p build
clang++ -std=c++20 -O2 codes/src/lru_cache_example.cpp -o build/lru_cache_example
./build/lru_cache_example
```

