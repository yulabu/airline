/* 数据结构课程设计报告生成器
 * 用法: node make_report.cjs  (在 tools/ 目录下)
 * 封面个人信息读取 ../student.env（不入仓库）；输出 ../课程设计报告_*.docx
 */
const {
  Document, Packer, Paragraph, TextRun, Table, TableRow, TableCell,
  Header, Footer, PageNumber, NumberFormat, AlignmentType, HeadingLevel,
  WidthType, BorderStyle, ShadingType, SectionType, TableOfContents,
} = require("docx");
const fs = require("fs");
const path = require("path");

// ─────────────────────────── 个人信息 ───────────────────────────
function parseEnv(p) {
  const info = { CLASS: "【班级】", STUDENT_ID: "【学号】", NAME: "【姓名】" };
  try {
    for (const raw of fs.readFileSync(p, "utf8").split(/\r?\n/)) {
      const t = raw.trim();
      if (!t || t.startsWith("#")) continue;
      const eq = t.indexOf("=");
      if (eq < 0) continue;
      const k = t.slice(0, eq).trim();
      const v = t.slice(eq + 1).trim();
      if (k in info && v) info[k] = v;
    }
  } catch (e) { /* 文件不存在时用占位符 */ }
  return info;
}
const STU = parseEnv(path.join(__dirname, "..", "student.env"));
const DATE_STR = "2026年11月";

// ─────────────────────────── 基础常量与助手 ───────────────────────────
const BLACK = "000000";
const NB = { style: BorderStyle.NONE, size: 0, color: "auto" };
// 交付目标为 Windows Word（SimSun/SimHei 标准配置）。
// 本机 LibreOffice 预览时设 REPORT_FONT_SET=mac 换用系统字体，仅用于版面验证。
const MAC_FONTS = process.env.REPORT_FONT_SET === "mac";
const FONT_BODY = { ascii: "Times New Roman", eastAsia: MAC_FONTS ? "Songti SC" : "SimSun" };
const FONT_HEAD = { ascii: "Times New Roman", eastAsia: MAC_FONTS ? "Heiti SC" : "SimHei" };
const FONT_CODE = { ascii: "Courier New", eastAsia: MAC_FONTS ? "Songti SC" : "SimSun" };

function safeText(v, ph) {
  if (v === undefined || v === null || v === "" || String(v) === "undefined") {
    return ph || "【请填写】";
  }
  return String(v);
}

// 标题
function h1(text) {
  return new Paragraph({
    heading: HeadingLevel.HEADING_1,
    alignment: AlignmentType.CENTER,
    spacing: { before: 480, after: 360, line: 360 },
    children: [new TextRun({ text, bold: true, size: 32, color: BLACK, font: FONT_HEAD })],
  });
}
function h2(text) {
  return new Paragraph({
    heading: HeadingLevel.HEADING_2,
    spacing: { before: 360, after: 240, line: 360 },
    children: [new TextRun({ text, bold: true, size: 30, color: BLACK, font: FONT_HEAD })],
  });
}
function h3(text) {
  return new Paragraph({
    heading: HeadingLevel.HEADING_3,
    spacing: { before: 240, after: 120, line: 360 },
    children: [new TextRun({ text, bold: true, size: 28, color: BLACK, font: FONT_HEAD })],
  });
}
// 正文（首行缩进两字符，两端对齐，1.5 倍行距）
function body(text, opts = {}) {
  return new Paragraph({
    alignment: AlignmentType.JUSTIFIED,
    indent: { firstLine: 480 },
    spacing: { line: 360 },
    children: [new TextRun({ text, size: 24, color: BLACK, font: FONT_BODY, ...opts })],
  });
}
// 无缩进说明行（用于表题等）
function caption(text, keepNext = true) {
  return new Paragraph({
    alignment: AlignmentType.CENTER,
    keepNext,
    spacing: { before: 200, after: 120, line: 312 },
    children: [new TextRun({ text, bold: true, size: 21, color: BLACK, font: FONT_BODY })],
  });
}
// 有序/无序列表段落（悬挂缩进）
function li(text, mark) {
  return new Paragraph({
    alignment: AlignmentType.JUSTIFIED,
    indent: { left: 720, hanging: 360 },
    spacing: { line: 360 },
    children: [
      new TextRun({ text: mark + " ", size: 24, color: BLACK, font: FONT_BODY }),
      new TextRun({ text, size: 24, color: BLACK, font: FONT_BODY }),
    ],
  });
}
// 代码/伪码块：等宽 + 浅灰底 + 逐行段落
function codeBlock(lines) {
  return lines.map((line, i) => new Paragraph({
    alignment: AlignmentType.LEFT,
    shading: { type: ShadingType.CLEAR, fill: "F5F7FA" },
    indent: { left: 240 },
    spacing: { line: 264, before: i === 0 ? 120 : 0, after: i === lines.length - 1 ? 160 : 0 },
    children: [new TextRun({ text: line.length ? line : " ", size: 18, color: BLACK, font: FONT_CODE })],
  }));
}
// 三线表
function table3(headers, rows, widths) {
  const mkCell = (text, isHead, w) => new TableCell({
    width: w ? { size: w, type: WidthType.PERCENTAGE } : undefined,
    shading: isHead ? { type: ShadingType.CLEAR, fill: "F1F5F9" } : undefined,
    margins: { top: 60, bottom: 60, left: 120, right: 120 },
    borders: {
      top: isHead ? { style: BorderStyle.SINGLE, size: 6, color: BLACK } : NB,
      bottom: { style: BorderStyle.SINGLE, size: 4, color: BLACK },
      left: NB, right: NB,
    },
    children: [new Paragraph({
      alignment: AlignmentType.LEFT,
      spacing: { line: 276 },
      children: [new TextRun({ text, bold: !!isHead, size: 21, color: BLACK, font: FONT_BODY })],
    })],
  });
  return new Table({
    width: { size: 100, type: WidthType.PERCENTAGE },
    borders: {
      top: { style: BorderStyle.SINGLE, size: 8, color: BLACK },
      bottom: { style: BorderStyle.SINGLE, size: 8, color: BLACK },
      left: NB, right: NB, insideVertical: NB,
      insideHorizontal: { style: BorderStyle.SINGLE, size: 2, color: "C8D0D8" },
    },
    rows: [
      new TableRow({ tableHeader: true, cantSplit: true, children: headers.map((t, i) => mkCell(t, true, widths ? widths[i] : undefined)) }),
      ...rows.map(r => new TableRow({ cantSplit: true, children: r.map((t, i) => mkCell(t, false, widths ? widths[i] : undefined)) })),
    ],
  });
}

