## gRPC 入门：原理与用法一图景

在现代 C++ / 后端系统里，gRPC 几乎是默认的“跨进程 RPC 协议栈”之一。  
它的目标是：**高性能、跨语言、强类型的远程调用**。

从抽象视角看，你可以把 gRPC 理解为三层叠加：

- **IDL + 代码生成**：用 `.proto` 文件定义消息类型与服务接口，gRPC 工具链为各语言生成 stub 代码；
- **传输协议**：基于 HTTP/2 的二进制帧、流式传输、压缩等能力；
- **运行时库**：封装连接管理、负载均衡、重试、deadline、metadata 等常见 RPC 需求。

---

## 一、gRPC 的核心理念：IDL 驱动的远程调用

在没有 gRPC 的时代，一个服务调用另一个服务通常要自己解决：

- 请求/响应的序列化格式（JSON？自定义二进制？）
- HTTP 方法和 URL 的约定
- 错误码和重试语义
- 客户端 SDK 的封装与维护

gRPC 的做法是：**把这些约定固化进一套统一的模型**，让你用一个 `.proto` 文件说清楚：

- 这个服务里有哪些 RPC 方法；
- 每个方法的请求消息、响应消息长什么样；
- 是一问一答的 unary，还是流式的 streaming。

然后它用代码生成器帮你在各个语言里生成：

- 服务端接口（你只实现业务逻辑）；
- 客户端 stub（像本地函数一样发起远程调用）。

---

## 二、协议栈：gRPC 究竟跑在什么之上？

gRPC 的典型协议栈可以简化为：

> 应用层（你的业务）  
> ↓  
> gRPC Runtime（C++ / Go / Java / … 实现）  
> ↓  
> HTTP/2（二进制帧、流、多路复用）  
> ↓  
> TLS（可选）  
> ↓  
> TCP

其中几件事情非常关键：

- **HTTP/2 多路复用**：同一 TCP 连接上可以并发多个独立的 stream（每个 stream 就是一条 RPC），避免了 HTTP/1.1 的队头阻塞问题；
- **二进制帧**：相比文本协议更易于高效解析和压缩；
- **ProtoBuf 序列化**：消息体通常使用 Protocol Buffers，既紧凑又有强类型描述。

这意味着：从性能和带宽利用率上，gRPC 通常优于传统的 REST+JSON 方案，尤其是在高并发场景下。

---

## 三、Proto 文件：先定义“合同”，再写代码

使用 gRPC 的第一步，不是写 C++ 代码，而是写一个 `.proto` 文件。  
它同时承担了三种角色：

- 类型系统（定义消息结构）；
- 接口定义（定义服务与方法）；
- 跨语言协议（所有语言共用这一份说明书）。

一个最小的例子大致如下（简化示意）：

```proto
syntax = "proto3";

package example;

service Greeter {
  rpc SayHello (HelloRequest) returns (HelloReply);
}

message HelloRequest {
  string name = 1;
}

message HelloReply {
  string message = 1;
}
```

定义完 `.proto` 后，你会用 `protoc` + gRPC 插件生成 C++ 代码：

- `xxx.grpc.pb.h` / `xxx.grpc.pb.cc`：gRPC 服务与客户端 stub；
- `xxx.pb.h` / `xxx.pb.cc`：普通的 ProtoBuf 消息类型。

---

## 四、服务端：实现接口，而不是处理 socket

在 C++ 侧，生成的 gRPC 代码通常会给你一个抽象基类，类似（伪代码）：

```cpp
class Greeter::Service {
public:
  virtual Status SayHello(ServerContext* ctx,
                          const HelloRequest* req,
                          HelloReply* resp);
};
```

你要做的事情只是：

- 继承这个基类；
- 实现每个 RPC 方法的业务逻辑（读取请求、填充响应、返回状态）。

至于：

- 如何监听端口；
- 如何接收/解析请求；
- 如何在 HTTP/2 stream 里调度；
- 如何处理连接的生命周期；

这些都由 gRPC runtime 帮你完成。你更像是在“填表”，告诉 gRPC：

- 对于这个方法，收到了消息以后我要做什么。

---

## 五、客户端：像本地函数一样调用远程服务

在同一个 `.proto` 基础上，gRPC 也会为客户端生成 stub 类。  
在 C++ 侧，通常长这样（伪代码）：

```cpp
std::unique_ptr<Greeter::Stub> stub =
    Greeter::NewStub(grpc::CreateChannel("server:port", creds));

HelloRequest req;
req.set_name("world");
HelloReply resp;
ClientContext ctx;

Status s = stub->SayHello(&ctx, req, &resp);
```

