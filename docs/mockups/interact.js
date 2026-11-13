/* MODX M mockup behaviour layer. One set of rules for every page:
   routing, on/off controls, menus, EQ graphs, value readouts. Loaded after support.js. */
(function () {
  'use strict';
  if (window.__modxIx) return; window.__modxIx = 1;

  var FILES = { Home: 'MODX2_Home.dc.html', Nav: 'MODX2_Nav.dc.html', AWM2: 'MODX2_AWM2.dc.html', FMX: 'MODX2_FMX.dc.html', ANX: 'MODX2_ANX.dc.html', FX: 'MODX2_FX.dc.html', Motion: 'MODX2_Motion.dc.html', Morph: 'MODX2_Morph.dc.html', Library: 'MODX2_Library.dc.html', Output: 'MODX2_Output.dc.html' };
  var PAGE = ((location.pathname.match(/MODX2_(\w+)\.dc\.html/) || [])[1]) || '';
  var ENGINES = { AWM2: 1, FMX: 1, ANX: 1 };
  function T(el) { return (el.innerText || el.textContent || '').trim().replace(/\s+/g, ' '); }
  function store(k, v) { try { if (v === undefined) return sessionStorage.getItem(k); sessionStorage.setItem(k, v); } catch (e) { } return null; }
  function go(page, hash) { location.href = FILES[page] + (hash ? '#' + encodeURIComponent(hash) : ''); }

  // ---------- effect lists from the MODX M Data List (Effect Type List, columns REV / VAR / Ins / Master) ----------
  var FX = {
    REV: ['No Effect', 'HD Hall', 'REV-X Hall', 'R3 Hall', 'SPX Hall', 'HD Room', 'REV-X Room', 'R3 Room', 'SPX Room', 'HD Plate', 'R3 Plate', 'SPX Stage', 'Space Simulator', 'Shimmer Reverb'],
    ALL: ['No Effect', 'SPX Hall', 'SPX Room', 'SPX Stage', 'Gated Reverb', 'Reverse Reverb', 'Cross Delay', 'Tempo Cross Delay', 'Tempo Delay Mono', 'Tempo Delay Stereo', 'Control Delay', 'Delay LR', 'Delay LCR', 'Analog Delay Retro', 'Analog Delay Modern', 'G Chorus', '2 Modulator', 'SPX Chorus', 'Symphonic', 'Ensemble Detune', 'GS1 Ensemble & Tremolo', 'VCM Flanger', 'Classic Flanger', 'Tempo Flanger', 'Dynamic Flanger', 'Control Flanger', 'VCM Phaser Mono', 'VCM Phaser Stereo', 'Tempo Phaser', 'Dynamic Phaser', 'Control Phaser', 'Amp Simulator 1', 'Amp Simulator 2', 'Comp Distortion', 'Comp Distortion Delay', 'US Combo', 'Jazz Combo', 'US High Gain', 'British Lead', 'Multi FX', 'Small Stereo', 'British Combo', 'British Legend', 'VCM Compressor 376', 'Classic Compressor', 'Multi Band Comp', 'Uni Comp Down', 'Uni Comp Up', 'Parallel Comp']
  };

  // ---------- toast ----------
  var toastEl, toastT;
  function toast(msg) {
    if (!toastEl) { toastEl = document.createElement('div'); toastEl.setAttribute('role', 'status'); toastEl.style.cssText = 'position:fixed;left:50%;bottom:22px;transform:translateX(-50%);z-index:100000;background:#16181C;color:#E4E6EA;border:1px solid #2ECBD5;border-radius:6px;padding:8px 14px;font:13px "Roboto Condensed",system-ui,sans-serif;pointer-events:none;opacity:0;transition:opacity .15s'; document.body.appendChild(toastEl); }
    toastEl.textContent = msg; toastEl.style.opacity = '1'; clearTimeout(toastT); toastT = setTimeout(function () { toastEl.style.opacity = '0'; }, 2200);
  }

  // ---------- modal (Sync with MODX) ----------
  function modal(title, lines, doneText) {
    var ov = document.createElement('div'); ov.style.cssText = 'position:fixed;inset:0;z-index:100001;background:rgba(0,0,0,.55);display:flex;align-items:center;justify-content:center';
    var box = document.createElement('div'); box.style.cssText = 'width:min(420px,90vw);background:#16181C;color:#E4E6EA;border:1px solid #2C3036;border-radius:8px;padding:16px 18px;font:14px "Roboto Condensed",system-ui,sans-serif;display:grid;gap:8px';
    box.innerHTML = '<b style="font-size:16px">' + title + '</b>';
    var rows = lines.map(function (l) { var d = document.createElement('div'); d.style.cssText = 'color:#8E949D;display:flex;gap:8px'; d.innerHTML = '<span style="width:14px;color:#2ECBD5">·</span><span>' + l + '</span>'; box.appendChild(d); return d; });
    var btn = document.createElement('button'); btn.textContent = 'Close'; btn.style.cssText = 'justify-self:end;margin-top:6px;height:26px;padding:0 14px;border-radius:3px;border:1px solid #2C3036;background:#1F2226;color:#E4E6EA;font:inherit;cursor:pointer';
    box.appendChild(btn); ov.appendChild(box); document.body.appendChild(ov);
    function close() { ov.remove(); document.removeEventListener('keydown', key, true); }
    function key(e) { if (e.key === 'Escape') close(); }
    document.addEventListener('keydown', key, true); btn.onclick = close; ov.addEventListener('pointerdown', function (e) { if (e.target === ov) close(); });
    var i = 0; (function step() { if (!ov.isConnected) return; if (i < rows.length) { rows[i].firstChild.textContent = '✓'; rows[i].style.color = '#E4E6EA'; i++; setTimeout(step, 320); } else if (doneText) { var d = document.createElement('div'); d.style.color = '#3FC07A'; d.textContent = doneText; box.insertBefore(d, btn); } })();
  }

  // ---------- section focus (arrive on a page and land on a named section or tab) ----------
  function findByText(names) {
    var all = document.querySelectorAll('button,a,span,div,b,h1,h2,h3'), best = null;
    for (var n = 0; n < names.length && !best; n++) {
      var want = names[n].toLowerCase();
      for (var i = 0; i < all.length; i++) {
        var e = all[i], t = T(e).replace(/[«◂▾]/g, '').trim().toLowerCase();
        if (!t || e.children.length > 3) continue;
        if (t === want || (t.indexOf(want) === 0 && t.length < want.length + 24)) { var r = e.getBoundingClientRect(); if (r.width > 0 && r.height > 0) { best = e; break; } }
      }
    }
    return best;
  }
  function flash(el) {
    var old = el.style.outline; el.style.outline = '2px solid #2ECBD5'; el.style.outlineOffset = '2px'; setTimeout(function () { el.style.outline = old; el.style.outlineOffset = ''; }, 1400);
  }
  function focusHash() {
    var h = decodeURIComponent((location.hash || '').slice(1)); if (!h) return;
    var names = h.split('|'), el = findByText(names); if (!el) return;
    var tab = el.closest('button') || el; if (tab.tagName === 'BUTTON' || tab.getAttribute('role') === 'tab') tab.click();
    var box = el.closest('section') || el; box.scrollIntoView({ block: 'center', behavior: 'smooth' }); flash(box);
  }

  // ---------- routing ----------
  var HEADER = {
    'Search': ['Library', 'Search'], 'Live Set': ['Library', 'Live Set|Live'], 'Files': ['Library', 'Files'], 'Data transfer': ['Output', 'MIDI|Ports']
  };
  var CARDS = { // Navigation page cards -> engine page + the section or tab to land on
    'Osc / Op': ['E', 'Operators|Osc|Oscillator'], 'Modifier': ['E', 'Modifier'], 'Filter': ['E', 'Filter'], 'Amplitude': ['E', 'Amplitude'],
    'Pitch EG': ['E', 'Pitch EG|Pitch'], 'Filter EG': ['E', 'Filter EG|Filter'], 'Part LFO': ['E', 'LFO'],
    '3-band EQ': ['E', '3-band'], '2-band EQ': ['E', '2-band'], 'Arp': ['Motion', 'ARPEGGIO'], 'Motion Seq': ['Motion', 'MOTION SEQUENCER'],
    'Mixing': ['Home', 'Part Name|Volume'], 'Pitch PB': ['E', 'Pitch'], 'Zone': ['E', 'Part'], 'Controller': ['Motion', 'SUPER KNOB'], 'Part Settings': ['E', 'Part']
  };
  function enginePage() { var e = store('modx.engine'); return ENGINES[e] ? e : 'AWM2'; }

  function routeFor(el) {
    var label = (el.getAttribute('aria-label') || '').trim(), t = T(el);
    if (HEADER[label]) return { page: HEADER[label][0], hash: HEADER[label][1] };
    if (label === 'Sync with MODX') return { action: 'sync' };
    var a = el.closest('a');
    if (a) {
      var href = a.getAttribute('href') || '', m = /MODX2_(\w+)\.dc\.html/.exec(href);
      if (m && ENGINES[m[1]]) store('modx.engine', m[1]);
      var sideLabel = T(a);
      if (a.closest('nav,[aria-label="MODX sections"]') || a.style.width === '58px') {
        if (sideLabel === 'Scene') return { page: 'Nav', hash: 'Scene|Scenes' };
        if (sideLabel === 'Part') return { page: enginePage() };
      }
      return null;
    }
    if (PAGE === 'Nav' && CARDS[t.split(' ').slice(0, 3).join(' ')] === undefined) {
      var key = Object.keys(CARDS).filter(function (k) { return t.indexOf(k) === 0; }).sort(function (x, y) { return y.length - x.length; })[0];
      if (key && el.closest('button')) { var c = CARDS[key]; return { page: c[0] === 'E' ? enginePage() : c[0], hash: c[1] }; }
    }
    if (t.indexOf('Waveform browser') === 0) return { page: 'Library', hash: 'Waveform|Waveforms' };
    if (t === 'Super Knob path') return { page: 'Motion', hash: 'SUPER KNOB' };
    return null;
  }

  document.addEventListener('click', function (e) {
    var el = e.target.closest && e.target.closest('button,a,[aria-label]'); if (!el) return;
    var r = routeFor(el); if (!r) return;
    e.preventDefault(); e.stopImmediatePropagation();
    if (r.action === 'sync') { modal('Sync with MODX M', ['Read Performance name and common settings', 'Read 16 part settings', 'Read effects and Master EQ', 'Read Super Knob and Assign knobs'], 'All values read. The window now shows what the synth holds.'); return; }
    if (r.page === PAGE) { location.hash = r.hash ? encodeURIComponent(r.hash) : ''; focusHash(); return; }
    go(r.page, r.hash);
  }, true);

  // ---------- menus: one at a time, close on outside click, Escape, scroll, pick ----------
  var menu = null, menuFor = null;
  function closeMenu() { if (menu) { menu.remove(); menu = null; menuFor = null; } }
  function openMenu(btn, items, cur, onpick) {
    closeMenu(); menuFor = btn; menu = document.createElement('div'); menu.setAttribute('role', 'listbox');
    var r = btn.getBoundingClientRect();
    menu.style.cssText = 'position:fixed;z-index:100002;left:' + Math.max(4, Math.min(r.left, innerWidth - 230)) + 'px;top:' + (r.bottom + 2) + 'px;min-width:' + Math.max(140, r.width) + 'px;max-height:' + Math.max(160, Math.min(320, innerHeight - r.bottom - 12)) + 'px;overflow:auto;background:#121316;color:#E4E6EA;border:1px solid #4A4E57;border-radius:6px;box-shadow:0 10px 30px rgba(0,0,0,.6);padding:4px;font:13px "Roboto Condensed",system-ui,sans-serif';
    items.forEach(function (t) {
      var o = document.createElement('div'); o.setAttribute('role', 'option'); o.textContent = t;
      o.style.cssText = 'padding:5px 10px;border-radius:4px;cursor:pointer;white-space:nowrap;' + (t === cur ? 'background:#1D3B3F;color:#7FE3D6' : '');
      o.onmouseenter = function () { o.style.background = '#2B3038'; }; o.onmouseleave = function () { o.style.background = t === cur ? '#1D3B3F' : ''; };
      o.onclick = function (e) { e.stopPropagation(); onpick(t); closeMenu(); }; menu.appendChild(o);
    });
    document.body.appendChild(menu);
    var sel = menu.querySelector('[style*="1D3B3F"]'); if (sel) menu.scrollTop = Math.max(0, sel.offsetTop - 60);
  }
  document.addEventListener('pointerdown', function (e) { if (menu && !menu.contains(e.target) && !(menuFor && menuFor.contains(e.target))) closeMenu(); }, true);
  document.addEventListener('keydown', function (e) { if (e.key === 'Escape') closeMenu(); }, true);
  window.addEventListener('scroll', closeMenu, true); window.addEventListener('blur', closeMenu); window.addEventListener('resize', closeMenu);

  function slotOf(btn) {
    var p = btn;
    for (var i = 0; i < 5 && p && p !== document.body; i++, p = p.parentElement) {
      var kids = p.querySelectorAll('span,b,div'), s = null;
      for (var k = 0; k < kids.length && !s; k++) { var t = T(kids[k]); if (kids[k].children.length === 0 && /^(Rev|Var|MFX|A|B|Master|Ins A|Ins B)$/.test(t)) s = t; }
      if (s) return s;
    }
    return '';
  }
  function labelOf(btn) {
    var p = btn.previousElementSibling; if (p && p.children.length === 0 && T(p).length < 20) return T(p);
    var up = btn.parentElement && btn.parentElement.previousElementSibling; return up && up.children.length === 0 ? T(up) : '';
  }
  function itemsFor(btn) {
    var lab = labelOf(btn), slot = slotOf(btn), txt = T(btn).replace(/\s*[▾▼]\s*$/, '');
    if (lab === 'Type' || /^(No Effect|REV|SPX|HD|R3)/i.test(txt) || FX.ALL.concat(FX.REV).some(function (x) { return x.toLowerCase() === txt.toLowerCase(); })) {
      var l = slot === 'Rev' ? FX.REV : FX.ALL; return l;
    }
    var same = {}, out = []; var sig = btn.getAttribute('style') || '';
    document.querySelectorAll('button').forEach(function (b) { if ((b.getAttribute('style') || '') === sig && /▾\s*$/.test(T(b))) { var t = T(b).replace(/\s*[▾▼]\s*$/, ''); if (!same[t]) { same[t] = 1; out.push(t); } } });
    if (out.indexOf(txt) < 0) out.unshift(txt); return out;
  }
  document.addEventListener('click', function (e) {
    var btn = e.target.closest && e.target.closest('button'); if (!btn || !/▾\s*$/.test(T(btn))) return;
    if (btn.getAttribute('aria-label') || btn.closest('a')) return;
    e.preventDefault(); e.stopImmediatePropagation();
    if (menu && menuFor === btn) { closeMenu(); return; }
    var cur = T(btn).replace(/\s*[▾▼]\s*$/, '');
    openMenu(btn, itemsFor(btn), cur, function (v) { btn.textContent = v + ' ▾'; });
  }, true);

  // ---------- on/off: switch pills and toggle buttons, one look each ----------
  var PILL_ON = '#2F7FE0', PILL_OFF = '#33373E';
  function isPill(s) { return s.tagName === 'SPAN' && /width:\s*30px/.test(s.getAttribute('style') || '') && /height:\s*15px/.test(s.getAttribute('style') || ''); }
  function pillOn(s) { return /#2F7FE0/i.test(s.getAttribute('style') || '') || (s.dataset.on === '1'); }
  function setPill(s, on) {
    s.dataset.on = on ? '1' : '0'; s.style.background = on ? PILL_ON : PILL_OFF; s.style.cursor = 'pointer';
    var th = s.firstElementChild; if (!th) { th = document.createElement('span'); s.appendChild(th); }
    th.setAttribute('style', 'position:absolute;top:2px;' + (on ? 'right:2px;left:auto' : 'left:2px;right:auto') + ';width:11px;height:11px;border-radius:50%;background:#FFF;transition:all .12s');
  }
  function initPills() { document.querySelectorAll('span').forEach(function (s) { if (isPill(s)) { setPill(s, pillOn(s)); s.setAttribute('role', 'switch'); s.setAttribute('aria-checked', pillOn(s) ? 'true' : 'false'); } }); }
  document.addEventListener('click', function (e) {
    var s = e.target.closest && e.target.closest('span'); if (!s || !isPill(s)) return;
    e.stopImmediatePropagation(); setPill(s, !pillOn(s)); s.setAttribute('aria-checked', pillOn(s) ? 'true' : 'false');
  }, true);

  var TOGGLE_TEXT = /^(Mute|Solo|Porta-? ?mento|Arp Master|MS Master|On|Sync|Learn|Draw|All)$/;
  var ACCENT = { Mute: '#C0602A', Solo: '#E9C46A', def: '#3FC07A' };
  function toggleBtn(b) { var t = T(b).replace(/\s*-\s*/, '-'); return TOGGLE_TEXT.test(t) ? t : null; }
  function styleBtn(b, on) {
    var t = toggleBtn(b), acc = b.dataset.accent || ACCENT[t] || ACCENT.def;
    b.dataset.on = on ? '1' : '0';
    if (on) { b.style.background = acc; b.style.color = '#071014'; b.style.borderColor = acc; }
    else { b.style.background = '#1F2226'; b.style.color = '#9EA3AA'; b.style.borderColor = '#2C3036'; }
    b.setAttribute('aria-pressed', on ? 'true' : 'false');
  }
  function initToggleBtns() {
    document.querySelectorAll('button').forEach(function (b) {
      if (!toggleBtn(b) || b.closest('a') || /▾/.test(T(b))) return;
      var bg = getComputedStyle(b).backgroundColor, neutral = /rgb\((31|44|22|15|8), ?\d+, ?\d+\)|rgba\(0, 0, 0, 0\)/.test(bg);
      if (!neutral) b.dataset.accent = bg; styleBtn(b, !neutral);
    });
  }
  document.addEventListener('click', function (e) {
    var b = e.target.closest && e.target.closest('button'); if (!b || !toggleBtn(b) || b.dataset.on === undefined) return;
    e.stopImmediatePropagation(); styleBtn(b, b.dataset.on !== '1');
  }, true);

  // ---------- Pan style knob: centre points straight up, arc runs out to the left or right ----------
  function initPan() {
    document.querySelectorAll('svg').forEach(function (svg) {
      var lab = svg.previousElementSibling, val = svg.nextElementSibling;
      if (!lab || !val || !/^Pan$/i.test(T(lab)) || svg.dataset.pan) return;
      var c = svg.querySelector('circle'), path = svg.querySelector('path'), line = svg.querySelector('line'); if (!c || !path || !line) return;
      svg.dataset.pan = '1'; svg.setAttribute('data-own', '1');
      var cx = +c.getAttribute('cx'), cy = +c.getAttribute('cy'), r = +c.getAttribute('r'), v = 64;
      function render() {
        var a = (v - 64) / 64 * 135, rad = a * Math.PI / 180, ex = cx + r * Math.sin(rad), ey = cy - r * Math.cos(rad);
        line.setAttribute('x2', ex.toFixed(1)); line.setAttribute('y2', ey.toFixed(1));
        path.setAttribute('d', v === 64 ? 'M' + cx + ',' + (cy - r) : 'M' + cx + ',' + (cy - r) + ' A' + r + ',' + r + ' 0 0 ' + (v > 64 ? 1 : 0) + ' ' + ex.toFixed(1) + ',' + ey.toFixed(1));
        val.textContent = v === 64 ? 'C' : v < 64 ? 'L' + (64 - v) : 'R' + (v - 64);
      }
      svg.style.cursor = 'ns-resize'; svg.style.touchAction = 'none';
      svg.addEventListener('pointerdown', function (ev) {
        ev.preventDefault(); svg.setPointerCapture(ev.pointerId); var y0 = ev.clientY, v0 = v;
        function mv(m) { v = Math.max(1, Math.min(127, Math.round(v0 - (m.clientY - y0) * 0.8))); render(); }
        function up() { svg.removeEventListener('pointermove', mv); svg.removeEventListener('pointerup', up); }
        svg.addEventListener('pointermove', mv); svg.addEventListener('pointerup', up);
      });
      svg.addEventListener('wheel', function (e) { e.preventDefault(); v = Math.max(1, Math.min(127, v - Math.sign(e.deltaY))); render(); }, { passive: false });
      svg.addEventListener('dblclick', function () { v = 64; render(); });
      render();
    });
  }

  // ---------- Volume slider with a live value ----------
  function initVolume() {
    document.querySelectorAll('span').forEach(function (lab) {
      if (T(lab) !== 'Volume' || lab.dataset.vol || lab.children.length) return;
      var track = lab.nextElementSibling; if (!track || !track.firstElementChild) return;
      var fill = track.firstElementChild; lab.dataset.vol = '1';
      var row = lab.parentElement; row.style.position = 'relative';
      var val = document.createElement('span'); val.style.cssText = 'position:absolute;right:0;top:0;font-size:10px;color:#2ECBD5'; row.appendChild(val);
      var v = Math.round(parseFloat(fill.style.width) / 100 * 127);
      function render() { fill.style.width = (v / 127 * 100) + '%'; val.textContent = v; }
      track.style.cursor = 'ew-resize'; track.style.touchAction = 'none'; track.style.padding = '4px 0'; track.style.backgroundClip = 'content-box';
      function set(e) { var r = track.getBoundingClientRect(); v = Math.max(0, Math.min(127, Math.round((e.clientX - r.left) / r.width * 127))); render(); }
      track.addEventListener('pointerdown', function (e) { e.preventDefault(); track.setPointerCapture(e.pointerId); set(e); function mv(m) { set(m); } function up() { track.removeEventListener('pointermove', mv); track.removeEventListener('pointerup', up); } track.addEventListener('pointermove', mv); track.addEventListener('pointerup', up); });
      track.addEventListener('wheel', function (e) { e.preventDefault(); v = Math.max(0, Math.min(127, v - Math.sign(e.deltaY))); render(); }, { passive: false });
      track.addEventListener('dblclick', function () { v = 100; render(); });
      render();
    });
  }

  // ---------- EQ graphs: round dots, sharp text, draggable bands ----------
  var FMIN = 20, FMAX = 20000;
  function fx(f) { return (Math.log(f / FMIN) / Math.log(FMAX / FMIN)); }       // 0..1 across the plot
  function fy(x) { return FMIN * Math.pow(FMAX / FMIN, x); }
  function fmtF(f) { return f >= 1000 ? (f / 1000).toFixed(f >= 10000 ? 0 : 1).replace(/\.0$/, '') + 'k' : String(Math.round(f)); }
  function fmtG(g) { return (g > 0 ? '+' : g < 0 ? '-' : '') + Math.abs(g).toFixed(1); }
  var COLORS = ['#2ECBD5', '#E9C46A', '#F27FC0', '#3FC07A'];
  function initEq() {
    document.querySelectorAll('svg[aria-label*="curve"]').forEach(function (svg) {
      var al = svg.getAttribute('aria-label') || ''; if (!/band EQ curve|over live output curve/.test(al) || /spectrum/i.test(al)) return;
      if (svg.dataset.eq) return; svg.dataset.eq = '1';
      var H = +svg.getAttribute('viewBox').split(' ')[3], sec = svg.closest('section') || svg.parentElement.parentElement;
      var head = sec.querySelector('div[style*="height: 18px"]') || sec.querySelector('div'); var title = T(head).toLowerCase();
      var n = /3-band/.test(title) ? 3 : /2-band/.test(title) ? 2 : 4;
      var kinds = n === 3 ? ['ls', 'pk', 'hs'] : n === 2 ? ['ls', 'hs'] : ['ls', 'pk', 'pk', 'hs'];
      var defaults = n === 3 ? [[100, 3], [1000, 0], [8000, 1.5]] : n === 2 ? [[300, 2], [6000, -1]] : [[60, 3], [200, 0], [1500, 0], [8000, -1]];
      // read the starting values from the tiles that sit under the graph when present
      var tiles = {}; sec.parentElement.querySelectorAll('span').forEach(function (s) { var t = T(s); if (/^EQ (Low|Mid|Hi) (Gain|Freq)$/.test(t) && s.nextElementSibling) tiles[t] = s.nextElementSibling.nextElementSibling || s.nextElementSibling; });
      var wrap = document.createElement('div'); wrap.style.cssText = 'position:relative;height:' + H + 'px;background:#0F1113;user-select:none;touch-action:none';
      var NS = 'http://www.w3.org/2000/svg', s2 = document.createElementNS(NS, 'svg'); s2.setAttribute('viewBox', '0 0 300 ' + H); s2.setAttribute('preserveAspectRatio', 'none');
      s2.style.cssText = 'position:absolute;left:30px;width:calc(100% - 30px);top:0;height:' + H + 'px'; wrap.appendChild(s2);
      var top = 10, bot = H >= 110 ? 100 : 90, mid = (top + bot) / 2, ph = bot - top;
      [0, 1, 2, 3, 4].forEach(function (i) { var y = top + ph * i / 4; var l = document.createElementNS(NS, 'line'); l.setAttribute('x1', 0); l.setAttribute('x2', 300); l.setAttribute('y1', y); l.setAttribute('y2', y); l.setAttribute('stroke', i === 2 ? '#30353C' : '#262A30'); l.setAttribute('vector-effect', 'non-scaling-stroke'); s2.appendChild(l);
        var t = document.createElement('span'); t.textContent = ['+24', '+12', '0', '-12', '-24'][i]; t.style.cssText = 'position:absolute;left:3px;top:' + (y - 6) + 'px;font-size:9px;line-height:12px;color:#6E747D'; wrap.appendChild(t); });
      [[20, 'left'], [100], [1000], [10000], [20000, 'right']].forEach(function (f) { var t = document.createElement('span'); t.textContent = fmtF(f[0]); var x = fx(f[0]); t.style.cssText = 'position:absolute;bottom:2px;font-size:9px;line-height:12px;color:#6E747D;left:calc(30px + (100% - 30px) * ' + x.toFixed(4) + ');transform:translateX(' + (f[1] === 'left' ? '0' : f[1] === 'right' ? '-100%' : '-50%') + ')'; wrap.appendChild(t); });
      var curve = document.createElementNS(NS, 'path'); curve.setAttribute('fill', 'none'); curve.setAttribute('stroke', getComputedStyle(head).backgroundColor && /rgb/.test(head.style.background) ? '#2ECBD5' : '#2ECBD5'); curve.setAttribute('stroke-width', 2); curve.setAttribute('vector-effect', 'non-scaling-stroke'); s2.appendChild(curve);
      var rgb = /rgb\((\d+), (\d+), (\d+)\)/.exec(getComputedStyle(head).backgroundImage + getComputedStyle(head).backgroundColor); curve.setAttribute('stroke', rgb ? 'rgb(' + rgb[1] + ',' + rgb[2] + ',' + rgb[3] + ')' : '#2ECBD5');
      var names = n === 3 ? ['Low', 'Mid', 'Hi'] : n === 2 ? ['Low', 'Hi'] : null;
      var bands = defaults.map(function (d, i) {
        var b = { f: d[0], g: d[1], kind: kinds[i], i: i };
        if (names) { var tg = tiles['EQ ' + names[i] + ' Gain'], tf = tiles['EQ ' + names[i] + ' Freq']; if (tg) b.g = parseFloat(T(tg).replace('−', '-')) || 0; if (tf) { var m = /([\d.]+)(k?)/.exec(T(tf)); if (m) b.f = parseFloat(m[1]) * (m[2] ? 1000 : 1); } b.tg = tg; b.tf = tf; }
        return b;
      });
      function resp(f) { var g = 0, x = Math.log(f); bands.forEach(function (b) { var d = x - Math.log(b.f); if (b.kind === 'pk') g += b.g * Math.exp(-d * d / (2 * 0.55 * 0.55)); else if (b.kind === 'ls') g += b.g / (1 + Math.exp(d * 2.2)); else g += b.g / (1 + Math.exp(-d * 2.2)); }); return g; }
      function gy(g) { return mid - g / 24 * (ph / 2); }
      function px(f) { return 'calc(30px + (100% - 30px) * ' + fx(f).toFixed(4) + ')'; }
      var dots = [];
      function draw() {
        var d = ''; for (var i = 0; i <= 120; i++) { var f = fy(i / 120); d += (i ? 'L' : 'M') + (i / 120 * 300).toFixed(1) + ',' + gy(resp(f)).toFixed(2); } curve.setAttribute('d', d);
        bands.forEach(function (b, i) { var dt = dots[i]; dt.style.left = px(b.f); dt.style.top = (gy(b.g) - 5) + 'px'; if (b.tg) b.tg.textContent = fmtG(b.g); if (b.tf) b.tf.textContent = fmtF(b.f); });
        readouts();
      }
      function readouts() { // Home: "Sub +3  Low 0  Mid 0  High -1" row under the graph
        if (n !== 4) return;
        var row = sec.querySelector('div[style*="margin-top: 4px"]'); if (!row) return; var nm = ['Sub', 'Low', 'Mid', 'High'];
        [].slice.call(row.children).forEach(function (c) { var t = T(c); nm.forEach(function (name, i) { if (t.indexOf(name) === 0) { var v = fmtG(bands[i].g).replace(/\.0$/, ''); if (c.children[0]) c.children[0].textContent = v; else c.textContent = name + ' ' + v; } }); });
      }
      bands.forEach(function (b, i) {
        var dt = document.createElement('div'); dt.style.cssText = 'position:absolute;width:10px;height:10px;margin-left:-5px;border-radius:50%;background:' + COLORS[i] + ';cursor:grab;box-shadow:0 0 0 2px rgba(0,0,0,.35)'; dt.title = (names ? names[i] : ['Sub', 'Low', 'Mid', 'High'][i]) + ' band: drag'; dots.push(dt); wrap.appendChild(dt);
        dt.addEventListener('pointerdown', function (e) {
          e.preventDefault(); e.stopPropagation(); dt.setPointerCapture(e.pointerId); dt.style.cursor = 'grabbing';
          var r = wrap.getBoundingClientRect(), x0 = r.left + 30, w = r.width - 30;
          function mv(m) { var u = Math.max(0, Math.min(1, (m.clientX - x0) / w)); b.f = Math.max(20, Math.min(20000, fy(u))); b.g = Math.max(-24, Math.min(24, Math.round((mid - (m.clientY - r.top)) / (ph / 2) * 24 * 2) / 2)); draw(); }
          function up() { dt.removeEventListener('pointermove', mv); dt.removeEventListener('pointerup', up); dt.style.cursor = 'grab'; }
          dt.addEventListener('pointermove', mv); dt.addEventListener('pointerup', up);
        });
        dt.addEventListener('dblclick', function () { b.f = defaults[i][0]; b.g = 0; draw(); });
      });
      svg.replaceWith(wrap); draw();
    });
  }


  // ---------- chips, step counters, audition, one-shot actions ----------
  var CHIP = /^(★ |☆ )?(All engines|AWM2|FM-X|AN-X|Drum|Bass|Pad \/ Choir|Keys|Strings|Lead|Hook|Bright|UK Hardcore|Moving|Analog)\b/;
  function isChip(b) { return (PAGE === 'Library' || PAGE === 'Output' || PAGE === 'Morph') && CHIP.test(T(b)) && !/▾/.test(T(b)) && !toggleBtn(b) && b.children.length < 3; }
  function setChip(b, on) { b.dataset.chip = on ? '1' : '0'; b.style.boxShadow = on ? 'inset 0 0 0 2px #7FE3D6' : ''; b.style.filter = on ? 'brightness(1.25)' : ''; b.setAttribute('aria-pressed', on ? 'true' : 'false'); }
  var playing = null, playT = null;
  function stopPlay() { if (playing) { playing.textContent = '▶'; playing.style.background = '#2C3036'; playing.style.color = '#E8E7E3'; playing = null; } clearTimeout(playT); }
  document.addEventListener('click', function (e) {
    var b = e.target.closest && e.target.closest('button'); if (!b) return;
    var t = T(b);
    if (isChip(b)) { e.stopImmediatePropagation(); setChip(b, b.dataset.chip !== '1'); return; }
    if (t === '▶' || t === '■') { e.stopImmediatePropagation(); var was = playing === b; stopPlay(); if (!was) { playing = b; b.textContent = '■'; b.style.background = '#2ECBD5'; b.style.color = '#071014'; playT = setTimeout(stopPlay, 5000); toast('Auditioning on the MODX M. Press again to stop.'); } return; }
    var m = /^Replace Part (\d+)$/.exec(t); if (m) { e.stopImmediatePropagation(); toast('Part ' + m[1] + ' now holds the selected sound. Sent to the MODX M.'); return; }
    if (/^Learn from MODX performance$|^Learn from DAW track$/.test(t)) { e.stopImmediatePropagation(); modal(t, ['Reading the source', 'Building the target curve', 'Applying to the Master EQ'], 'Done. The curve is now the reference.'); return; }
    if (t === 'Average selected') { e.stopImmediatePropagation(); toast('Averaged the selected reference curves.'); return; }
    if (PAGE === 'Motion' && /^\d+$/.test(t) && /On\s*Sync/.test(T(b.parentElement))) {
      e.stopImmediatePropagation(); if (menu && menuFor === b) { closeMenu(); return; }
      var items = []; for (var i = 1; i <= 16; i++) items.push(String(i)); openMenu(b, items, t, function (v) { b.textContent = v; }); }
  }, true);

  // ---------- pick-one groups: Fingered / Full-time, FM-X algorithm tiles, oscillator wave buttons ----------
  function radioGroup(b) {
    var P = b.parentElement; if (!P) return null;
    var al = b.getAttribute('aria-label') || '';
    if (/^Algorithm \d+$/.test(al)) return { kind: 'tile', items: [].slice.call(P.querySelectorAll('button[aria-label^="Algorithm"]')) };
    var t = T(b); if (t === 'Fingered' || t === 'Full-time') return { kind: 'seg', items: [].slice.call(P.querySelectorAll('button')) };
    return null;
  }
  function pick(b, g) {
    g.items.forEach(function (x) {
      var on = x === b; x.dataset.pick = on ? '1' : '0'; x.setAttribute('aria-pressed', on ? 'true' : 'false');
      if (g.kind === 'tile') { x.style.borderColor = on ? '#2ECBD5' : '#2C3036'; x.style.background = on ? '#12272A' : '#0F1113'; }
      else { x.style.background = on ? '#2E6F55' : '#1F2226'; x.style.color = on ? '#071014' : '#E4E6EA'; x.style.borderColor = on ? '#2E6F55' : '#2C3036'; }
    });
  }
  function initPicks() {
    document.querySelectorAll('button').forEach(function (b) { var g = radioGroup(b); if (g && b.dataset.pick === undefined) { delete b.dataset.on; b.dataset.pick = '0'; } });
    document.querySelectorAll('button').forEach(function (b) { var g = radioGroup(b); if (!g || b.dataset.pickInit) return; g.items.forEach(function (x) { x.dataset.pickInit = '1'; });
      if (g.kind === 'seg') { var cur = g.items.filter(function (x) { return T(x) === 'Fingered'; })[0]; if (cur) pick(cur, g); }
      else if (g.items.length) pick(g.items[0], g); });
  }
  document.addEventListener('click', function (e) {
    var b = e.target.closest && e.target.closest('button'); if (!b) return; var g = radioGroup(b); if (!g) return;
    e.stopImmediatePropagation(); pick(b, g);
  }, true);
  document.addEventListener('click', function (e) { // oscillator wave buttons on the AN-X page: pick from the waves in use
    var b = e.target.closest && e.target.closest('button'); if (!b || PAGE !== 'ANX' || !/^(Saw2|Triangle)$/.test(T(b))) return;
    e.stopImmediatePropagation(); if (menu && menuFor === b) { closeMenu(); return; }
    openMenu(b, ['Saw2', 'Triangle'], T(b), function (v) { b.textContent = v; });
  }, true);


  document.addEventListener('click', function (e) { // Utility (crosshair) menu: MIDI housekeeping
    var b = e.target.closest && e.target.closest('button[aria-label="Utility"]'); if (!b) return;
    e.stopImmediatePropagation(); if (menu && menuFor === b) { closeMenu(); return; }
    openMenu(b, ['All notes off', 'Reset all controllers', 'Send all values to the device', 'Read all values from the device'], '', function (v) {
      if (/^Read/.test(v)) modal('Read from the MODX M', ['Read Performance name and common settings', 'Read 16 part settings', 'Read effects and Master EQ'], 'All values read.');
      else toast(v + ': sent to the MODX M.');
    });
  }, true);


  // ---------- span controls: scenes, part tabs, in-page tabs, mute/solo, tempo box ----------
  function strip(spans) { // pick-one strip made of spans: the selected one lends its look to the clicked one
    var sel = spans.filter(function (x) { return getComputedStyle(x).backgroundColor !== getComputedStyle(spans[spans.length - 1 === spans.indexOf(x) ? 0 : spans.length - 1]).backgroundColor; });
    return sel[0] || spans[0];
  }
  var tabStrips = [];
  function registerStrip(spans, onpick) {
    // the selected item is the one whose background differs from the majority
    var count = {}; spans.forEach(function (x) { var c = getComputedStyle(x).backgroundColor; count[c] = (count[c] || 0) + 1; });
    var sel = spans.filter(function (x) { return count[getComputedStyle(x).backgroundColor] === 1; })[0] || spans[0];
    var offOf = spans.filter(function (x) { return x !== sel; })[0];
    var onStyle = sel.getAttribute('style'), offStyle = offOf ? offOf.getAttribute('style') : onStyle;
    spans.forEach(function (x) { x.dataset.strip = '1'; x.style.cursor = 'pointer'; x.setAttribute('role', 'tab'); x.setAttribute('aria-selected', x === sel ? 'true' : 'false'); x._on = onStyle; x._off = offStyle; });
    function choose(el) { spans.forEach(function (x) { var on = x === el; var keep = x.style.cssText; x.setAttribute('style', on ? onStyle.replace(/(height:[^;]*;)/, '$1') : offStyle); x.style.cursor = 'pointer'; x.setAttribute('aria-selected', on ? 'true' : 'false'); }); if (onpick) onpick(el); }
    spans.forEach(function (x) { x.addEventListener('click', function (e) { e.stopPropagation(); choose(x); }); });
    return choose;
  }
  var partState = {}; // mute/solo by tab and row
  function initStrips() {
    // scenes
    var scenes = [].slice.call(document.querySelectorAll('span')).filter(function (x) { return /^Scene[1-8]$/.test(T(x)) && x.children.length === 0; });
    if (scenes.length > 1) registerStrip(scenes, function (el) { toast(T(el) + ' recalled on the MODX M.'); });
    // part tabs 1-8 / 9-16 / Audio
    var tabs = [].slice.call(document.querySelectorAll('span')).filter(function (x) { return /^(1-8|9-16|Audio)$/.test(T(x)) && x.children.length === 0; });
    if (tabs.length === 3) {
      var sec = tabs[0].closest('section'), rows = sec ? [].slice.call(sec.querySelectorAll('div[style*="height: 24px"]')).filter(function (r) { return /grid/.test(r.getAttribute('style')); }) : [];
      var orig = rows.map(function (r) { return r.innerHTML; });
      function muteSolo(r, key) {
        [].forEach.call(r.querySelectorAll('span'), function (x) { var t = T(x); if ((t === 'Mute' || t === 'Solo') && x.children.length === 0) { var on = partState[key + t]; paint(x, t, on); } });
      }
      function paint(x, t, on) { x.dataset.on = on ? '1' : '0'; x.style.background = on ? (t === 'Mute' ? '#C0602A' : '#E9C46A') : '#22252A'; x.style.color = on ? '#071014' : '#8E949D'; x.style.cursor = 'pointer'; x.setAttribute('role', 'button'); x.setAttribute('aria-pressed', on ? 'true' : 'false'); }
      function render(mode) {
        rows.forEach(function (r, i) {
          r.style.display = 'grid'; r.innerHTML = orig[i]; r.style.opacity = '';
          var cells = r.children, key = mode + ':' + i + ':';
          if (mode === '9-16') {
            cells[0].textContent = String(9 + i); cells[1].style.background = 'transparent'; cells[1].textContent = ''; cells[3].textContent = ''; cells[4].textContent = 'Empty'; cells[4].style.color = '#6E747D'; cells[5].textContent = ''; cells[6].style.visibility = 'hidden';
            [].forEach.call(cells[2].querySelectorAll('span'), function (x) { x.style.opacity = '.35'; x.style.pointerEvents = 'none'; });
          } else if (mode === 'Audio') {
            if (i > 1) { r.style.display = 'none'; return; }
            cells[0].textContent = i ? 'USB' : 'A/D'; cells[0].style.fontSize = '9px'; cells[1].style.background = '#2A2D33'; cells[1].style.color = '#9EA3AA'; cells[1].textContent = 'IN'; cells[3].textContent = 'Aud'; cells[4].textContent = i ? 'Audio In (USB)' : 'Audio In (A/D)'; cells[4].removeAttribute && (cells[4].outerHTML = '<span style="font-size: 11px">' + (i ? 'Audio In (USB)' : 'Audio In (A/D)') + '</span>');
          }
          muteSolo(r, key);
        });
        var note = sec.querySelector('[data-parts-note]'); void note;
      }
      registerStrip(tabs, function (el) { render(T(el)); });
      rows.forEach(function (r) { r.addEventListener('click', function (e) { var x = e.target.closest('span'); var t = x && T(x); if (x && (t === 'Mute' || t === 'Solo') && x.children.length === 0) {
        e.preventDefault(); e.stopPropagation(); var mode = (tabs.filter(function (q) { return q.getAttribute('aria-selected') === 'true'; })[0] || tabs[0]).textContent.trim(); var key = mode + ':' + rows.indexOf(r) + ':' + t; partState[key] = !partState[key]; paint(x, t, partState[key]); toast(t + ' ' + (partState[key] ? 'on' : 'off') + ', part ' + (rows.indexOf(r) + 1) + '.'); } }); });
      render('1-8');
    }
    // in-page tab strips on part pages (Pitch | Porta | LFO, Modifier | Filter | Amplitude, Type1 | Type2 ...)
    document.querySelectorAll('div[style*="gap: 2px"]').forEach(function (d) {
      var kids = [].slice.call(d.children).filter(function (x) { return x.tagName === 'SPAN' && x.children.length === 0 && /height: 20px/.test(x.getAttribute('style') || '') && !x.dataset.strip && !/« ◂/.test(T(x)); });
      if (kids.length >= 2 && kids.length === [].filter.call(d.children, function (x) { return !/« ◂/.test(T(x)); }).length) registerStrip(kids);
    });
  }
  // DAW tempo box
  function initTempo() {
    var box = [].slice.call(document.querySelectorAll('span,div')).filter(function (x) { return /^♩ \d+$/.test(T(x)) && x.children.length === 0; })[0]; if (!box) return;
    var bpm = parseInt(T(box).slice(2), 10); function render() { box.textContent = '♩ ' + bpm; }
    box.style.cursor = 'ns-resize'; box.style.touchAction = 'none'; box.title = 'DAW tempo: drag, wheel, double click for 128';
    box.addEventListener('pointerdown', function (e) { e.preventDefault(); box.setPointerCapture(e.pointerId); var y0 = e.clientY, b0 = bpm; function mv(m) { bpm = Math.max(20, Math.min(300, b0 + Math.round((y0 - m.clientY) / 3))); render(); } function up() { box.removeEventListener('pointermove', mv); box.removeEventListener('pointerup', up); } box.addEventListener('pointermove', mv); box.addEventListener('pointerup', up); });
    box.addEventListener('wheel', function (e) { e.preventDefault(); bpm = Math.max(20, Math.min(300, bpm - Math.sign(e.deltaY))); render(); }, { passive: false });
    box.addEventListener('dblclick', function () { bpm = 128; render(); });
  }

  function initChips() { document.querySelectorAll('button').forEach(function (b) { if (isChip(b) && b.dataset.chip === undefined) setChip(b, false); }); }

  function init() {
    initStrips(); initTempo(); initPicks(); initChips(); initPills(); initToggleBtns(); initPan(); initVolume(); initEq(); focusHash();
  }
  function start() { setTimeout(init, 1800); }  // after support.js has finished its own set-up
  window.addEventListener('hashchange', focusHash);
  if (document.readyState !== 'loading') start(); else document.addEventListener('DOMContentLoaded', start);
})();