// ─────────────────────────── 封面 ───────────────────────────
function buildCover() {
  const infoRows = [
    ["学　　院", "计算机学院"],
    ["课程名称", "数据结构"],
    ["班　　级", safeText(STU.CLASS)],
    ["学　　号", safeText(STU.STUDENT_ID)],
    ["姓　　名", safeText(STU.NAME)],
    ["指导教师", "【指导教师】"],
    ["完成日期", DATE_STR],
  ];
  const infoTable = new Table({
    width: { size: 62, type: WidthType.PERCENTAGE },
    alignment: AlignmentType.CENTER,
    borders: { top: NB, bottom: NB, left: NB, right: NB, insideHorizontal: NB, insideVertical: NB },
    rows: infoRows.map(([label, value]) => new TableRow({
      cantSplit: true,
      children: [
        new TableCell({
          width: { size: 34, type: WidthType.PERCENTAGE },
          borders: { bottom: { style: BorderStyle.SINGLE, size: 4, color: BLACK }, top: NB, left: NB, right: NB },
          margins: { top: 60, bottom: 60, left: 120, right: 120 },
          children: [new Paragraph({
            alignment: AlignmentType.RIGHT,
            spacing: { line: Math.ceil(14 * 23), lineRule: "atLeast" },
            children: [new TextRun({ text: label + "：", size: 28, color: BLACK, font: FONT_HEAD })],
          })],
        }),
        new TableCell({
          borders: { bottom: { style: BorderStyle.SINGLE, size: 4, color: BLACK }, top: NB, left: NB, right: NB },
          margins: { top: 60, bottom: 60, left: 120, right: 120 },
          children: [new Paragraph({
            alignment: AlignmentType.CENTER,
            spacing: { line: Math.ceil(14 * 23), lineRule: "atLeast" },
            children: [new TextRun({ text: value, size: 28, color: BLACK, font: FONT_BODY })],
          })],
        }),
      ],
    })),
  });

  return [
    new Paragraph({ alignment: AlignmentType.CENTER, spacing: { before: 1400, after: 300, line: Math.ceil(22 * 23), lineRule: "atLeast" },
      children: [new TextRun({ text: "广东工业大学", size: 44, bold: true, color: BLACK, font: FONT_HEAD })] }),
    new Paragraph({ alignment: AlignmentType.CENTER, spacing: { after: 900, line: Math.ceil(18 * 23), lineRule: "atLeast" },
      children: [new TextRun({ text: "《数据结构》课程设计报告", size: 36, color: BLACK, font: FONT_HEAD })] }),
    new Paragraph({ alignment: AlignmentType.CENTER, spacing: { after: 160, line: Math.ceil(20 * 23), lineRule: "atLeast" },
      children: [new TextRun({ text: "航空客运订票系统", size: 40, bold: true, color: BLACK, font: FONT_HEAD })] }),
    new Paragraph({ alignment: AlignmentType.CENTER, spacing: { after: 1100, line: Math.ceil(14 * 23), lineRule: "atLeast" },
      children: [new TextRun({ text: "—— 线性表 · 队列 · 哈希索引 的综合应用 ——", size: 24, color: "333333", font: FONT_BODY })] }),
    infoTable,
  ];
}

// ─────────────────────────── 目录节 ───────────────────────────
function buildToc() {
  return [
    new Paragraph({
      alignment: AlignmentType.CENTER,
      spacing: { before: 480, after: 360 },
      children: [new TextRun({ text: "目　　录", bold: true, size: 32, color: BLACK, font: FONT_HEAD })],
    }),
    new TableOfContents("目录", { hyperlink: true, headingStyleRange: "1-3" }),
    new Paragraph({
      spacing: { before: 200 },
      children: [new TextRun({
        text: "注：本目录由域代码生成。若编辑后页码变化，请在目录上点击右键并选择\u201c更新域\u201d以刷新页码。",
        italics: true, size: 18, color: "888888", font: FONT_BODY,
      })],
    }),
  ];
}

