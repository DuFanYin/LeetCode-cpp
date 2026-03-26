## C++ 类型系统与多态体系（Type System & Polymorphism System）

C++ 的类型系统和 OOP 机制，不是一堆孤立关键字，而是一整套 **“用类型组织代码 + 在编译期/运行期选择行为”** 的系统。

这套系统核心在回答三个问题：

1. **如何用类型来组织复杂系统的代码结构？**（类、封装、继承）  
2. **如何让不同类型共享接口？**（抽象类、接口风格设计）  
3. **如何在编译期 / 运行期选择正确的实现？**（静态多态、动态多态、vtable 等）  

---

## 一、从系统视角看类型体系的层次

可以把 C++ 的类型 + 多态系统，粗分成三层：

1. **结构与访问控制层**：`class` / `struct`，`public` / `protected` / `private`（封装）  
2. **类型关系层**：继承（`public` / `protected` / `private`）、抽象类、接口风格设计  
3. **调用分发层**：静态多态（overload / templates）、动态多态（virtual / vtable / RTTI）  

所有关键词基本都可以往这三层里归位。

---

## 二、封装：控制能看到什么（Encapsulation）

**封装解决的问题：**

> 如何控制类的内部实现被外部访问的方式。

C++ 用三种访问控制关键字：

- `public`：类外可以访问  
- `protected`：类内部 + 子类可以访问  
- `private`：只有类内部可以访问  

示例：

```cpp
class OrderBook {
public:
    void add_order(int id, double price);
    void cancel_order(int id);
    void match();

private:
    // 订单存储结构、匹配细节完全隐藏
    std::vector<int>  order_ids_;
    std::vector<double> prices_;
};
```

要点：

- **隐藏实现细节，只暴露必要接口**；  
- 外部代码只能通过 `add_order` / `cancel_order` / `match` 访问行为，无法直接破坏内部状态。  

---

## 三、继承：建立类型层次（Inheritance）

**继承解决的问题：**

> 如何让多个类型共享代码与接口，并表达“是一种”的关系。

典型写法：

```cpp
class Instrument {
public:
    virtual ~Instrument() = default;

    void set_symbol(std::string s) { symbol_ = std::move(s); }
    std::string symbol() const { return symbol_; }

    virtual double price() const = 0;  // 纯虚函数：定义统一接口

protected:
    std::string symbol_;
};

class Stock : public Instrument {
public:
    double price() const override {
        return last_traded_price_;
    }

private:
    double last_traded_price_{0.0};
};

class Option : public Instrument {
public:
    double price() const override {
        return theoretical_price_;
    }

private:
    double theoretical_price_{0.0};
};
```

这里：

- `Stock` **is-a** `Instrument`；  
- `Option` **is-a** `Instrument`；  
- 通过 `public` 继承，子类对象可以被当成基类类型来使用。  

注意：`public` 继承表达“类型层次”；`protected` / `private` 继承更多是实现细节场景，使用频率远低很多。

---

## 四、多态的两种形态：编译期 vs 运行时（Polymorphism）

多态（Polymorphism）的核心含义：

> 同一个接口调用，在不同语境下表现出不同的行为。

在 C++ 中主要有两大类：

1. **编译期多态（compile-time polymorphism）**  
   - 函数重载（overloading）  
   - 模板（templates）  
2. **运行期多态（runtime polymorphism）**  
   - `virtual` 函数 + 基类指针/引用 + vtable  

编译期多态示例：

```cpp
int add(int a, int b) {
    return a + b;
}

double add(double a, double b) {
    return a + b;
}

template <typename T>
T add_t(T a, T b) {
    return a + b;
}
```

选择调用哪个重载 / 模板实例化，完全在 **编译期** 决定。

运行期多态的典型场景见下一节。

---

## 五、虚函数与运行时多态（Virtual Functions & Runtime Polymorphism）

仅有继承只能共享代码，还不足以实现：

> “在运行时根据对象真实类型选择正确实现。”

这正是 **虚函数（`virtual`）** 与 **运行时多态** 的职责。

继续使用 `Instrument` 体系：

```cpp
void print_price(const Instrument& inst) {
    std::cout << inst.symbol() << " : " << inst.price() << '\n';
}

int main() {
    Stock  s;
    Option o;

    s.set_symbol("AAPL");
    o.set_symbol("AAPL-CALL");

    print_price(s);  // 调用 Stock::price()
    print_price(o);  // 调用 Option::price()
}
```

虽然参数类型是 `const Instrument&`，但 `price()` 调用会在运行时根据真实类型分发，这就是：

- **runtime polymorphism（运行时多态）**
- **dynamic dispatch（动态绑定）**

---

## 六、vtable：虚函数在底层是怎么跑起来的

典型实现中，编译器会为有虚函数的类生成：

- **vtable（virtual table）**：存放虚函数对应的函数指针；  
- **vptr（virtual pointer）**：每个对象里有一个指针，指向自己所属类的 vtable。  

逻辑模型可以这样想象：

- `Instrument` 的 vtable：  
  - `price` → `Instrument::price`（或者纯虚占位）  
- `Stock` 的 vtable：  
  - `price` → `Stock::price`  
- `Option` 的 vtable：  
  - `price` → `Option::price`  

当执行：

```cpp
Instrument* p = new Stock{};
double v = p->price();
```

大致流程：

1. 通过 `p` 访问对象；  
2. 读取对象中的 `vptr`；  
3. 通过 `vptr` 找到该对象所属类的 vtable；  
4. 在 vtable 中定位到 `price` 对应的函数指针；  
5. 间接调用该函数指针。  

平时写代码不需要直接操作 vtable，只要理解：

