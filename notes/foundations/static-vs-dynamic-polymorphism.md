## C++ 中的静态多态 vs 动态多态

> 对应代码示例：
> - 动态多态：`codes/src/oop/dynamic_virtual_example.cpp`
> - 静态多态：`codes/src/oop/static_polymorphism_example.cpp`

---

## 一、整体直觉：两种“多态”的出发点不同

- **动态多态（runtime polymorphism）**
  - 关键字：`virtual`、虚函数表（vtable）、基类指针/引用。
  - 在**运行时**根据对象的真实类型选择调用哪个实现。
  - 典型用法：`Shape* shape = new Circle(...); shape->draw();`
  - 本质：**“对象驱动的行为选择”**——对象携带类型信息，调用时才决定行为。

- **静态多态（static/compile-time polymorphism）**
  - 关键字：模板、CRTP、duck typing、`std::variant`。
  - 在**编译期**根据模板实参决定生成什么代码；调用在编译期就能静态解析。
  - 典型用法：`template <typename Shape> void render(const Shape& shape) { shape.draw(); }`
  - 本质：**“类型驱动的代码生成”**——选择在编译期就做完，生成专用代码。

两者都提供“写一份代码，支持多种具体类型”的能力，但：

- 动态多态偏向：**接口稳定、运行期灵活**；
- 静态多态偏向：**性能好、优化空间大**。

---

## 二、动态多态：基类 + virtual 函数（运行时分派）

对应代码：`dynamic_virtual_example.cpp`

```cpp
class Shape {
public:
    virtual ~Shape() = default;
    virtual void draw() const = 0;
    virtual double area() const = 0;
};

class Circle : public Shape {
public:
    explicit Circle(double r) : r_(r) {}
    void draw() const override { /* ... */ }
    double area() const override { return M_PI * r_ * r_; }
private:
    double r_;
};

class Rectangle : public Shape {
public:
    Rectangle(double w, double h) : w_(w), h_(h) {}
    void draw() const override { /* ... */ }
    double area() const override { return w_ * h_; }
private:
    double w_, h_;
};
```

使用方式：

```cpp
std::vector<std::unique_ptr<Shape>> shapes;
shapes.emplace_back(std::make_unique<Circle>(1.0));
shapes.emplace_back(std::make_unique<Rectangle>(2.0, 3.0));

double total_area = 0.0;
for (const auto& s : shapes) {
    s->draw();           // 在运行时根据真实类型动态绑定
    total_area += s->area();
}
```

### 2.1 动态多态的关键特性

- **统一抽象类型**：`Shape*` / `Shape&` 可以指向任意派生类（`Circle`, `Rectangle`, ...）。
- **运行时绑定（dynamic dispatch）**：
  - 调用 `s->draw()` 时，编译器生成“查虚表 + 间接跳转”的代码。
  - 运行时根据对象的 vptr 决定具体调用哪个函数实现。
- **对象通过“基类接口”在系统中流动**：
  - 比如图形渲染管线、UI widget 系统、游戏实体系统等，管线内部只认识 `Shape&` 或 `Widget&`。

### 2.2 动态多态的优点

- **扩展性好**：
  - 新增一个派生类只需实现基类接口，不必修改使用方逻辑（经典的“开闭原则”）。
- **运行时灵活**：
  - 可以根据配置/输入/工厂模式，在运行时决定具体类型。
- **接口稳定**：
  - 使用方依赖的是基类抽象，内部实现可以替换，而不影响调用代码。

### 2.3 动态多态的代价

- 每次虚调用涉及：
  - 访问 vptr、查 vtable、一次间接跳转；
  - 对 CPU 分支预测和指令缓存不够友好。
- 无法内联到具体实现（一般情况下），限制编译器优化。
- 对象布局多一个 vptr 字段（通常是一个指针大小）。

在绝大多数业务代码里，这些成本是完全可以接受的换取“结构清晰 + 扩展性”的代价。

---

## 三、静态多态：模板 / duck typing（编译期分派）

对应代码：`static_polymorphism_example.cpp`

```cpp
struct CircleStatic {
    explicit CircleStatic(double r) : r_(r) {}
    void draw() const { /* ... */ }
    double area() const { return M_PI * r_ * r_; }
private:
    double r_;
};

struct RectangleStatic {
    RectangleStatic(double w, double h) : w_(w), h_(h) {}
    void draw() const { /* ... */ }
    double area() const { return w_ * h_; }
private:
    double w_, h_;
};

template <typename Shape>
void render_and_accumulate(const Shape& s, double& acc) {
    s.draw();        // 编译期静态解析
    acc += s.area(); // 编译期静态解析
}
```

使用方式：

```cpp
std::vector<CircleStatic> circles;
std::vector<RectangleStatic> rects;

circles.emplace_back(1.0);
circles.emplace_back(2.5);
rects.emplace_back(2.0, 3.0);

double total_area = 0.0;
for (const auto& c : circles) {
    render_and_accumulate(c, total_area);
}
for (const auto& r : rects) {
    render_and_accumulate(r, total_area);
}
```

### 3.1 静态多态的关键特性

- **模板参数决定类型**：
  - `render_and_accumulate<CircleStatic>` 与 `render_and_accumulate<RectangleStatic>` 是两个不同的函数实例。