// ─────────────────────────── 第一章 需求分析 ───────────────────────────
function ch1() {
  return [
    h1("第一章　需求分析"),
    h2("1.1　设计任务"),
    body("航空客运订票的业务活动包括查询航线、客票预订和办理退票等。本课程设计要求设计并实现一个航空客运订票系统：每条航线包含起点站名、终点站名、航班号、飞机号、飞行周日、乘员定额、分舱位（头等舱、商务舱、经济舱）的余票量与价格、已订票客户名单及等候替补客户名单；程序启动时须已建立不少于 15 条航线；系统分为乘客与管理员两类角色，分别提供查询、订票、退票与航线管理、记录查看等功能。"),
    body("在完成基本要求之外，本设计还实现了题目给出的全部三项扩展要求：订票不满足时推荐同目的地其他航线；允许舱位级机票超卖并设立模拟登机环节进行超卖调整；图形化显示飞机座位图并支持订票时选座。"),
    h2("1.2　功能需求"),
    caption("表 1-1　功能需求一览"),
    table3(
      ["角色", "编号", "功能", "说明"],
      [
        ["乘客", "①", "查询航线", "按起点站名或终点站名检索，输出航班号、飞机号、飞行周日、最近一班日期及分舱位余票"],
        ["乘客", "②", "承办订票", "按航班号与订票数额查询票额；有余票则出票并输出座位号；满员或余票不足时重新询问，可登记排队候补"],
        ["乘客", "③", "承办退票", "按日期、航班及身份证定位订单办理退票，随后扫描候补队列，按\u201c所退票额能满足者优先\u201d依次撮合"],
        ["管理员", "①", "新建航线", "录入航线与各舱位配置，进行合法性与唯一性校验"],
        ["管理员", "②", "修改航线", "修改时校验数据合法性：某舱位新定额不得低于该舱位已售最大值"],
        ["管理员", "③", "查看订票记录", "按航班号、日期、身份证过滤查看并打印全部订票记录"],
        ["扩展", "一", "航线推荐", "订票要求不能满足时，推荐到达同一目的地、近期有票的其他航线"],
        ["扩展", "二", "超卖与登机调整", "允许舱位级超卖；模拟登机时经济舱超卖旅客调整至商务舱、商务舱超卖旅客调整至头等舱，仍无法安置的按\u201c最后订票先退\u201d强制退票，打印调整与退票名单"],
        ["扩展", "三", "座位图与选座", "图形化显示飞机舱位与座位占用情况，订票时可选座也可不选"],
      ],
      [10, 8, 16, 66],
    ),
    h2("1.3　输入形式与取值范围"),
    caption("表 1-2　输入约定"),
    table3(
      ["输入项", "形式", "取值范围 / 约束"],
      [
        ["航班号", "字符串", "大写字母与数字，长度 1~7，全局唯一"],
        ["日期", "YYYY-MM-DD", "合法公历日期；订票可缺省，缺省取最近一班"],
        ["身份证号", "字符串", "固定 18 位，前 17 位为数字，末位可为数字或 X"],
        ["舱位", "枚举", "0 头等舱 / 1 商务舱 / 2 经济舱"],
        ["订票张数", "整数", "1~6；退票张数 0 表示整单全退"],
        ["座位号", "排号+字母", "如 12A；字母须属于该舱位布局（A~F）"],
        ["飞行周日", "7 位掩码", "bit0~bit6 依次表示周一至周日，至少一位"],
        ["舱位配置", "三组数值", "各舱位定额之和等于乘员定额，定额不超过该舱位物理座位数"],
      ],
      [18, 20, 62],
    ),
    h2("1.4　输出形式"),
    body("系统采用 B/S 形式的本地交互界面：后端以 JSON 返回结构化数据，前端渲染为表格与座位图。主要输出包括：航线查询卡片（含分舱位余票与票价表）、订票结果（订单号与座位号列表，超卖待定订单有明确标记）、候补登记位次、退票结果及被撮合候补者名单、管理员订票记录表、登机调整报告（升舱名单、强制退票名单、各舱位登机数与物理座位数对照）。所有操作结果同时写入运行日志文件。"),
    h2("1.5　文档留白处的自定义规则"),
    body("指导书对部分业务细节未作规定。为保证实现的一致性并在答辩中可解释，本设计显式约定以下规则："),
    li("库存按\u201c航班 × 日期\u201d建模。依据是指导书\u201c某航线某天已经售出 40 张\u201d\u201c根据日期、航班退票\u201d两处表述：余票、订单、候补均挂在航班实例上，航线只存静态信息。", "1."),
    li("余票与可售余量分离。对外展示余票 = max(0, 定额 − 已售)，不为负；订票判定用可售余量 = 定额 + 超卖上限 − 已售。", "2."),
    li("超卖按舱位独立设定阈值。若按总票数超卖，登机时所有舱位皆满，升舱无从谈起；只有局部超卖才能通过升舱消化，这正是扩展二能够成立的前提。", "3."),
    li("超卖订单没有实体座位号。座位是有限物理资源，超卖部分标记\u201c待定\u201d，登机时通过升舱或强制退票消化。", "4."),
    li("候补按舱位分队列；撮合策略为\u201c首个能满足者优先\u201d而非严格 FIFO，依据指导书\u201c若所退票额能满足该客户的要求……否则依次询问其他\u201d。", "5."),
    li("订单粒度为一证一航班一单，重复订票直接拒绝。", "6."),
    li("退票支持部分退（按\u201c所退票额\u201d的数量语义），当次释放的票额在当次调用内完成撮合。", "7."),
    li("登机完成后航班锁定，不能再订票、退票或候补。", "8."),
    li("有已售票的航线禁止更换机型（物理座位布局会变）；修改定额不得低于该舱位所有班次已售最大值。", "9."),
    li("订单保存下单时的票价快照，管理员改价不影响历史订单。", "10."),
  ];
}

// ─────────────────────────── 第二章 概要设计 ───────────────────────────
function ch2() {
  return [
    h1("第二章　概要设计"),
    h2("2.1　总体结构"),
    body("系统采用分层架构，依赖严格自上而下，任何一层不得反向或跨层依赖。核心算法与数据结构编译为独立的静态库 airline_core，不依赖任何第三方库；HTTP 与 JSON 编解码位于外层适配模块，属于可替换的\u201c壳\u201d。这一约束既符合课程对\u201c数据结构自行实现\u201d的要求，也使核心层可以脱离界面单独测试。"),
    caption("图 2-1　系统分层结构"),
    ...codeBlock([
      "web/       HTML + CSS + JS 静态页面（零构建）",
      " │ HTTP + JSON",
      " ▼",
      "api/       路由注册 · 参数格式校验 · JSON 编解码",
      " ▼",
      "service/   用例编排 · 校验段 → 变更段 · 全局互斥锁",
      " ▼",
      "repo/      状态聚合 · 两级哈希索引 · 惰性航班实例 · 序号生成",
      " ▼",
      "domain/    实体 · 不变量 · 纯策略函数（座位分配/撮合/登机调整）",
      " ▼",
      "ds/        SeqList · LinkQueue · HashIndex（全部自研）",
      " ▼",
      "base/      Status 错误码 · Date 公历日期 · Result<T>",
    ]),
    h2("2.2　数据结构选择"),
    body("按\u201c以数据结构为中心\u201d的原则划分模块，三个课程要求的核心结构全部自行实现："),
    caption("表 2-1　核心数据结构及其用途"),
    table3(
      ["结构", "实现要点", "用途", "选择理由"],
      [
        ["顺序表 SeqList", "连续存储，倍增扩容，支持按位插入删除", "航线表、订单表", "查询远多于中间插入，随机访问 O(1)；存储紧凑利于遍历展示"],
        ["链队列 LinkQueue", "带头尾指针的单链表，附 remove_at 按位删除", "候补队列（每舱位一条）", "队首出队 O(1)、长度不定；remove_at 支持撮合时\u201c跳过队首\u201d"],
        ["哈希索引 HashIndex", "FNV-1a 散列 + 拉链法，装填因子 3/4 自动扩容", "航班号→航线、(航班号,日期)→航班实例 两级索引", "定位 O(1)；存数组下标而非指针，避免顺序表扩容后指针失效"],
        ["座位占用位图", "unsigned char 三维数组 [舱位][排][字母]", "实体座位占用", "布局固定，直接寻址，置位/判位 O(1)"],
      ],
      [16, 28, 24, 32],
    ),
    h2("2.3　核心数据类型定义"),
    body("系统围绕五个实体组织数据，其关系为：航线 Route 1 : N 航班实例 FlightInstance，航班实例 1 : N 订单 Order 与候补 WaitEntry。关键字段如下："),
    caption("表 2-2　核心实体字段"),
    table3(
      ["实体", "关键字段", "说明"],
      [
        ["Route 航线", "flightNo, from, to, aircraft, weekdays, cabins[3]", "静态信息；cabins 含定额、超卖上限、票价"],
        ["FlightInstance 航班实例", "flightNo, date, sold[3], occupied 位图, orders, waitLists[3], phase", "库存与业务数据主体；首次访问时惰性创建"],
        ["Order 订单", "orderId, name, idCard, cabin, qty, refunded, seats[], seatPending, priceAtBooking, bookSeq, status", "seatPending 标记超卖待定；bookSeq 为强制退票依据"],
        ["WaitEntry 候补", "name, idCard, cabin, qty, joinSeq, joinTime", "joinSeq 决定同舱位队列次序"],
        ["CabinConfig 舱位", "quota, overbookMax, price", "quota 之和等于乘员定额"],
      ],
      [20, 42, 38],
    ),
    h2("2.4　主程序流程"),
    caption("图 2-2　主程序流程"),
    ...codeBlock([
      "启动 main",
      " ├─ 解析命令行参数（--port / --web / --state / --student）",
      " ├─ state 文件存在？ ── 是 → 反序列化恢复全部状态",
      " │                    └ 否 → 灌入内置种子（18 条航线）并落盘",
      " ├─ 读取 student.env（班级/学号/姓名，用于界面展示）",
      " ├─ 打印启动横幅（突出显示设计者信息）",
      " └─ 注册 HTTP 路由 → 监听 0.0.0.0:port → 循环处理请求",
      "      每个请求：解析 → 格式校验 → 调 service 用例 → 编码响应",
      "      变更类用例成功后 → 原子落盘（写临时文件再 rename）",
    ]),
    h2("2.5　模块调用关系"),
    body("一次订票请求自上而下穿越各层，每层只做一类事情：api 层只翻译（HTTP/JSON ↔ 命令对象），service 层只决策（校验与变更的编排），domain 层只计算（给定输入输出结论的纯函数），repo 层只存取。以订票为例的调用链为：routes_passenger → BookingService.book → Repository.get_or_create_instance → domain::avail_sellable → domain::seat_alloc → Repository.add_order → store::flush。"),
    h2("2.6　订单状态机"),
    caption("图 2-3　订单状态机"),
    ...codeBlock([
      "订票成功",
      "  ▼",
      "ACTIVE（有效）──┬── 部分退票 ──▶ PARTIAL_REFUND ── 退完 ──▶ REFUNDED",
      "               ├── 整单退票 ──▶ REFUNDED",
      "               ├── 登机 · 超卖待定 · 升舱成功 ──▶ UPGRADED（升舱后登机）",
      "               └── 登机 · 超卖待定 · 无座可调 ──▶ FORCE_REFUNDED（强制退票）",
      "",
      "  有座订单随航班登机自然完成，航班进入锁定态后订单保持有效；",
      "  锁定航班（FLIGHT_LOCKED）拒绝一切再变更。",
    ]),
    body("航班实例另有独立的售票中 / 已登机两阶段状态；进入已登机后，该实例的一切变更请求均被拒绝。"),
  ];
}

