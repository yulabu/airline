/* 航空客运订票系统 - 前端逻辑
 * 边界约定：本文件不做任何业务计算（余票判断、能否候补等全部以服务端返回为准）。
 * 只负责：收集输入、格式校验、渲染结果。每次操作后重新拉取数据。 */

let META = { aircraft: [], cabins: [], max_seats_per_order: 6 };
let CURRENT_ROUTE = null;   // 正在订票的航线视图
let CURRENT_SEAT = '';      // 已选座位
let LAST_FAIL = null;       // 订票失败上下文（供候补）

// ─────────────── 基础设施 ───────────────

async function api(path, opts = {}) {
  const res = await fetch(path, {
    headers: { 'Content-Type': 'application/json' },
    ...opts,
    body: opts.body ? JSON.stringify(opts.body) : undefined,
  });
  const data = await res.json().catch(() => ({ ok: false, code: 'INTERNAL', msg: '响应解析失败' }));
  return { http: res.status, ...data };
}

function toast(msg, isErr = false) {
  const el = document.getElementById('toast');
  el.textContent = msg;
  el.style.background = isErr ? '#7f1d1d' : '#1e293b';
  el.classList.add('show');
  clearTimeout(el._t);
  el._t = setTimeout(() => el.classList.remove('show'), 2600);
}

const CABIN_NAMES = ['头等舱', '商务舱', '经济舱'];