在你的 C++ 代码里，这看起来就像一次普通函数调用：

- 你构造请求对象；
- 调用 `stub->Method(...)`；
- 拿到响应对象和一个 `Status`。

底层的连接管理、序列化、重试（若你配置了）、deadline 等，都被封装在 gRPC runtime 内部。

---

## 六、gRPC 的调用模式：不止一问一答

gRPC 支持四种调用模式，它们都基于 HTTP/2 的 stream。

1. **Unary RPC**（一问一答）  
   - 最常见的模式：一个请求，一个响应；
   - 对使用者来说很接近传统函数调用。

2. **Server streaming**  
   - 客户端发一次请求，服务器返回一个“响应流”；
   - 适合订阅型场景：例如订阅行情、日志推送、数据流。

3. **Client streaming**  
   - 客户端发送一个“请求流”，服务器在结束时返回一个总结响应；
   - 适合上传批量数据，例如批量上报 metrics。

4. **Bidirectional streaming**  
   - 双向都是流；客户端和服务器都可以在同一条连接上持续读写；
   - 适合聊天、交互式会话、复杂协调协议。

在 C++ API 中，这些模式分别对应不同形态的 stub 方法和 `Reader` / `Writer` / `ReaderWriter` 对象。

---

## 七、重要运行时概念：deadline、metadata、状态码

一些 gRPC 的运行时概念，在工程里很关键：

- **Deadline / Timeout**
  - 每个 RPC 都可以设置 deadline（如 200ms 内必须返回）；
  - gRPC 会在超时后主动取消请求，并返回相应状态；
  - 这是构建“可预期的延迟上界”和“端到端超时控制”的基础。

- **Metadata**
  - 类似 HTTP 头，可以随请求/响应携带额外键值对；
  - 常用于传输认证信息、trace id、租户信息等。

- **Status / 错误码**
  - gRPC 自带一套标准错误码（OK、NOT_FOUND、UNAVAILABLE、DEADLINE_EXCEEDED 等）；
  - 你可以在 Status 中附加人类可读的信息；
  - 工程里通常会把“业务错误码”二次封装在 payload 或 metadata 里。

这些机制一起构成了“RPC 语义层”的合同，让你能从 C++ 代码里可靠地观察远程调用是否成功、失败、超时、被取消。

---

## 八、在 C++ 项目里使用 gRPC 的典型步骤

综合来看，一个 C++ 项目如果要引入 gRPC，大致会经历这些步骤：

1. **设计服务与消息模型**
   - 用 `.proto` 文件描述 service / rpc / message；
   - 在这个阶段就要考虑版本演进（字段号、可选字段、默认值）。

2. **生成代码**
   - 在构建系统（CMake 等）里集成 `protoc` + `grpc_cpp_plugin`；
   - 生成 `*.pb.cc` / `*.grpc.pb.cc` 并编译进工程。

3. **实现服务端逻辑**
   - 继承生成的 `Service` 基类；
   - 实现每个方法的业务逻辑；
   - 配置并启动 gRPC 服务器（线程模型、端口、凭证等）。

4. **实现客户端调用**
   - 创建 `Channel`（含负载均衡/证书配置）；
   - 构造 stub，像调用本地函数一样发起 RPC；
   - 合理设置 deadline、重试策略和错误处理。

5. **运维与观测**
   - 在 metadata 里透传 trace id / span id；
   - 利用 gRPC 自带的统计接口或在中间层打点；
   - 结合负载均衡、熔断、限流构建完整的 service mesh（常见搭档：Envoy、Istio 等）。

---

## 九、小结：什么时候用 gRPC，什么时候不用？

**适合用 gRPC 的情况：**

- 服务间调用频繁、对性能和延迟敏感；
- 需要强类型、跨语言、统一治理（负载均衡、重试、超时）的 RPC；
- 服务拓扑稳定，有专门的服务发现/配置中心；
- 整体技术栈已经有 gRPC 生态（监控、trace、sidecar 等）。

**可以不必上 gRPC 的情况：**

- 对性能要求不高，更倾向于开放/通用的 HTTP+JSON API；
- 主要面向浏览器或不易使用 HTTP/2 的客户端；
- 场景非常简单，不值得引入额外的运行时与生成工具链。

理解 gRPC 的最好方式，是同时带着“协议栈视角”和“编译器视角”看它：

- 从协议栈看：它是跑在 HTTP/2 + ProtoBuf 上的一套 RPC 语义；  
- 从编译器视角看：`.proto` 就是跨语言的“类型和接口合同”，生成的 C++ 代码只是这份合同的一个具体实现。