// ─────────────────────────── 第三章 详细设计 ───────────────────────────
function ch3() {
  return [
    h1("第三章　详细设计"),
    body("本章给出各核心操作的伪码算法。伪码采用类 C 风格，参数以 & 表示引用，与教材书写规范一致，详细程度达到可直接译写为高级语言程序。"),
    h2("3.1　余票计算"),
    body("余票与可售余量是两个语义不同的派生量，必须分开计算："),
    ...codeBlock([
      "int AvailDisplay(Route &R, FlightInstance &FI, int c) {",
      "    /* 对外展示的余票：不允许出现负数 */",
      "    return Max(0, R.cabins[c].quota - FI.sold[c]);",
      "}",
      "int AvailSellable(Route &R, FlightInstance &FI, int c) {",
      "    /* 可售余量：含超卖额度，订票判定专用 */",
      "    return Max(0, R.cabins[c].quota + R.cabins[c].overbookMax",
      "               - FI.sold[c]);",
      "}",
    ]),
    h2("3.2　座位分配"),
    body("座位分配支持两种模式：留空则自该舱位首位起顺次分配；指定座位则从指定位置起环形分配。核心约束是\u201c全有或全无\u201d——可用实体座位不足 qty 时整单置 assigned = 0，由调用方标记超卖待定，避免出现半张订单有座、半张无座的中间态："),
    ...codeBlock([
      "Status SeatAlloc(SeatLayout L, FlightInstance FI, int c,",
      "                 int qty, char pref[], char out[][4], int &assigned) {",
      "    if (qty < 1 || qty > MAXSEATS_PER_ORDER) return ERROR;",
      "    start = 舱位 c 的首排首座;",
      "    if (pref 非空) {                    /* 指定座位 */",
      "        if (!SeatParse(pref, &row, &letter))    return ERROR;",
      "        if (SeatCabinOf(L, row, letter) != c)   return CABINMISMATCH;",
      "        if (!FI.IsFree(c, row, letter))         return SEATTAKEN;",
      "        start = (row, letter);",
      "    }",
      "    if (CountFree(L, FI, c) < qty) {    /* 全有或全无 */",
      "        assigned = 0;  return OK;       /* 整单转超卖待定 */",
      "    }",
      "    assigned = 0;",
      "    for (k = 0; k < 总座位数 && assigned < qty; ++k) {",
      "        (row, letter) = 自 start 环形推进的下一座位;",
      "        if (!FI.IsFree(c, row, letter)) continue;",
      "        SeatMake(row, letter, out[assigned]);  ++assigned;",
      "    }",
      "    return OK;",
      "}",
    ]),
    h2("3.3　候补撮合"),
    body("候补撮合不是严格 FIFO，而是\u201c首个能满足者优先\u201d：退 n 张票时，从队首起依次询问，跳过所需票数大于 n 的登记。该函数是纯函数，只报告首个满足项的位置，删除动作由调用方执行："),
    ...codeBlock([
      "bool WaitlistFind(LinkQueue<WaitEntry> &Q, int released,",
      "                  int &index, WaitEntry &out) {",
      "    /* released：本次退票释放的票额 */",
      "    i = 0;",
      "    for (e in Q 从队首到队尾) {",
      "        if (e.qty <= released) { index = i; out = e; return TRUE; }",
      "        ++i;                            /* 票额不够，跳过 */",
      "    }",
      "    return FALSE;",
      "}",
    ]),
    h2("3.4　订票用例"),
    body("所有变更类用例遵循统一的\u201c校验段 → 变更段\u201d两段式：全部可能失败的操作集中在只读的校验段完成，进入变更段后只执行必然成功的内存修改，从而天然保证原子性，无需回滚机制。"),
    ...codeBlock([
      "Status AirlineSystem::Book(BookCmd cmd, BookingOut &out) {",
      "    Lock();                       /* 全局互斥：串行化状态访问 */",
      "    /* ── 校验段（只读，可失败）────────────────── */",
      "    if (!LegalNameId(cmd))              return INVALIDARG;",
      "    R = FindRoute(cmd.flightNo);",
      "    if (!R)                             return NOTFOUND;",
      "    d  = cmd.date 非空 ? Parse(cmd.date) : R.NextDate(today);",
      "    FI = GetOrCreateInstance(cmd.flightNo, d);   /* 惰性创建 */",
      "    if (FI.phase == LOCKED)             return FLIGHTLOCKED;",
      "    if (FindOrderByIdCard(FI, cmd.idCard)) return DUPLICATEORDER;",
      "    if (AvailSellable(*R, FI, cmd.cabin) < cmd.qty) {",
      "        out.remaining = AvailSellable(...);   /* 附上下文 */",
      "        return NOSEAT;                        /* 前端转候补/推荐 */",
      "    }",
      "    st = SeatAlloc(L, FI, cmd.cabin, cmd.qty, cmd.seat, seats, &n);",
      "    if (st != OK) return st;",
      "    /* ── 变更段（必然成功）────────────────────── */",
      "    o = { 订单号=NextOrderSeq(), 票价快照=R.cabins[c].price,",
      "          bookSeq=序号, seatPending=(n==0) };",
      "    for (k = 0; k < n; ++k) FI.SetOccupied(c, seats[k], TRUE);",
      "    FI.orders.PushBack(o);    FI.sold[c] += cmd.qty;",
      "    Flush();                  /* 原子落盘：tmp + rename */",
      "    return OK;",
      "}",
    ]),
    h2("3.5　退票与候补撮合用例"),
    ...codeBlock([
      "Status AirlineSystem::Refund(RefundCmd cmd, RefundOut &out) {",
      "    Lock();",
      "    FI = FindInstance(cmd.flightNo, cmd.date);",
      "    if (!FI)                       return ORDERNOTFOUND;",
      "    if (FI.phase == LOCKED)        return FLIGHTLOCKED;",
      "    idx = FindOrderIndex(FI, cmd.idCard);",
      "    if (idx < 0)                   return ORDERNOTFOUND;",
      "    o = FI.orders[idx];",
      "    n = (cmd.qty <= 0 || cmd.qty >= o.qty) ? o.qty : cmd.qty;",
      "    /* ── 变更：从座位表尾部释放 n 张 ── */",
      "    for (k = 0; k < n; ++k) FreeSeat(o.seats[--o.seatCount]);",
      "    o.qty -= n;  o.status = (o.qty == 0 ? REFUNDED : PARTIALREFUND);",
      "    FI.sold[o.cabin] -= n;",
      "    /* ── 撮合前清理失效登记：持票者 / 同证重复登记 ── */",
      "    PurgeStale(FI, FI.waitLists[o.cabin]);",
      "    /* ── 同舱位候补撮合：首个能满足者优先 ── */",
      "    remaining = n;",
      "    while (remaining > 0) {",
      "        if (!WaitlistFind(queue, remaining, &widx, &e)) break;",
      "        QueueRemoveAt(queue, widx, &e);",
      "        SeatAlloc(L, FI, e.cabin, e.qty, \"\", ws, &wassigned);",
      "        w = 建订单(e, 票价快照, NextOrderSeq());",
      "        FI.orders.PushBack(w);  FI.sold[e.cabin] += e.qty;",
      "        out.promoted += w;      remaining -= e.qty;",
      "    }",
      "    Flush();",
      "    return OK;",
      "}",
    ]),
    h2("3.6　登机调整（超卖消解）"),
    body("登机调整先生成纯计划、后执行，计划本身不修改任何状态。消解顺序为：先清上方舱位（商务舱超卖者升头等舱，腾出商务舱座位），再接下方舱位（经济舱超卖者升商务舱），两步之后仍无座者按订票序号倒序列入强制退票名单："),
    ...codeBlock([
      "Status PlanCheckin(Route &R, FlightInstance &FI, CheckinPlan &plan) {",
      "    L = SeatLayoutFor(R.aircraft);",
      "    for c: plan.physical[c] = L.SeatsOf(c);",
      "    /* 1. 统计各舱位登机人数 */",
      "    for (o in FI.orders)",
      "        if (o.HoldsTickets()) plan.boarded[o.cabin] += o.qty;",
      "    /* 2. 收集无座订单，按订票序号倒序（最后订票者排最前）*/",
      "    for (o in FI.orders) if (o.seatPending) cand[o.cabin].Add(o);",
      "    SortDesc(cands);",
      "    /* 3. 商务舱超卖 → 头等舱有空位则整单升舱 */",
      "    Upgrade(BIZ, FIRST, boarded[BIZ] - physical[BIZ]);",
      "    /* 4. 经济舱超卖 → 商务舱空位（已腾出）则整单升舱 */",
      "    Upgrade(ECO, BIZ, boarded[ECO] - physical[ECO]);",
      "    /* 5. 仍无座者 → 强制退票名单（最后订票者最先退）*/",
      "    for c: for (x in cand[c]) if (!x.upgraded) plan.forfeits.Add(x);",
      "    return OK;",
      "}",
      "/* Upgrade(from, to, want)：把 from 舱的待定订单整单转移到 to 舱；",
      "   目标舱空位装不下整单时跳过该单、尝试张数更小的下一单，",
      "   空位耗尽即止。座位在计划内的位图副本上分配，成功后统一回写 */",
    ]),
    h2("3.7　函数调用关系"),
    caption("图 3-1　核心调用关系（订票 / 退票 / 登机）"),
    ...codeBlock([
      "api 层路由",
      " ├─ POST /api/orders ────▶ service::Book",
      " │                          ├─ repo::FindRoute          (哈希 O(1))",
      " │                          ├─ repo::GetOrCreateInstance(复合键哈希)",
      " │                          ├─ domain::AvailSellable    (纯函数)",
      " │                          ├─ domain::SeatAlloc        (纯函数)",
      " │                          └─ repo::AddOrder → store::Flush",
      " ├─ POST /api/refund ────▶ service::Refund",
      " │                          ├─ 释放座位 / 收缩订单",
      " │                          ├─ 清理失效候补登记",
      " │                          ├─ domain::WaitlistFind     (纯函数)",
      " │                          └─ 为候补建单 → store::Flush",
      " └─ POST /api/admin/checkin ▶ service::Checkin",
      "                             ├─ domain::PlanCheckin     (纯函数)",
      "                             ├─ 执行升舱（回写座位位图）",
      "                             ├─ 执行强制退票（按 bookSeq 倒序）",
      "                             └─ 锁定航班 → store::Flush",
    ]),
  ];
}