- **无虚表、无间接调用**：
  - 调用在编译期就解析为“直接调用 `CircleStatic::draw`”，可以被内联。
- **duck typing**：
  - 只要类型提供了 `draw()` 和 `area()`，就可以作为 `Shape` 使用，并不要求继承某个基类。

### 3.2 静态多态的优点

- **零运行时开销**：
  - 没有虚调用，没有 vptr，调用路径和手写调用几乎一致。
- **优化空间更大**：
  - 内联、常量传播、死代码消除等都可以跨调用展开。
- **对类型不侵入**：
  - 现有类型只要接口满足要求，无需继承某个基类，也无需加 `virtual`。

### 3.3 静态多态的局限

- **类型集合在编译期“写死”**：
  - 不能在运行时随意组装“装着各种未知派生类”的 `std::vector<Shape>`。
  - 通常需要为每种具体类型单独存容器，或用 `std::variant` 之类额外结构。
- **接口约定“隐式”**：
  - 依靠模板实例化时报错来发现某个类型缺少 `draw()` / `area()`；
  - 不像基类那样有显式的虚函数签名可检查。

---

## 四、从“编译期/运行期 + 性能模型”的角度统一看

### 4.1 静态多态（compile-time polymorphism）

- **实现手段**：模板、函数重载、CRTP、`std::variant` 等。
- **分派时机**：在 **编译期** 完成分派，编译器在实例化时就确定具体类型和调用目标。
- **优化空间**：
  - 可以完全内联、消除抽象；
  - 易于做跨函数优化（常量传播、向量化、死代码消除等）。
- **本质**：**“类型驱动的代码生成”**——选择前移到编译期。
- **代价**：
  - 代码膨胀：每种具体类型一份实例；
  - 编译时间增加；
  - 没有统一基类约束，依赖 concept/鸭子类型，错误在编译期暴露（有时报错信息会比较“模板味”）。

### 4.2 动态多态（run-time polymorphism）

- **实现手段**：虚函数（vtable）、接口类 + 继承。
- **分派时机**：在 **运行期** 完成分派，对象持有 vptr，调用时经由 vtable 做一次间接跳转。
- **优点**：
  - 接口稳定、代码体积相对紧凑；
  - 支持运行期扩展（插件/模块化/配置驱动）。
- **缺点**：
  - 每次调用有间接开销（加载函数指针 + 跳转，可能分支预测失败）；
  - 优化受限：难以内联，跨边界优化能力弱。

### 4.3 性能模型对比

- 静态多态调用 ≈ 普通函数调用：
  - 在启用优化的构建里，往往被内联，开销接近零。
- 动态多态调用 = 一次间接调用：
  - 多一层 indirection，对 instruction cache / branch predictor 不如静态分派友好；
  - 在热路径/高频调用场景下差异会被放大。

一句话：

- 静态多态是“**编译期把选择做完，换取运行期性能**”；
- 动态多态是“**运行期再选择，换取系统灵活性与可扩展性**”。

---

## 五、什么时候用动态多态，什么时候用静态多态？

可以用两个问题来快速判断：

### 5.1 类型集合是否在编译期封闭？

- 如果类型集合**基本固定且已知**，且不需要在运行时随意注入新类型：
  - 静态多态常常更适合（模板/CRTP/`std::variant`）。
- 如果类型集合**开放**，语义上更像“插件”：
  - 典型是基类 + `virtual`，或者运行时注册表。

### 5.2 调用方关注的是“接口”还是“性能/优化”？

- 如果调用方通过统一抽象接口工作（如 GUI 控件、绘制接口、日志后端）：
  - 动态多态更自然，扩展性好，接口稳定。
- 如果调用处是性能敏感路径（热循环、数值内核、序列化/反序列化）：
  - 静态多态可以让编译器做更多优化，减少虚调用和分支预测成本。

### 5.3 常见“混用”模式

- 在 **性能关键路径内部** 使用静态多态：
  - 算法、执行引擎内部使用模板/CRTP 提供高性能实现；
- 在 **系统边界** 使用动态多态或 type erasure：
  - 暴露一个统一抽象（接口类或 type-erased wrapper），对外隐藏模板细节；
  - 内部仍然是模板/静态多态的世界。

这样可以同时拿到：

- 外层：接口稳定、可扩展、调用方简单；
- 内层：靠静态多态把性能榨干。 

---

## 六、结合你的两个示例文件来看

- `dynamic_virtual_example.cpp`：
  - 展示典型 OOP 风格：`Shape` 基类 + `Circle`/`Rectangle` 派生类。
  - 适合“渲染管线里有一堆异构图元”的场景：API 只认 `Shape&`。

- `static_polymorphism_example.cpp`：
  - 展示“接口靠模板约定”的静态多态：`CircleStatic` / `RectangleStatic` 满足同样接口。
  - `render_and_accumulate` 对调用者来说是通用的，对编译器来说是“具体化后的直接调用”。

这两个例子放在一起，可以帮你在脑子里建立这样一张图：

- 动态多态：**类型通过继承关系连接起来，接口显式，调用晚绑定**。
- 静态多态：**类型通过模板参数连接起来，接口隐式（duck typing），调用早绑定**。

理解了这两种工具的 trade-off，之后在写库/框架/性能敏感代码时，就能更有意识地选用“哪一种多态”来表达抽象。 

