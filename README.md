# 航空客运订票系统

数据结构课程设计 · 第 6 题。C++17 实现核心业务与数据结构，HTTP + JSON 提供接口，零构建静态页面做前端。

> **运行环境说明**：本作业要求 Windows 环境。日常开发在 macOS 上进行（Apple Silicon + clang），**提交与答辩以 Windows 构建为准**，见下方[「在 Windows 上运行」](#在-windows-上运行提交环境)。CMakeLists 已做双平台适配，同一份代码两边都能编。

## 功能

**乘客端**

- 查询航线：按起点**或**终点检索，输出航班号、机型、飞行日、最近班次日期与分舱位余票
- 订票：有余票直接出票并分配座位号；余票不足时返回剩余量，可选择改张数重试、**登记候补**或查看同目的地推荐
- 退票：按航班 + 日期 + 身份证定位订单，支持整单退或按张部分退；退票后自动撮合候补
- 选座：订票时可指定座位，也可留空自动分配

**管理员端**

- 新建航线（航班号唯一性、各舱位定额不超物理座位等校验）
- 修改航线（新定额不得低于该舱位已售最大值；有已售票时不允许换机型）
- 查看订票记录（按航班 / 日期 / 证件过滤）
- 模拟登机：超卖调整报告（升舱名单 + 强制退票名单），完成后航班锁定

**三项扩展（对应题目【扩展要求】）**

1. 订票不满足时推荐到达同一目的地的其他航线（未来 3 天内有票者）
2. 舱位级超卖 + 登机调整：经济舱超卖旅客升入商务舱、商务舱升入头等舱，仍无法安置的按**最后订票先退**强制退票
3. 图形化座位图（3-3 布局、走道、行号，已售 / 可选 / 已选三色），支持订票时选座或不选座

## 构建与运行

依赖：CMake ≥ 3.20 和任意 C++17 编译器（clang / MSVC / gcc）。第三方库只有两个 header-only 文件，已放在 `third_party/`，无需安装。

```bash
cmake -S . -B build
cmake --build build
./build/airline            # 默认 0.0.0.0:8080
```

启动后浏览器打开 <http://localhost:8080>。

| 启动参数 | 默认 | 说明 |
|---|---|---|
| `--port` | 8080 | HTTP 端口 |
| `--web` | `web` | 静态资源目录 |
| `--state` | `state.json` | 状态文件（不存在时自动灌入内置种子） |

Windows 环境是提交与答辩的正式环境，完整的安装、构建、双击运行、防火墙、打包步骤见下文[「在 Windows 上运行」](#在-windows-上运行提交环境)。

**重置演示数据**：删除 `state.json` 后重启即可恢复干净种子（18 条航线、4 种机型）。

## 在 Windows 上运行（提交环境）

### 第 1 步：装工具（一次性）

1. 安装 **Visual Studio 2022 Community**（免费）：[visualstudio.microsoft.com](https://visualstudio.microsoft.com/zh-hans/)
   安装器里必须勾选 **"使用 C++ 的桌面开发"** 工作负载（这一项会带上 MSVC 编译器和 CMake）。
2. 不需要单独装 CMake——VS 2022 自带；也不需要联网装任何库，`third_party/` 里已经带齐。

### 第 2 步：获取代码

在任意目录用 git 拉取（Windows 10+ 自带 git 或随 Git for Windows 安装）：

```bat
git clone https://github.com/yulabu/airline.git
cd airline
```

目录结构保持完整即可：

```
airline\
├── CMakeLists.txt
├── third_party\        ← 必须带上（httplib.h、nlohmann\json.hpp）
├── src\
├── web\                ← 必须带上（界面全部在这里）
├── tests\
└── data\（如有）
```

`build\`、`state.json` 不在仓库里——Windows 上重新构建，状态文件首次运行自动生成。

### 第 3 步：构建

从开始菜单打开 **"x64 Native Tools Command Prompt for VS 2022"**（开发者命令行），`cd` 进项目目录：

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

成功后得到两个文件：

- `build\Release\airline.exe` —— 主程序
- `build\Release\airline_tests.exe` —— 测试程序

> 也可以在 VS 2022 里选「打开本地文件夹」直接打开项目目录，VS 会自动识别 CMakeLists 并提供 F7 构建 / F5 调试。

### 第 4 步：运行

**方式一（推荐演示用）**：把 `build\Release\airline.exe` 复制到项目根目录（和 `web\` 同级），然后直接**双击**——

```
数据结构课程设计\
├── airline.exe         ← 双击它
├── web\
└── state.json          ← 首次运行自动生成
```

控制台会打印启动横幅（班级 / 学号 / 姓名）并保持运行（这个黑窗口就是服务器，别关），浏览器访问 <http://localhost:8080>。

**方式二**：在开发者命令行里运行，方便看日志、传参数：

```bat
build\Release\airline.exe --port 8080 --web web
```

| 启动参数 | 默认 | 说明 |
|---|---|---|
| `--port` | 8080 | 端口被占用时换一个，如 `--port 9090` |
| `--web` | `web` | 静态资源目录（相对当前工作目录） |
| `--state` | `state.json` | 状态文件路径 |

### 第 5 步：验证

```bat
build\Release\airline_tests.exe
```

全部通过时最后一行是 `… checks, 0 failures` 且退出码为 0。

### 首次运行的防火墙弹窗

Windows 首次运行会弹「Windows 安全中心警报」，因为程序要监听本地端口。**勾选"专用网络"并点"允许访问"**。程序只监听本机、不联外网，弹窗只是 Windows 对监听端口的例行询问。

### 常见问题

| 现象 | 原因与解决 |
|---|---|
| 双击后窗口一闪而过 | 多半是 `web\` 目录没和 exe 放一起。用方式二在项目根目录的命令行里跑，能看到具体报错 |
| 浏览器打开是空白 / 404 | `--web` 路径不对。确认 exe 和 `web\` 同级，或显式传 `--web web` |
| 提示端口被占用 | 换端口：`airline.exe --port 9090` |
| 控制台中文乱码 | CMakeLists 已对 MSVC 强制 `/utf-8`，正常不会出现；若在旧版 cmd 里乱码，执行 `chcp 65001` 后再运行 |
| 杀软报隔离 | 若使用 MinGW 交叉编译的 exe 可能误报；用 VS 2022（MSVC）构建的 exe 无此问题，**提交件务必用 MSVC 构建** |

### 交付打包（光盘 / 目录）

按指导书要求，每题一个子目录，内含：

```
CZ6_航空订票_班级_学号_姓名\
├── src\  third_party\  web\  tests\  CMakeLists.txt   ← 源码
├── airline.exe                                         ← Release 可执行文件
├── state.json（可选，演示现场数据）
├── 测试用例.md（或演示脚本）
└── 课程设计报告.docx
```

## 运行测试

```bash
./build/airline_tests      # 全部通过时退出码 0
```

测试只链接 `airline_core`（不含 HTTP / JSON），覆盖顺序表、链队列、哈希索引、日期运算、座位分配、候补撮合、登机调整计划，以及订票 → 退票 → 候补 → 登机的 service 层全链路。

## 架构

依赖只能向下；`airline_core` 刻意不链接任何第三方库，是"算法自包含"的构建级证据。

```
web/      静态页面            只渲染，不做业务计算
────────────────────────────────  HTTP + JSON
api/      路由 + 格式校验      只翻译，不判断业务
────────────────────────────────  函数调用
service/  用例编排             唯一能改状态的地方（校验段 → 变更段）
repo/     内存聚合 + 两级索引  航班号→航线、(航班号,日期)→航班实例
────────────────────────────────  airline_core ────────────────
domain/   实体 + 纯策略函数    座位分配 / 候补撮合 / 登机调整计划
ds/       手写数据结构         SeqList / LinkQueue / HashIndex
base/     Status / Date / Result
────────────────────────────────
store/    JSON 持久化（唯一碰第三方库的业务模块，与 api 同属外层）
```

| CMake target | 内容 | 第三方依赖 |
|---|---|---|
| `airline_core` | base / ds / domain / repo / service | **无** |
| `airline_api` | store / api | cpp-httplib、nlohmann/json |
| `airline` | main | — |
| `airline_tests` | 单元测试 | 无（只链 core） |

## 目录结构

```
├── CMakeLists.txt
├── third_party/            httplib.h、nlohmann/json.hpp（header-only）
├── src/
│   ├── base/               错误码、日期运算、Result<T>
│   ├── ds/                 顺序表 / 链队列 / 哈希索引（自研，报告分析对象）
│   ├── domain/             实体与业务规则
│   │   └── policies/       余票 / 座位分配 / 候补撮合 / 登机调整 / 配置校验
│   ├── repo/               仓储：聚合、两级哈希索引、序号生成、惰性实例化
│   ├── service/            用例：查询 / 订票 / 退票 / 候补 / 管理 / 登机 + 种子
│   ├── api/                HTTP 路由与 JSON 编解码
│   ├── store/              state.json 原子读写
│   └── app/                main（启动参数、状态恢复、横幅）
├── web/                    index.html + css + js（零构建）
└── tests/                  test_main.cpp（ds/domain）、test_service.cpp（用例链路）
```

## HTTP 接口

统一响应信封：`{"ok":true,"data":…}` 或 `{"ok":false,"code":"NO_SEAT","msg":"…"}`。

| 方法 | 路径 | 说明 |
|---|---|---|
| GET | `/api/meta` | 机型清单、舱位枚举、服务器日期 |
| GET | `/api/search?from=&to=&weekday=` | 查询航线（from/to 至少一个） |
| GET | `/api/flights/{no}/instances?n=7` | 未来 n 个班次的分舱余票 |
| POST | `/api/orders` | 订票 `{flight_no,date?,name,id_card,cabin,qty,seat?}` |
| POST | `/api/waitlist` | 登记候补（订票失败后的二次确认） |
| POST | `/api/refund` | 退票 `{flight_no,date?,id_card,qty?}`，qty=0 整单退 |
| GET | `/api/recommend?to=&date=&exclude=` | 同目的地其他航线 |
| GET | `/api/seats?flight_no=&date=&cabin=` | 座位图 |
| POST | `/api/admin/routes` | 新建航线 |
| PUT | `/api/admin/routes/{no}` | 修改航线 |
| GET | `/api/admin/records?flight_no=&date=&id_card=` | 订票记录 |
| POST | `/api/admin/checkin` | 模拟登机，返回调整报告 |

错误码 → HTTP：`INVALID_ARG`→400，`NOT_FOUND/ORDER_NOT_FOUND`→404，`NO_SEAT/SEAT_TAKEN/DUPLICATE_ORDER/FLIGHT_EXISTS/QUOTA_BELOW_SOLD`→409，`FLIGHT_LOCKED`→423，其余→500。

## 业务规则（文档留白处的自行约定）

以下规则指导书未明确规定，为保持一致性自行定义，报告"需求分析"章节需说明：

1. **库存按"航班 × 日期"建模**：依据是题目"某天已售 40 张""根据日期、航班退票"两处表述；航线只存静态信息，余票、订单、候补都挂在航班实例上（首次访问时惰性创建）
2. **余票与可售余量分离**：余票 = `max(0, 定额−已售)`，对外展示不为负；可售余量 = `定额+超卖上限−已售`，订票判断用它
3. **超卖是舱位级的**：总票数超卖时升舱无意义（所有舱都满），只有局部超卖才能通过升舱消化，因此超卖上限按舱位设定
4. **超卖订单没有座位号**：实体座位是有限资源，超卖部分标记"待定"，登机时通过升舱或强制退票消化——这正是扩展 2 存在的原因
5. **候补按舱位分队列**，撮合策略为"首个能满足者优先"而非严格 FIFO（依据题目"若所退票额能满足该客户的要求……否则依次询问其他"）
6. **订单粒度为一证一航班一单**，重复订票直接拒绝（`DUPLICATE_ORDER`）
7. **退票支持部分退**（题目"所退票额"是数量语义）；释放的票额在当次调用内撮合候补
8. **登机后航班锁定**，不能再订票 / 退票 / 候补
9. **换机型约束**：该航线任一班次已有售票时禁止更换机型（物理座位布局会变）
10. **票价快照**：订单保存下单时价格，管理员改价不影响历史订单

## 数据结构选择（对应报告"概要设计"）

| 结构 | 实现 | 用途 | 理由 |
|---|---|---|---|
| `SeqList`（顺序表） | 自研，倍增扩容 | 航线表、订单表 | 查询远多于中间插入 |
| `LinkQueue`（链队列） | 自研，带头尾指针 | 候补队列（每舱位一条） | 队首出队 O(1)，长度不定；`remove_at` 支持"跳过队首"撮合 |
| `HashIndex`（哈希索引） | 自研，FNV-1a + 拉链法 + 自动扩容 | 航班号索引、(航班号,日期) 复合键索引 | 查找 O(1)；存下标而非指针，避免顺序表扩容失效 |
| 小顶堆 / 位图 | 数组位图 | 座位占用 | 布局固定，直接寻址 |

## 提交前检查清单

- [ ] `src/app/main.cpp` 与 `web/index.html` 顶部的班级 / 学号 / 姓名占位符改成真实信息
- [ ] **在 Windows（VS 2022 / MSVC）上完整构建并跑通一遍**——这是提交环境，macOS 上通过不算数
- [ ] 准备一份演示脚本：查询 → 订票（含选座）→ 订满触发超卖 → 候补 → 退票撮合 → 登机调整报告
- [ ] 报告七章（需求分析 / 概要设计 / 详细设计 / 调试分析 / 用户使用说明 / 测试结果 / 附录）
- [ ] 光盘目录：每题一个子目录，含源码、可执行程序（MSVC 构建的 exe）、测试用例、报告 WORD