// ─────────────────────────── 第四章 调试分析 ───────────────────────────
function ch4() {
  return [
    h1("第四章　调试分析"),
    h2("4.1　调试过程中遇到的问题及解决"),
    body("开发过程中记录并解决了以下具有代表性的问题。为便于复查，每个问题按现象、定位、解决与启示四步整理。"),
    h3("问题一：候补撮合可能产生\u201c一人两单\u201d"),
    body("现象：构造\u201c先登记候补、后通过正常途径订到票\u201d的场景时，其后的一次退票撮合为该乘客再次建单，同一身份证在同一航班出现两张有效订单，违反概要设计确立的不变量。"),
    body("定位：join_waitlist 不校验登记人是否已持票；撮合前也不清理失效登记。两条路径叠加即可触发。"),
    body("解决：登记端增加三条守卫——有余票时拒绝候补、已持有效订单拒绝、同舱位队列重复登记拒绝；撮合端在扫描前清除\u201c已持票者\u201d与\u201c同证重复\u201d的登记。"),
    body("启示：不变量的维护点必须覆盖所有进入路径。候补队列的元素生命周期独立于订单，每次消费队列前都应重新验证其前提仍然成立。"),
    h3("问题二：座位图网格列数差一导致整图错位"),
    body("现象：浏览器中座位图每行数量逐行递减，图形整体呈阶梯状错位。"),
    body("定位：每行渲染单元为\u201c行号 + 座位数 + 走道\u201d共 letters+2 个，而 CSS 网格仅声明 letters+1 列，每行溢出一个单元触发换行。"),
    body("解决：列数改为 letters+2。"),
    body("启示：前端网格布局的列数必须与\u201c逻辑单元数\u201d而非\u201c业务元素数\u201d对齐；此类差一错误在纯数据检查中不可见，需要视觉验收才能暴露。"),
    h3("问题三：推荐航线包含已过去的日期"),
    body("现象：以当日为目标日期请求推荐，返回结果中出现三天前的班次日期。"),
    body("定位：推荐窗口原设计为目标日期 ±3 天，负向位移会命中过去日期，而过去班次对乘客毫无意义。"),
    body("解决：窗口改为目标日期起 3 天内（位移 0~3）。"),
    body("启示：时间窗口类需求必须显式声明\u201c是否含过去\u201d，不能把数学上的对称区间直接照搬为业务规则。"),
    h3("问题四：状态文件路径在加载与落盘两处不一致"),
    body("现象：以 --state 指定自定义路径启动时，程序从该路径加载，但落盘仍写死工作目录下的默认文件名，两个文件内容随即分叉。"),
    body("定位：持久化调用点在注册路由时以字面量写死文件名，未接收启动参数。"),
    body("解决：将状态路径作为参数贯穿注册函数，加载与落盘使用同一路径。"),
    body("启示：同一资源的\u201c读路径\u201d与\u201c写路径\u201d必须来自同一配置源；字面量重复是配置分裂的常见根源。"),
    h3("问题五：登机人数统计的语义歧义"),
    body("现象：单测断言登机计划中经济舱人数为 159（含超卖），实际返回 156，测试失败。"),
    body("定位：boarded 字段被设计为\u201c升舱调整后的最终分布\u201d，随升舱在舱位间转移；测试者按调整前的口径书写断言。"),
    body("解决：修正测试预期，并在代码注释中显式声明该字段为\u201c调整后\u201d语义。"),
    body("启示：跨舱位转移的统计量必须在接口注释中写明\u201c统计时点\u201d，否则测试者与实现者对同一字段各执一词。"),
    h3("问题六：修改定额校验的已售口径"),
    body("现象：航线某舱位已售 156 张（含超卖），管理员把定额改回 150 被拒绝，而改定额定额 156 则通过，直觉上\u201c定额\u201d似乎应可回退。"),
    body("定位：指导书规定\u201c某天已售 40 张，定额不能修改成 <40\u201d，已售口径是含超卖的实际售票量；定额低于已售意味着已售出的票失去定额依据。"),
    body("解决：维持\u201c新定额 ≥ 所有班次已售最大值\u201d的校验，并在使用说明中向管理员说明该约束的由来。"),
    body("启示：指导书给出的校验示例即验收口径，实现不得按直觉\u201c纠正\u201d；确需放宽时应先修改需求认知而非代码。"),
    h2("4.2　算法时空分析"),
    caption("表 4-1　基本操作时空复杂度（R 航线数，n 订单数，k 候补数，S 舱位座位数）"),
    table3(
      ["操作", "时间复杂度", "说明"],
      [
        ["航班号定位航线", "O(1) 均摊", "FNV-1a 哈希 + 拉链法，装填因子 3/4 触发倍增扩容"],
        ["(航班号,日期) 定位实例", "O(1) 均摊", "复合键哈希；惰性创建避免预生成 420 个班次"],
        ["按起点/终点查询航线", "O(R)", "顺序扫描并逐条组装视图，R ≤ 数十条，常数小"],
        ["订票（含座位分配）", "O(S)", "全舱位空闲扫描为最坏情形；占用位图判位 O(1)"],
        ["候补登记 / 出队", "O(1)", "链队列头尾指针；remove_at 按位 O(k)"],
        ["退票 + 撮合", "O(n + k·S)", "订单查重 O(n)；每撮合一单做一次座位分配 O(S)"],
        ["登机调整计划", "O(n + ΣS)", "位图副本上分配；候选序列近有序，插入排序实际近 O(n)"],
        ["状态落盘 / 加载", "O(总状态量)", "全量 JSON 序列化；写临时文件后 rename 保证原子性"],
      ],
      [26, 20, 54],
    ),
    body("空间方面，系统状态为 O(R + 班次实例数 + Σ订单 + Σ候补)；座位占用位图为 3 × 40 × 6 字节的常数规模；峰值额外开销出现在登机调整时的位图副本与推荐扫描，均为千字节级。"),
    h2("4.3　改进设想"),
    li("候补撮合与合并两棵平衡结构：若将候补队列升级为按票数的有序结构，可用树状合并把撮合降至 O(log k)；当前票数上限为 6，线性扫描已足够。", "1."),
    li("锁粒度：当前一把全局互斥锁保证正确性；若演示并发，可按航班实例分桶加锁，读多写少的查询接口可改为读写锁。", "2."),
    li("落盘增量化：全量 JSON 在状态量大时写放大明显，可改为操作日志（append-only）加周期快照。", "3."),
    li("前端可增加局部刷新与座位图增量渲染；当前全量重拉在演示规模下已足够流畅。", "4."),
  ];
}