function esc(s) {
  return String(s ?? '').replace(/[&<>"]/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));
}

// ─────────────── 初始化 ───────────────

async function init() {
  const meta = await api('/api/meta');
  if (meta.ok) {
    META = meta.data;
    // 页头个人信息来自后端（student.env 配置，不入仓库）
    const s = META.student;
    if (s) {
      document.getElementById('student-info').innerHTML =
        `班级：${esc(s.class)} &nbsp; 学号：${esc(s.student_id)} &nbsp; 姓名：${esc(s.name)}`;
    }
  }

  // 机型下拉框
  for (const sel of [document.getElementById('na-aircraft'), document.getElementById('ud-aircraft')]) {
    sel.innerHTML = META.aircraft.map(a => `<option>${esc(a)}</option>`).join('');
  }

  // 舱位价格输入行（新建/修改共用生成逻辑）
  renderCabinInputs('na-cabins');
  renderCabinInputs('ud-cabins');

  // 订票对话框的舱位下拉框
  document.getElementById('bd-cabin').innerHTML =
    META.cabins.map(c => `<option value="${c.cabin}">${esc(c.name)}</option>`).join('');

  document.getElementById('tab-passenger').onclick = () => switchTab('passenger');
  document.getElementById('tab-admin').onclick = () => switchTab('admin');

  document.getElementById('bd-date').addEventListener('change', loadSeatMap);
  document.getElementById('bd-cabin').addEventListener('change', loadSeatMap);

  switchTab('passenger');
}

function switchTab(name) {
  const p = name === 'passenger';
  document.getElementById('view-passenger').hidden = !p;
  document.getElementById('view-admin').hidden = p;
  document.getElementById('tab-passenger').classList.toggle('active', p);
  document.getElementById('tab-admin').classList.toggle('active', !p);
  if (!p) refreshAdminFlights();
}

function renderCabinInputs(containerId) {
  const el = document.getElementById(containerId);
  el.innerHTML = META.cabins.map(c => `
    <fieldset style="border:1px solid var(--line);border-radius:8px;padding:8px 12px;">
      <legend>${esc(c.name)}</legend>
      <label>定额 <input class="${containerId}-quota" data-cabin="${c.cabin}" type="number" min="0" style="width:80px"></label>
      <label>超卖上限 <input class="${containerId}-over" data-cabin="${c.cabin}" type="number" min="0" value="0" style="width:70px"></label>
      <label>票价 <input class="${containerId}-price" data-cabin="${c.cabin}" type="number" min="1" style="width:90px"></label>
    </fieldset>`).join('');
}

function readCabins(containerId) {
  return META.cabins.map(c => {
    const q = document.querySelector(`.${containerId}-quota[data-cabin="${c.cabin}"]`);
    const o = document.querySelector(`.${containerId}-over[data-cabin="${c.cabin}"]`);
    const p = document.querySelector(`.${containerId}-price[data-cabin="${c.cabin}"]`);
    return { quota: +q.value || 0, overbook_max: +o.value || 0, price: +p.value || 0 };
  });
}

function weekdaysMask(spanId) {
  let mask = 0;
  document.querySelectorAll(`#${spanId} input`).forEach(cb => {
    const wd = +cb.value === 0 ? 7 : +cb.value;   // 页面上“日”的 value 是 0
    if (cb.checked) mask |= (1 << (wd - 1));
  });
  return mask;
}

function setWeekdays(spanId, mask) {
  document.querySelectorAll(`#${spanId} input`).forEach(cb => {
    const wd = +cb.value === 0 ? 7 : +cb.value;
    cb.checked = (mask & (1 << (wd - 1))) !== 0;
  });
}

// ─────────────── 乘客：查询航线 ───────────────

async function doSearch() {
  const from = document.getElementById('q-from').value.trim();
  const to = document.getElementById('q-to').value.trim();
  const weekday = +document.getElementById('q-weekday').value;
  if (!from && !to) return toast('起点和终点至少填写一个', true);

  const params = new URLSearchParams();
  if (from) params.set('from', from);
  if (to) params.set('to', to);
  if (weekday) params.set('weekday', weekday);

  const r = await api('/api/search?' + params);
  const panel = document.getElementById('result-panel');
  const box = document.getElementById('route-list');
  panel.hidden = false;

  if (!r.ok) { box.innerHTML = `<p class="result-err">${esc(r.msg)}</p>`; return; }
  if (!r.data.length) { box.innerHTML = '<p class="result-info">没有符合条件的航线</p>'; return; }
  box.innerHTML = r.data.map(routeCard).join('');
}

function routeCard(v) {
  const rows = v.cabins.map(c => `
    <tr>
      <td>${esc(c.name)}</td>
      <td>¥${c.price}</td>
      <td>${c.quota}</td>
      <td class="${c.avail === 0 ? 'avail-0' : (c.avail <= 5 ? 'avail-low' : '')}">${c.avail}</td>
      <td><button class="primary" onclick='openBook(${JSON.stringify(v)}, ${c.cabin})'>订票</button></td>
    </tr>`).join('');
  return `
    <div class="route-card">
      <div class="route-head">
        <span class="route-no">${esc(v.flight_no)}</span>
        <span class="route-path">${esc(v.from)} ✈ ${esc(v.to)}</span>
        <span class="route-meta">机型 ${esc(v.aircraft)} ｜ ${esc(v.weekday_text)} ｜ 最近班次 ${esc(v.next_date)}</span>
        <span class="spacer"></span>
        <button onclick="showInstances('${esc(v.flight_no)}')">未来班次</button>
      </div>
      <table class="cabin">
        <tr><th>舱位</th><th>票价</th><th>定额</th><th>余票</th><th></th></tr>
        ${rows}
      </table>
      <div class="future-box" id="future-${esc(v.flight_no)}"></div>
    </div>`;
}

async function showInstances(no) {
  const box = document.getElementById('future-' + no);
  if (box.innerHTML) { box.innerHTML = ''; return; }
  const r = await api(`/api/flights/${no}/instances?n=7`);
  if (!r.ok) { box.innerHTML = `<p class="result-err">${esc(r.msg)}</p>`; return; }
  box.innerHTML = `<table class="cabin"><tr><th>日期</th>` +
    META.cabins.map(c => `<th>${esc(c.name)}</th>`).join('') + `</tr>` +
    r.data.map(v => `<tr><td>${esc(v.next_date)}</td>` +
      v.cabins.map(c => `<td class="${c.avail === 0 ? 'avail-0' : ''}">${c.avail}</td>`).join('') +
      `</tr>`).join('') + `</table>`;
}

// ─────────────── 乘客：订票 ───────────────

function openBook(route, cabin) {
  CURRENT_ROUTE = route;
  CURRENT_SEAT = '';
  LAST_FAIL = null;
  const dlg = document.getElementById('book-dialog');
  document.getElementById('bd-title').textContent =
    `订票 — ${route.flight_no} ${route.from} ✈ ${route.to}`;
  document.getElementById('bd-date').value = route.next_date;
  document.getElementById('bd-cabin').value = cabin;
  document.getElementById('bd-qty').value = 1;
  document.getElementById('bd-seat').value = '';
  document.getElementById('bd-name').value = '';
  document.getElementById('bd-idcard').value = '';
  document.getElementById('bd-result').innerHTML = '';
  document.getElementById('bd-waitlist-btn').hidden = true;
  dlg.showModal();
  loadSeatMap();
}

async function loadSeatMap() {
  const no = CURRENT_ROUTE.flight_no;
  const date = document.getElementById('bd-date').value;
  const cabin = +document.getElementById('bd-cabin').value;
  const box = document.getElementById('bd-seatmap');

  const r = await api(`/api/seats?flight_no=${no}&date=${encodeURIComponent(date)}&cabin=${cabin}`);
  if (!r.ok) { box.innerHTML = ''; return; }

  // 按(排,字母)渲染；字母表中 D-E 之间插入走道。
  // 列数 = 行号列 + 座位数 + 走道列
  const letters = r.data.letters;
  const mid = Math.ceil(letters.length / 2);
  const colCount = letters.length + 2;
  let html = `<div class="seatmap-legend">
      <span class="swatch" style="background:#ecfdf5;border:1px solid #6ee7b7"></span>可选
      <span class="swatch" style="background:#e2e8f0"></span>已售
      <span class="swatch" style="background:#1a56db"></span>已选
    </div><div class="seat-grid" style="grid-template-columns: repeat(${colCount}, auto)">`;

  let row = 0;
  for (const cell of r.data.cells) {
    if (cell.row !== row) {
      row = cell.row;
      html += `<div class="seat-row-label">${row}</div>`;
    }
    const idx = letters.indexOf(cell.letter);
    if (idx === mid) html += `<div class="seat aisle"></div>`;
    const cls = cell.occupied ? 'taken' : 'free';
    html += `<div class="seat ${cls}" data-code="${cell.code}" ${cell.occupied ? '' : `onclick="pickSeat('${cell.code}')"`}>${cell.code}</div>`;
  }
  box.innerHTML = html + '</div>';
}

function pickSeat(code) {
  document.querySelectorAll('#bd-seatmap .selected').forEach(e => e.classList.remove('selected'));
  const el = document.querySelector(`#bd-seatmap [data-code="${code}"]`);
  if (el) el.classList.add('selected');
  CURRENT_SEAT = code;
  document.getElementById('bd-seat').value = code;
}

async function submitBook() {
  const body = {
    flight_no: CURRENT_ROUTE.flight_no,
    date: document.getElementById('bd-date').value,
    cabin: +document.getElementById('bd-cabin').value,
    qty: +document.getElementById('bd-qty').value,
    seat: document.getElementById('bd-seat').value.trim(),
    name: document.getElementById('bd-name').value.trim(),
    id_card: document.getElementById('bd-idcard').value.trim(),
  };
  const box = document.getElementById('bd-result');
  const r = await api('/api/orders', { method: 'POST', body });

  if (r.ok) {
    const d = r.data;
    const seatTxt = d.seat_pending
      ? '<span class="tag warn">超卖待定：暂无实体座位，登机时按规则调整</span>'
      : d.seats.map(s => `<span class="tag">座位 ${esc(s)}</span>`).join('');
    box.innerHTML = `<p class="result-ok">✔ 订票成功 订单号 ${esc(d.order_id)}　${seatTxt}</p>`;
    document.getElementById('bd-waitlist-btn').hidden = true;
    loadSeatMap();
    return;
  }

  if (r.code === 'NO_SEAT') {
    LAST_FAIL = r.data;
    box.innerHTML = `<p class="result-err">✘ ${esc(r.msg)}（剩余可售 ${r.data.remaining} 张，候补 ${r.data.waitlist_size} 人）</p>
      <p class="result-info">可改少量张数重试、登记候补，或查看 <a href="#" onclick="showRecommend();return false;">同目的地其他航线</a></p>`;
    document.getElementById('bd-waitlist-btn').hidden = false;
  } else if (r.code === 'SEAT_TAKEN') {
    box.innerHTML = `<p class="result-err">✘ ${esc(r.msg)}，请在座位图中另选</p>`;
    loadSeatMap();
  } else {
    box.innerHTML = `<p class="result-err">✘ ${esc(r.msg)}</p>`;
  }
}

async function submitWaitlist() {
  if (!LAST_FAIL) return;
  const body = {
    flight_no: CURRENT_ROUTE.flight_no,
    date: document.getElementById('bd-date').value,
    cabin: +document.getElementById('bd-cabin').value,
    qty: +document.getElementById('bd-qty').value,
    name: document.getElementById('bd-name').value.trim(),
    id_card: document.getElementById('bd-idcard').value.trim(),
  };
  const box = document.getElementById('bd-result');
  const r = await api('/api/waitlist', { method: 'POST', body });
  if (r.ok) {
    box.innerHTML = `<p class="result-ok">✔ 已登记候补，当前排在第 ${r.data.position} 位。退票后按"首个能满足者"原则依次撮合。</p>`;
    document.getElementById('bd-waitlist-btn').hidden = true;
  } else {
    box.innerHTML += `<p class="result-err">✘ 候补登记失败：${esc(r.msg)}</p>`;
  }
}

async function showRecommend() {
  const r = await api(`/api/recommend?to=${encodeURIComponent(CURRENT_ROUTE.to)}&date=${encodeURIComponent(document.getElementById('bd-date').value)}&exclude=${CURRENT_ROUTE.flight_no}`);
  const box = document.getElementById('bd-result');
  if (!r.ok || !r.data.length) {
    box.innerHTML += '<p class="result-info">暂无同目的地的其他推荐航线</p>';
    return;
  }
  box.innerHTML += '<p class="result-info">同目的地推荐：</p>' + r.data.map(v =>
    `<p>✈ ${esc(v.flight_no)}　${esc(v.from)} → ${esc(v.to)}　${esc(v.weekday_text)}　最近 ${esc(v.next_date)}
      ${v.cabins.filter(c => c.avail > 0).map(c => `<span class="tag">${esc(c.name)} 余 ${c.avail}</span>`).join('')}</p>`).join('');
}

// ─────────────── 乘客：退票 ───────────────

async function doRefund() {
  const body = {
    flight_no: document.getElementById('rf-flight').value.trim().toUpperCase(),
    date: document.getElementById('rf-date').value,
    id_card: document.getElementById('rf-idcard').value.trim(),
    qty: +document.getElementById('rf-qty').value || 0,
  };
  const box = document.getElementById('refund-result');
  const r = await api('/api/refund', { method: 'POST', body });
  if (!r.ok) { box.innerHTML = `<p class="result-err">✘ ${esc(r.msg)}</p>`; return; }

  let html = `<p class="result-ok">✔ 退票成功，共退 ${r.data.refunded} 张（订单 ${esc(r.data.order_id)}）</p>`;
  if (r.data.promoted.length) {
    html += '<p class="result-info">已按候补队列撮合：</p>';
    for (const p of r.data.promoted) {
      const seatTxt = p.seat_pending ? '<span class="tag warn">超卖待定</span>'
        : p.seats.map(s => `<span class="tag">座位 ${esc(s)}</span>`).join('');
      html += `<p>✈ ${esc(p.name)}（${esc(p.id_card)}）获票 ${p.qty} 张 ${esc(CABIN_NAMES[p.cabin])}　${seatTxt}</p>`;
    }
  } else {
    html += '<p class="result-info">该舱位暂无满足条件的候补客户</p>';
  }
  box.innerHTML = html;
}

// ─────────────── 管理员 ───────────────

async function refreshAdminFlights() {
  const r = await api('/api/search?from=&to=').catch(() => null);
  const sel = document.getElementById('ud-flight');
  if (r && r.ok) {
    sel.innerHTML = r.data.map(v => `<option>${esc(v.flight_no)}</option>`).join('');
  }
}

async function createRoute() {
  const body = {
    flight_no: document.getElementById('na-no').value.trim().toUpperCase(),
    from: document.getElementById('na-from').value.trim(),
    to: document.getElementById('na-to').value.trim(),
    aircraft: document.getElementById('na-aircraft').value,
    weekdays: weekdaysMask('na-weekdays'),
    cabins: readCabins('na-cabins'),
  };
  const r = await api('/api/admin/routes', { method: 'POST', body });
  if (!r.ok) return toast(`${r.msg}`, true);
  toast(`✔ 航线 ${r.data.flight_no} 已创建`);
  refreshAdminFlights();
}

async function loadRouteForEdit() {
  const no = document.getElementById('ud-flight').value;
  const r = await api(`/api/search?from=&to=`);
  if (!r.ok) return;
  const v = r.data.find(x => x.flight_no === no);
  if (!v) return toast('未找到该航线', true);

  document.getElementById('ud-from').value = v.from;
  document.getElementById('ud-to').value = v.to;
  document.getElementById('ud-aircraft').value = v.aircraft;
  setWeekdays('ud-weekdays', v.weekdays);
  v.cabins.forEach(c => {
    document.querySelector(`.ud-cabins-quota[data-cabin="${c.cabin}"]`).value = c.quota;
    document.querySelector(`.ud-cabins-over[data-cabin="${c.cabin}"]`).value = c.overbook_max;
    document.querySelector(`.ud-cabins-price[data-cabin="${c.cabin}"]`).value = c.price;
  });
}

async function updateRoute() {
  const no = document.getElementById('ud-flight').value;
  const body = {
    from: document.getElementById('ud-from').value.trim(),
    to: document.getElementById('ud-to').value.trim(),
    aircraft: document.getElementById('ud-aircraft').value,
    weekdays: weekdaysMask('ud-weekdays'),
    cabins: readCabins('ud-cabins'),
  };
  const r = await api(`/api/admin/routes/${no}`, { method: 'PUT', body });
  if (!r.ok) return toast(`✘ ${r.msg}`, true);
  toast(`✔ 航线 ${no} 已更新`);
}

async function loadRecords() {
  const params = new URLSearchParams();
  const no = document.getElementById('rec-flight').value.trim().toUpperCase();
  const date = document.getElementById('rec-date').value;
  const idcard = document.getElementById('rec-idcard').value.trim();
  if (no) params.set('flight_no', no);
  if (date) params.set('date', date);
  if (idcard) params.set('id_card', idcard);

  const r = await api('/api/admin/records?' + params);
  const box = document.getElementById('records-box');
  if (!r.ok) { box.innerHTML = `<p class="result-err">${esc(r.msg)}</p>`; return; }
  if (!r.data.length) { box.innerHTML = '<p class="result-info">暂无记录</p>'; return; }

  box.innerHTML = `<table class="records">
    <tr><th>航班</th><th>日期</th><th>订单号</th><th>姓名</th><th>身份证</th>
    <th>舱位</th><th>张数</th><th>座位</th><th>票价</th><th>订票时间</th><th>状态</th></tr>` +
    r.data.map(row => `<tr>
      <td>${esc(row.flight_no)}</td><td>${esc(row.date)}</td><td>${esc(row.order_id)}</td>
      <td>${esc(row.name)}</td><td>${esc(row.id_card)}</td>
      <td>${esc(row.cabin_name)}</td><td>${row.qty}</td>
      <td>${row.seat_pending ? '<span class="tag warn">待定</span>' : row.seats.map(esc).join(' ')}</td>
      <td>¥${row.price}</td><td>${esc(row.book_time)}</td>
      <td>${esc(row.status_name)}（序号${row.book_seq}）</td></tr>`).join('') +
    '</table>';
}

async function doCheckin() {
  const body = {
    flight_no: document.getElementById('ck-flight').value.trim().toUpperCase(),
    date: document.getElementById('ck-date').value,
  };
  const box = document.getElementById('checkin-result');
  const r = await api('/api/admin/checkin', { method: 'POST', body });
  if (!r.ok) { box.innerHTML = `<p class="result-err">✘ ${esc(r.msg)}</p>`; return; }

  const d = r.data;
  let html = `<p class="result-ok">✔ 登机完成：${esc(d.flight_no)} @ ${esc(d.date)}，航班已锁定</p>`;

  html += '<p class="result-info">各舱位登机情况：</p><table class="records"><tr><th>舱位</th><th>物理座位</th><th>登机人数</th></tr>';
  for (const b of d.boarded) {
    html += `<tr><td>${esc(b.name)}</td><td>${b.physical}</td><td>${b.boarded}</td></tr>`;
  }
  html += '</table>';

  if (d.upgrades.length) {
    html += `<p class="result-info">升舱调整 ${d.upgrades.length} 人：</p>`;
    for (const u of d.upgrades) {
      html += `<p>⬆ ${esc(u.name)}（${esc(u.order_id)}）${esc(u.from_name)} → ${esc(u.to_name)}，${u.qty} 张，新座位：${u.seats.map(esc).join(' ')}</p>`;
    }
  }
  if (d.forfeits.length) {
    html += `<p class="result-err">强制退票 ${d.forfeits.length} 人（最后订票者优先）：</p>`;
    for (const f of d.forfeits) {
      html += `<p>✘ ${esc(f.name)}（${esc(f.order_id)}）${esc(f.cabin_name)} ${f.qty} 张，订票序号 ${f.book_seq}</p>`;
    }
  }
  if (!d.upgrades.length && !d.forfeits.length) {
    html += '<p class="result-info">无超卖，所有客户按原座位登机</p>';
  }
  box.innerHTML = html;
}

init();