- **有虚函数 → 对象多了一个指向 vtable 的指针 → 多态调用会有一次间接跳转的开销**。  

---

## 七、抽象类与接口风格设计（Abstract Class & Interface-Style Design）

**抽象类（abstract class）**：包含至少一个 **纯虚函数（pure virtual function）** 的类。

```cpp
class PricingModel {
public:
    virtual ~PricingModel() = default;

    virtual double price() const = 0;  // 纯虚函数
};
```

特点：

- 不能被直接实例化；  
- 专门用来 **定义接口**；  
- 具体行为由派生类实现。  

接口风格设计示例：

```cpp
class ExchangeGateway {
public:
    virtual ~ExchangeGateway() = default;

    virtual void send_order(/*...*/)   = 0;
    virtual void cancel_order(/*...*/) = 0;
};

class CmeGateway : public ExchangeGateway {
public:
    void send_order(/*...*/) override;
    void cancel_order(/*...*/) override;
};
```

这类只包含纯虚函数（或极少量非虚公共工具函数）的类，在 C++ 里就是实际意义上的 **interface**。

---

## 八、`override` / `final`：让重写更安全

C++11 引入：

- `override`：显式声明“这是在重写基类虚函数”；  
- `final`：禁止进一步重写，或禁止类被继承。  

示例：

```cpp
class Base {
public:
    virtual void foo(int) {}
};

class Derived : public Base {
public:
    void foo(int) override;   // 正确：确实重写了 Base::foo(int)
    // void foo(double) override; // 错误：签名不匹配，编译器会报错
};
```

这样可以防止“以为重写了，实际上只是新建了一个重载”的隐藏 bug。

---

## 九、`dynamic_cast` 与 RTTI：在运行时看真实类型

当你通过基类指针/引用操作对象时，有时候需要知道它 **真实指向哪种派生类**，这时可以用：

- RTTI（Run-Time Type Information）  
- `dynamic_cast`  

示例：

```cpp
void handle(Instrument* inst) {
    if (auto opt = dynamic_cast<Option*>(inst)) {
        // inst 实际上是 Option*
        // 这里可以使用 Option 特有的接口
    } else if (auto stk = dynamic_cast<Stock*>(inst)) {
        // inst 实际上是 Stock*
    } else {
        // 其他类型
    }
}
```

要点：

- `dynamic_cast` 依赖 RTTI 信息，只在有虚函数的类层次上有意义；  
- 指针版本转换失败返回 `nullptr`；引用版本转换失败会抛异常。  

---

## 十、object slicing：为什么“按值传基类”是危险的

**object slicing（对象切片）**：

> 把子类对象按值赋给基类对象时，子类特有部分会被“切掉”。

示例：

```cpp
struct Base {
    int x;
};

struct Derived : Base {
    int y;
};

void foo(Base b);  // 按值接收

int main() {
    Derived d;
    d.x = 1;
    d.y = 2;
    foo(d);  // 发生 slicing：y 信息丢失，只剩 Base 部分
}
```

因此，在使用多态的设计中：

- **不要按值传递基类对象**，应使用 `Base&` / `Base*` / 智能指针等引用语义。  

---

## 十一、多继承与菱形继承（Multiple Inheritance）

C++ 支持多继承：

```cpp
class InterfaceA {
public:
    virtual void fa() = 0;
    virtual ~InterfaceA() = default;
};

class InterfaceB {
public:
    virtual void fb() = 0;
    virtual ~InterfaceB() = default;
};

class Impl : public InterfaceA, public InterfaceB {
public:
    void fa() override;
    void fb() override;
};
```

在“多接口实现”的场景，多继承很自然。但如果多个父类之间也有继承关系，就会产生：

- **菱形继承（diamond problem）**：最顶层基类在派生类对象中出现多份副本。  

这时可以用：

- `virtual` 继承（`virtual inheritance`）来合并这几条路径上的同一个基类。  

实践中常见建议：

- 把多继承主要用在 **interface 层**（抽象基类多继承），  
- 具体实现层更多用 **单继承 + 组合**。  

---

## 十二、虚析构函数（Virtual Destructor）

一个非常重要的规则：

> 只要类中有任何虚函数，就应该提供 **虚析构函数**。

否则，通过基类指针 `delete` 派生类对象会产生未定义行为。

正确写法：

```cpp
class Instrument {
public:
    virtual ~Instrument() = default;  // 一旦有虚函数，析构也要 virtual

    virtual double price() const = 0;
};
```

原则：

- “打算以多态方式使用的基类” → 一定要有 `virtual ~Base()`。  

---

## 十三、这套类型系统与“对象语义 / 资源管理体系”的关系

之前另一份笔记讲的是：

- 值类别（lvalue / rvalue）  
- 复制语义 / 移动语义（copy / move semantics）  
- RAII、智能指针（`unique_ptr` / `shared_ptr` / `weak_ptr`）  

它们主要回答：

> **对象在内存中如何存在、如何被复制/移动、谁拥有资源、谁负责释放。**

而本篇的：

- 封装（encapsulation）  
- 继承（inheritance）  
- 虚函数 / 多态（virtual function / polymorphism）  
- vtable / dynamic dispatch / RTTI  

则主要回答：

> **类型之间是什么关系、接口如何抽象、在编译期/运行期如何选择具体实现。**

两个体系叠在一起，构成了完整的 C++ “对象世界”：

- 一边是 **value + ownership + lifetime**（对象语义与资源管理）；  
- 一边是 **type + interface + dispatch**（类型系统与多态）。  

理解它们各自负责哪一块、如何配合，你在设计类层次、接口、以及资源管理策略时就会有一个清晰的“全局地图”。 