// ─────────────────────────── 第五章 用户使用说明 ───────────────────────────
function ch5() {
  return [
    h1("第五章　用户使用说明"),
    h2("5.1　环境要求与构建"),
    body("程序支持 Windows 与 macOS。以提交环境 Windows 为例：安装 Visual Studio 2022 并勾选\u201c使用 C++ 的桌面开发\u201d，在开发者命令行执行以下两步构建，得到 build\\Release\\airline.exe（主程序）与 airline_tests.exe（测试程序）。第三方依赖仅两个头文件，已随仓库附带，构建过程不需要联网。"),
    ...codeBlock([
      "git clone https://github.com/yulabu/airline.git",
      "cd airline",
      "cmake -S . -B build -G \"Visual Studio 17 2022\" -A x64",
      "cmake --build build --config Release",
    ]),
    h2("5.2　个人信息配置"),
    body("将 student.env.example 复制为 student.env，按 KEY=VALUE 格式填写 CLASS（班级）、STUDENT_ID（学号）、NAME（姓名），重启程序后启动横幅与网页页头即显示设计者信息。该文件不被纳入版本管理；Windows 双击运行时须与 exe 同目录。"),
    h2("5.3　启动与访问"),
    body("将 airline.exe 置于项目根目录（与 web 文件夹同级）后双击，或命令行运行。控制台打印横幅并保持运行，浏览器访问 http://localhost:8080 即可使用。首次运行会弹出防火墙提示，选择允许访问。端口被占用时可用 --port 换端口；删除 state.json 后重启即恢复干净演示数据。"),
    h2("5.4　乘客端操作"),
    li("查询航线：在\u201c查询航线\u201d区填写起点或终点（至少一项），可再按星期过滤，点击查询，结果以航线卡片展示分舱位余票与票价。", "①"),
    li("订票：点击卡片中的\u201c订票\u201d，在弹窗中选择舱位与张数，可在座位图中点选具体座位（也可留空由系统分配），填写姓名与身份证后提交；成功返回订单号与座位号。", "②"),
    li("登记候补：余票不足时订票失败界面会给出剩余可售量与候补人数，点击\u201c登记候补\u201d完成排队，界面显示当前位次。", "③"),
    li("退票：在\u201c退票\u201d区填写航班号、日期、身份证与张数（0 表示整单全退）后提交，界面显示退票张数及本次被撮合成功的候补者名单。", "④"),
    li("查看推荐：订票失败界面可点击\u201c同目的地其他航线\u201d，系统列出未来三天内有票的替代航班。", "⑤"),
    h2("5.5　管理员端操作"),
    li("新建航线：切换到\u201c管理员\u201d页签，在\u201c新建航线\u201d区填写航班号、起终点、机型、飞行日与三舱位配置后创建；航班号重复或配置不合法会被拒绝并提示原因。", "①"),
    li("修改航线：选择航班号后点击\u201c载入\u201d，修改后保存。注意：各舱位新定额不得低于该舱位已售最大值；有已售票时不能更换机型。", "②"),
    li("查看订票记录：按航班号、日期、身份证任意组合过滤，表格展示订单号、舱位、张数、座位、订票时间与状态。", "③"),
    li("模拟登机：填写航班号与日期后执行，界面输出调整报告——各舱位登机数与物理座位数对照、升舱名单（含新座位号）、强制退票名单（按最后订票优先）。登机后该航班锁定。", "④"),
    h2("5.6　常见问题"),
    li("双击后窗口一闪而过：多为 web 文件夹未与 exe 同目录，改在项目根目录用命令行运行可看到具体报错。", "1."),
    li("端口占用：使用 --port 9090 等更换端口。", "2."),
    li("控制台中文乱码：构建已强制 UTF-8；旧版命令行可先执行 chcp 65001。", "3."),
  ];
}

