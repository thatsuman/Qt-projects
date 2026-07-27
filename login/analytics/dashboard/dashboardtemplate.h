#ifndef ANALYTICS_DASHBOARD_DASHBOARDTEMPLATE_H
#define ANALYTICS_DASHBOARD_DASHBOARDTEMPLATE_H

#include <QString>

namespace Analytics {

// Returns the complete embedded CSS for the dashboard HTML.
inline QString dashboardCss()
{
    return QStringLiteral(R"CSS(
:root {
    --bg: #0f1117;
    --surface: #1a1d27;
    --surface2: #232638;
    --accent: #6c8ef5;
    --accent2: #4ecdc4;
    --text: #e8eaf6;
    --text-muted: #8892b0;
    --good: #4caf50;
    --warn: #ff9800;
    --bad: #f44336;
    --border: #2d3153;
    --font: 'Segoe UI', system-ui, sans-serif;
}
* { box-sizing: border-box; margin: 0; padding: 0; }
body { background: var(--bg); color: var(--text); font-family: var(--font); font-size: 14px; min-height: 100vh; }
header { background: var(--surface); border-bottom: 1px solid var(--border); padding: 18px 32px; display: flex; align-items: center; gap: 16px; }
header h1 { font-size: 20px; font-weight: 700; color: var(--accent); letter-spacing: .5px; }
header .meta { font-size: 12px; color: var(--text-muted); margin-left: auto; }
nav { background: var(--surface); border-bottom: 1px solid var(--border); display: flex; padding: 0 32px; }
nav button { background: none; border: none; color: var(--text-muted); cursor: pointer; font-size: 14px; font-family: var(--font); padding: 14px 20px; border-bottom: 2px solid transparent; transition: all .2s; }
nav button:hover { color: var(--text); }
nav button.active { color: var(--accent); border-bottom-color: var(--accent); }
.tab-content { display: none; padding: 28px 32px; }
.tab-content.active { display: block; }
.cards { display: grid; grid-template-columns: repeat(auto-fill, minmax(200px, 1fr)); gap: 16px; margin-bottom: 28px; }
.card { background: var(--surface); border: 1px solid var(--border); border-radius: 10px; padding: 18px; }
.card .label { font-size: 11px; text-transform: uppercase; letter-spacing: .8px; color: var(--text-muted); margin-bottom: 8px; }
.card .value { font-size: 26px; font-weight: 700; color: var(--accent); }
.card .sub { font-size: 12px; color: var(--text-muted); margin-top: 4px; }
h2 { font-size: 16px; font-weight: 600; color: var(--text); margin-bottom: 16px; }
h3 { font-size: 13px; font-weight: 600; color: var(--text-muted); margin-bottom: 10px; text-transform: uppercase; letter-spacing: .5px; }
.table-wrap { background: var(--surface); border: 1px solid var(--border); border-radius: 10px; overflow: hidden; margin-bottom: 24px; }
.table-toolbar { display: flex; align-items: center; gap: 12px; padding: 12px 16px; border-bottom: 1px solid var(--border); }
.table-toolbar input { background: var(--surface2); border: 1px solid var(--border); color: var(--text); border-radius: 6px; padding: 6px 10px; font-size: 13px; font-family: var(--font); width: 220px; }
.table-toolbar input::placeholder { color: var(--text-muted); }
table { width: 100%; border-collapse: collapse; }
th { background: var(--surface2); padding: 10px 14px; text-align: left; font-size: 11px; text-transform: uppercase; letter-spacing: .6px; color: var(--text-muted); cursor: pointer; user-select: none; white-space: nowrap; }
th:hover { color: var(--accent); }
th .sort-icon { margin-left: 4px; opacity: .4; }
th.sorted .sort-icon { opacity: 1; color: var(--accent); }
td { padding: 10px 14px; border-bottom: 1px solid var(--border); font-size: 13px; }
tr:last-child td { border-bottom: none; }
tr:hover td { background: var(--surface2); }
.badge { display: inline-block; padding: 2px 8px; border-radius: 10px; font-size: 11px; font-weight: 600; }
.badge-high { background: #1b3a2b; color: var(--good); }
.badge-medium { background: #2d2a10; color: var(--warn); }
.badge-low { background: #2d1a1a; color: var(--bad); }
.badge-none { background: var(--surface2); color: var(--text-muted); }
.badge-approx { background: #1e2540; color: var(--accent2); }
.bar-wrap { display: flex; align-items: center; gap: 8px; }
.bar { height: 6px; border-radius: 3px; background: var(--accent); min-width: 2px; }
.bar.recv { background: var(--accent2); }
.quality-row { display: flex; justify-content: space-between; padding: 10px 16px; border-bottom: 1px solid var(--border); font-size: 13px; }
.quality-row:last-child { border-bottom: none; }
.quality-label { color: var(--text-muted); }
.quality-val { font-weight: 600; }
.quality-val.warn { color: var(--warn); }
.quality-val.bad  { color: var(--bad); }
.quality-val.good { color: var(--good); }
.timeline-wrap { overflow-x: auto; }
.timeline-grid { display: flex; gap: 2px; align-items: flex-end; min-height: 80px; }
.tl-bucket { display: flex; flex-direction: column; align-items: center; min-width: 14px; cursor: pointer; position: relative; }
.tl-bucket .bar-act { background: var(--accent); border-radius: 2px 2px 0 0; width: 10px; }
.tl-bucket .bar-net { background: var(--accent2); border-radius: 2px 2px 0 0; width: 10px; margin-top: 1px; }
.tl-bucket:hover .tooltip { display: block; }
.tooltip { display: none; position: absolute; bottom: 100%; left: 50%; transform: translateX(-50%); background: var(--surface2); border: 1px solid var(--border); border-radius: 6px; padding: 6px 10px; font-size: 11px; white-space: nowrap; z-index: 10; pointer-events: none; }
.note { font-size: 12px; color: var(--text-muted); margin-top: 8px; font-style: italic; }
.empty { padding: 28px; text-align: center; color: var(--text-muted); font-size: 13px; }
)CSS");
}

// Returns the complete embedded JavaScript for the dashboard HTML.
inline QString dashboardJs()
{
    return QStringLiteral(R"JS(
// ── Tab navigation ────────────────────────────────────────────────────────────
function showTab(id) {
    document.querySelectorAll('.tab-content').forEach(t => t.classList.remove('active'));
    document.querySelectorAll('nav button').forEach(b => b.classList.remove('active'));
    const tabEl = document.getElementById(id);
    if (tabEl) tabEl.classList.add('active');
    const btnEl = document.querySelector('nav button[data-tab="' + id + '"]');
    if (btnEl) btnEl.classList.add('active');
}

// ── Table sort ────────────────────────────────────────────────────────────────
function initSort(tableId) {
    const table = document.getElementById(tableId);
    if (!table) return;
    const ths = table.querySelectorAll('th[data-col]');
    let lastCol = -1, asc = true;
    ths.forEach(th => {
        th.innerHTML += ' <span class="sort-icon">\u25B2</span>';
        th.addEventListener('click', () => {
            const col = parseInt(th.dataset.col);
            asc = (col === lastCol) ? !asc : true;
            lastCol = col;
            ths.forEach(h => h.classList.remove('sorted'));
            th.classList.add('sorted');
            th.querySelector('.sort-icon').textContent = asc ? '\u25B2' : '\u25BC';
            const tbody = table.querySelector('tbody');
            const rows = Array.from(tbody.querySelectorAll('tr'));
            rows.sort((a, b) => {
                const av = a.cells[col]?.dataset.val ?? a.cells[col]?.textContent ?? '';
                const bv = b.cells[col]?.dataset.val ?? b.cells[col]?.textContent ?? '';
                const an = parseFloat(av), bn = parseFloat(bv);
                const cmp = (!isNaN(an) && !isNaN(bn)) ? an - bn : av.localeCompare(bv);
                return asc ? cmp : -cmp;
            });
            rows.forEach(r => tbody.appendChild(r));
        });
    });
}

// ── Table filter ──────────────────────────────────────────────────────────────
function initFilter(inputId, tableId) {
    const input = document.getElementById(inputId);
    if (!input) return;
    input.addEventListener('input', () => {
        const q = input.value.toLowerCase();
        document.querySelectorAll('#'+tableId+' tbody tr').forEach(row => {
            row.style.display = row.textContent.toLowerCase().includes(q) ? '' : 'none';
        });
    });
}

// ── Byte formatter ────────────────────────────────────────────────────────────
function fmtBytes(b) {
    if (b < 1024) return b + ' B';
    if (b < 1048576) return (b/1024).toFixed(1) + ' KB';
    if (b < 1073741824) return (b/1048576).toFixed(1) + ' MB';
    return (b/1073741824).toFixed(2) + ' GB';
}

function fmtSecs(s) {
    if (s < 60) return s + 's';
    if (s < 3600) return Math.floor(s/60) + 'm ' + (s%60) + 's';
    return Math.floor(s/3600) + 'h ' + Math.floor((s%3600)/60) + 'm';
}

function confidenceBadge(c) {
    const cls = {high:'badge-high', medium:'badge-medium', low:'badge-low'}[c] ?? 'badge-none';
    return '<span class="badge '+cls+'">'+(c||'none')+'</span>';
}

// ── Timeline bars ─────────────────────────────────────────────────────────────
function buildTimeline(buckets) {
    const wrap = document.getElementById('tl-grid');
    if (!wrap || !buckets.length) return;
    const maxAct = Math.max(...buckets.map(b => b.activeSeconds), 1);
    const maxNet = Math.max(...buckets.map(b => b.totalBytes), 1);
    const maxH = 60;
    buckets.forEach(b => {
        const div = document.createElement('div');
        div.className = 'tl-bucket';
        const actH = Math.round((b.activeSeconds / maxAct) * maxH);
        const netH = Math.round((b.totalBytes  / maxNet) * maxH);
        div.innerHTML = `
            <div class="tooltip">
                ${b.label}<br>
                App: ${b.foregroundProcess || '-'}<br>
                Active: ${fmtSecs(b.activeSeconds)}<br>
                Net: ${fmtBytes(b.totalBytes)}
            </div>
            <div class="bar-act" style="height:${actH}px"></div>
            <div class="bar-net" style="height:${netH}px"></div>`;
        wrap.appendChild(div);
    });
}

// ── Initialise on load ────────────────────────────────────────────────────────
window.addEventListener('DOMContentLoaded', () => {
    showTab('overview');

    // Activity table
    initSort('act-table');
    initFilter('act-filter', 'act-table');

    // Network by process table
    initSort('net-proc-table');
    initFilter('net-proc-filter', 'net-proc-table');

    // Network detailed records table
    initSort('net-rec-table');
    initFilter('net-rec-filter', 'net-rec-table');

    // App usage table
    initSort('app-table');
    initFilter('app-filter', 'app-table');

    // Timeline
    if (typeof DASHBOARD_DATA !== 'undefined') {
        buildTimeline(DASHBOARD_DATA.timeline || []);
        renderActivity(DASHBOARD_DATA.activityByProcess || []);
        renderNetworkByProcess(DASHBOARD_DATA.networkByProcess || []);
        renderNetworkRecords(DASHBOARD_DATA.networkRecords || []);
        renderAppUsage(DASHBOARD_DATA.appUsage || []);
    }
});

function renderActivity(rows) {
    const tbody = document.querySelector('#act-table tbody');
    if (!tbody) return;
    if (!rows.length) { tbody.innerHTML = '<tr><td colspan="5" class="empty">No activity data</td></tr>'; return; }
    tbody.innerHTML = rows.map(r => `<tr>
        <td>${r.processName}</td>
        <td data-val="${r.totalActiveDurationSeconds}">${fmtSecs(r.totalActiveDurationSeconds)}</td>
        <td data-val="${r.sessionCount}">${r.sessionCount}</td>
        <td data-val="${r.totalKeystrokes}">${r.totalKeystrokes.toLocaleString()}</td>
        <td data-val="${r.totalMouseDistancePx}">${Math.round(r.totalMouseDistancePx).toLocaleString()} px</td>
    </tr>`).join('');
}

function renderNetworkByProcess(rows) {
    const tbody = document.querySelector('#net-proc-table tbody');
    if (!tbody) return;
    if (!rows.length) { tbody.innerHTML = '<tr><td colspan="6" class="empty">No network data</td></tr>'; return; }
    tbody.innerHTML = rows.map(r => `<tr>
        <td>${r.processName}</td>
        <td data-val="${r.bytesSent}">${fmtBytes(r.bytesSent)}</td>
        <td data-val="${r.bytesReceived}">${fmtBytes(r.bytesReceived)}</td>
        <td data-val="${r.totalBytes}">${fmtBytes(r.totalBytes)}</td>
        <td data-val="${r.sessionCount}">${r.sessionCount}</td>
        <td>${(r.protocolHints||[]).join(', ')}</td>
    </tr>`).join('');
}

function renderNetworkRecords(rows) {
    const tbody = document.querySelector('#net-rec-table tbody');
    if (!tbody) return;
    if (!rows.length) { tbody.innerHTML = '<tr><td colspan="8" class="empty">No network records</td></tr>'; return; }
    tbody.innerHTML = rows.map(r => `<tr>
        <td>${r.processName||'unknown'}</td>
        <td>${r.remoteHostname || r.remoteIp}</td>
        <td data-val="${r.remotePort}">${r.remotePort}</td>
        <td>${r.appProtocolHint||r.transportProtocol}</td>
        <td data-val="${r.bytesSent}">${fmtBytes(r.bytesSent)}</td>
        <td data-val="${r.bytesReceived}">${fmtBytes(r.bytesReceived)}</td>
        <td>${confidenceBadge(r.hostnameConfidence)}</td>
        <td>${r.closeReason||''}</td>
    </tr>`).join('');
}

function renderAppUsage(rows) {
    const tbody = document.querySelector('#app-table tbody');
    if (!tbody) return;
    if (!rows.length) { tbody.innerHTML = '<tr><td colspan="6" class="empty">No data</td></tr>'; return; }
    tbody.innerHTML = rows.map(r => `<tr>
        <td>${r.processName}</td>
        <td data-val="${r.activeDurationSeconds}">${fmtSecs(r.activeDurationSeconds)}</td>
        <td data-val="${r.keystrokeCount}">${r.keystrokeCount.toLocaleString()}</td>
        <td data-val="${r.bytesSent}">${fmtBytes(r.bytesSent)}</td>
        <td data-val="${r.bytesReceived}">${fmtBytes(r.bytesReceived)}</td>
        <td><span class="badge badge-approx">${r.correlationConfidence}</span></td>
    </tr>`).join('');
}
)JS");
}

} // namespace Analytics

#endif // ANALYTICS_DASHBOARD_DASHBOARDTEMPLATE_H