// ─────────────────────────── 第六章 测试数据及测试结果 ───────────────────────────
function ch6() {
  return [
    h1("第六章　测试数据及测试结果"),
    h2("6.1　测试方法与覆盖"),
    body("测试分三层组织。第一层为核心层单元测试：对顺序表、链队列、哈希索引、日期运算、座位分配、候补撮合、登机调整计划及 service 层全链路共编写 1432 项断言，全部通过；该测试只链接不含第三方的核心静态库，可在任何环境独立运行。第二层为接口端到端测试：服务启动后以 HTTP 请求逐条驱动全部接口，覆盖正常流与错误流。第三层为浏览器验收：对页面渲染、座位图交互与页头信息进行人工视觉核对。"),
    h2("6.2　测试用例及结果"),
    body("下表列出具有代表性的接口级测试用例。实际结果均为程序真实运行输出，全部符合预期。"),
    caption("表 6-1　代表性测试用例"),
    table3(
      ["编号", "模块", "输入 / 前置操作", "预期结果", "实际结果"],
      [
        ["T01", "查询航线", "起点=广州", "返回该起点全部航线及分舱余票", "返回 3 条（含 CZ3101），一致"],
        ["T02", "查询航线", "终点=北京", "命中所有到达北京的航线", "返回 CZ3101、MU5203 等，一致"],
        ["T03", "订票", "CZ3101 经济舱 2 张，自动分配", "出票并返回连续座位号", "返回 7A、7B，一致"],
        ["T04", "订票", "CZ3101 经济舱 1 张，指定 9C", "出票座位 9C", "一致"],
        ["T05", "订票", "指定已被占用座位 9C", "拒绝：SEAT_TAKEN", "一致"],
        ["T06", "订票", "同一身份证再次订同航班", "拒绝：DUPLICATE_ORDER", "一致"],
        ["T07", "超卖", "商务舱订满定额+超卖上限后再订", "拒绝：NO_SEAT，附剩余可售 0 与候补人数", "一致"],
        ["T08", "候补登记", "NO_SEAT 后登记（甲 2 张、乙 5 张）", "入队并返回位次 1、2", "一致"],
        ["T09", "候补守卫", "有余票时直接登记候补", "拒绝：NOT_ELIGIBLE", "一致"],
        ["T10", "候补守卫", "已持订单者登记候补", "拒绝：DUPLICATE_ORDER", "一致"],
        ["T11", "退票撮合", "退 3 张；队首要 2 张、次位要 5 张", "队首获票 2 张带座位号，次位跳过", "一致，乙未被撮合"],
        ["T12", "退票撮合", "再退 5 张", "乙（5 张）获票", "一致"],
        ["T13", "部分退票", "6 张订单退 1 张", "订单保留 5 张，状态为部分退票", "一致"],
        ["T14", "一致性", "候补者后经正常途径持票，再触发撮合", "其残留登记被清理，不产生第二张订单", "一致"],
        ["T15", "修改航线", "已售 6 张，定额改 5", "拒绝：QUOTA_BELOW_SOLD", "一致"],
        ["T16", "修改航线", "有已售票时更换机型", "拒绝：NOT_ELIGIBLE", "一致"],
        ["T17", "登机调整", "经济舱 156 座售 157 张，商务舱全空", "1 人升商务舱并分配新座位，无人被强制退票", "一致"],
        ["T18", "登机锁定", "登机后再订票", "拒绝：FLIGHT_LOCKED（HTTP 423）", "一致"],
        ["T19", "航线推荐", "订票失败后请求同目的地推荐", "列出未来三天内有票的其他航班，不含过去日期", "返回 MU5101、MU5203，一致"],
        ["T20", "持久化", "重启程序", "从 state.json 恢复全部航线、订单、候补与序号", "重启后记录完整恢复，一致"],
        ["T21", "数据结构", "顺序表/链队列/哈希索引 1432 项断言", "全部通过", "1432 项 0 失败"],
      ],
      [8, 12, 28, 28, 24],
    ),
    h2("6.3　测试结论"),
    body("三层测试全部通过：核心数据结构在 1432 项断言下无失败；接口端到端用例覆盖全部基本要求与三项扩展的正常流与错误流；浏览器验收确认页面渲染、座位图交互与设计者信息显示正确。系统在演示规模（18 条航线、百余订单）下响应即时，功能完整性与规则一致性达到设计预期。"),
  ];
}

// ─────────────────────────── 附录 ───────────────────────────
function appendix() {
  return [
    h1("附　　录"),
    body("按指导书要求，带注释的完整源程序不贴于报告正文，随源码仓库与光盘提交。以下为源程序文件清单（共 46 个文件，版本管理于 GitHub 仓库 yulabu/airline，提交历史完整可追溯）。"),
    caption("表 F-1　源程序文件清单"),
    table3(
      ["目录", "文件", "职责"],
      [
        ["src/base/", "status.h/.cpp, date.h/.cpp, result.h", "错误码、公历日期运算、Result 包装"],
        ["src/ds/", "seq_list.h, link_queue.h, hash_index.h", "自研顺序表、链队列、哈希索引"],
        ["src/domain/", "types, route, seat_layout, instance, order, waitlist", "实体定义与不变量"],
        ["src/domain/policies/", "avail, seat_alloc, waitlist_match, checkin_plan, config_check", "纯策略函数：余票/座位/撮合/登机计划/校验"],
        ["src/repo/", "repository.h/.cpp", "状态聚合、两级哈希索引、惰性实例、序号生成"],
        ["src/service/", "airline_service, query, booking, admin, seed, dto", "六个用例的实现与内置种子数据"],
        ["src/store/", "store.h/.cpp", "state.json 原子读写"],
        ["src/api/", "server.h/.cpp", "HTTP 路由、参数校验、JSON 编解码"],
        ["src/app/", "main.cpp, student_info.h/.cpp", "启动参数、状态恢复、个人信息配置"],
        ["web/", "index.html, css/style.css, js/app.js", "零构建静态前端"],
        ["tests/", "test_main.cpp, test_service.cpp", "核心层与用例链路测试（1432 项断言）"],
        ["third_party/", "httplib.h, nlohmann/json.hpp", "header-only 第三方库（仅外层使用）"],
      ],
      [20, 38, 42],
    ),
    body("构建产物与运行时状态文件（build/、state.json、student.env）不入版本库；其中 student.env 携带个人信息，仅随光盘交付。"),
  ];
}

// ─────────────────────────── 组装文档 ───────────────────────────
const pgSize = { width: 11906, height: 16838 };
const pgMargin = { top: 1440, bottom: 1440, left: 1701, right: 1417, header: 850, footer: 992 };

function bodyHeader() {
  return new Header({ children: [
    new Paragraph({
      alignment: AlignmentType.CENTER,
      border: { bottom: { style: BorderStyle.SINGLE, size: 4, color: BLACK } },
      children: [new TextRun({ text: "《数据结构》课程设计报告", size: 18, color: "333333", font: FONT_BODY })],
    }),
  ] });
}
function pageFooter() {
  return new Footer({ children: [
    new Paragraph({
      alignment: AlignmentType.CENTER,
      children: [
        new TextRun({ text: "- ", size: 21, font: FONT_BODY }),
        new TextRun({ children: [PageNumber.CURRENT], size: 21, font: { ascii: "Times New Roman" } }),
        new TextRun({ text: " -", size: 21, font: FONT_BODY }),
      ],
    }),
  ] });
}

const doc = new Document({
  creator: safeText(STU.NAME),
  title: "航空客运订票系统 - 数据结构课程设计报告",
  styles: {
    default: {
      document: {
        run: { font: FONT_BODY, size: 24, color: BLACK },
        paragraph: { spacing: { line: 360 } },
      },
      heading1: {
        run: { font: FONT_HEAD, size: 32, bold: true, color: BLACK },
        paragraph: { alignment: AlignmentType.CENTER, spacing: { before: 480, after: 360, line: 360 }, outlineLevel: 0 },
      },
      heading2: {
        run: { font: FONT_HEAD, size: 30, bold: true, color: BLACK },
        paragraph: { spacing: { before: 360, after: 240, line: 360 }, outlineLevel: 1 },
      },
      heading3: {
        run: { font: FONT_HEAD, size: 28, bold: true, color: BLACK },
        paragraph: { spacing: { before: 240, after: 120, line: 360 }, outlineLevel: 2 },
      },
    },
  },
  sections: [
    { // 第 1 节：封面（无页码页眉）
      properties: { page: { size: pgSize, margin: { top: 0, bottom: 0, left: 0, right: 0 } } },
      children: buildCover(),
    },
    { // 第 2 节：目录（无页码）
      properties: { type: SectionType.NEXT_PAGE, page: { size: pgSize, margin: pgMargin } },
      children: buildToc(),
    },
    { // 第 3 节：正文（阿拉伯页码从 1 起）
      properties: {
        type: SectionType.NEXT_PAGE,
        page: { size: pgSize, margin: pgMargin, pageNumbers: { start: 1, formatType: NumberFormat.DECIMAL } },
      },
      headers: { default: bodyHeader() },
      footers: { default: pageFooter() },
      children: [
        ...ch1(), ...ch2(), ...ch3(), ...ch4(), ...ch5(), ...ch6(), ...appendix(),
      ],
    },
  ],
});

const SUFFIX = MAC_FONTS ? "_预览版" : `_${safeText(STU.STUDENT_ID, "学号")}`;
const OUT = path.join(__dirname, "..", `课程设计报告_航空客运订票系统${SUFFIX}.docx`);
Packer.toBuffer(doc).then(buf => {
  fs.writeFileSync(OUT, buf);
  console.log("生成完成: " + OUT);
});
