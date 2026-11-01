// Reads one mockup page (docs/mockups/MODX2_*.dc.html) with Playwright and writes every box, text, svg, knob and bar
// with its exact position, so the JUCE window can be generated from the mockup (mockup-is-the-plugin).
// usage: node tools/extract_mockup.js docs/mockups/MODX2_Home.dc.html out.json [shot.png]
const { chromium } = require('playwright');
const path = require('path');
(async () => {
  const [, , file, out, shot] = process.argv;
  const b = await chromium.launch(); const p = await b.newPage({ viewport: { width: 1700, height: 720 } });
  await p.goto('file://' + path.resolve(file)); await p.waitForTimeout(1500);
  if (shot) await p.screenshot({ path: shot, fullPage: true });
  const res = await p.evaluate(() => {
    const root = [...document.querySelectorAll('div')].find(d => d.style.width === '1520px' && d.style.height === '641px');
    const o = root.getBoundingClientRect(); const L = { W: 1520, H: 641, rects: [], texts: [], svgs: [], knobs: [], bars: [], links: [], ops: [] };
    const op = (k, a) => L.ops.push([k, a.length - 1]);
    const rgba = s => { const m = s.match(/[\d.]+/g).map(Number); return [m[0], m[1], m[2], m.length > 3 ? m[3] : 1]; };
    const R = e => { const r = e.getBoundingClientRect(); return { x: +(r.x - o.x).toFixed(1), y: +(r.y - o.y).toFixed(1), w: +r.width.toFixed(1), h: +r.height.toFixed(1) }; };
    const walk = e => {
      const cs = getComputedStyle(e); const tag = e.tagName.toLowerCase();
      if (tag === 'svg') {
        const sib = [...(e.parentElement ? e.parentElement.children : [])].filter(c => c.tagName === 'SPAN').length;
        if (e.hasAttribute('data-aknob') || (sib >= 2 && e.getBoundingClientRect().width >= 20 && e.getBoundingClientRect().width <= 32 && e.children.length <= 4 && e.querySelector('circle') && e.querySelector('line'))) {
          const line = e.querySelector('line'), circ = e.querySelectorAll('circle'), arc = e.querySelector('path');
          const par = e.parentElement; const sp = [...par.children].filter(c => c.tagName === 'SPAN');
          const lab = sp[0] ? sp[0].textContent.trim() : '', valEl = sp.length > 1 ? sp[sp.length - 1] : null;
          const a = line ? Math.atan2(+line.getAttribute('x2') - +line.getAttribute('x1'), -(+line.getAttribute('y2') - +line.getAttribute('y1'))) : 0;
          const big = +e.getAttribute('width') > 40;
          L.knobs.push({ ...R(e), label: lab, valText: valEl ? valEl.textContent.trim() : '', valRect: valEl ? R(valEl) : null, valColor: valEl ? getComputedStyle(valEl).color : '',
            frac: +(Math.min(1, Math.max(0, (a * 180 / Math.PI + 135) / 270))).toFixed(3), track: circ[0] ? circ[0].getAttribute('stroke') : '', body: circ[0] ? circ[0].getAttribute('fill') : '', arcCol: arc ? arc.getAttribute('stroke') : '',
            hub: circ[1] ? circ[1].getAttribute('fill') : '', needle: line ? line.getAttribute('stroke') : '', r: circ[0] ? +circ[0].getAttribute('r') : 8, sw: circ[0] ? +circ[0].getAttribute('stroke-width') : 3, big,
            labRect: sp[0] ? R(sp[0]) : null, labColor: sp[0] ? getComputedStyle(sp[0]).color : '', labSize: sp[0] ? parseFloat(getComputedStyle(sp[0]).fontSize) : 10 });
          op('knob', L.knobs); if (valEl) valEl.setAttribute('data-skip', '1');
          return;
        }
        const clone = e.cloneNode(true); clone.querySelectorAll('text').forEach(t => t.remove());
        const svgAt = L.ops.length; L.ops.push(null);
        e.querySelectorAll('text').forEach(t => { const r = R(t); L.texts.push({ ...r, s: t.textContent.trim(), fs: parseFloat(t.getAttribute('font-size') || 8), fw: 400, c: t.getAttribute('fill') || '#8E949D', ls: 0, tt: 'none', ff: 'rc' }); op('text', L.texts); });
        const r = R(e); clone.setAttribute('width', r.w); clone.setAttribute('height', r.h);
        L.svgs.push({ ...r, svg: new XMLSerializer().serializeToString(clone) }); L.ops[svgAt] = ['svg', L.svgs.length - 1]; return;
      }
      if (e.hasAttribute('data-bar')) { const f = e.firstElementChild; L.bars.push({ ...R(e), frac: f ? parseFloat(f.style.width) / 100 : 0, fill: f ? (getComputedStyle(f).backgroundColor !== 'rgba(0, 0, 0, 0)' ? getComputedStyle(f).backgroundColor : '#DDE1E6') : '#DDE1E6', bg: cs.backgroundColor, rad: parseFloat(cs.borderTopLeftRadius) }); op('bar', L.bars); return; }
      const r = R(e);
      if (e.getAttribute('data-skip')) return;
      const bg = rgba(cs.backgroundColor); let fill = bg;
      if (bg[3] === 0 && cs.backgroundImage.includes('rgb')) fill = rgba(cs.backgroundImage.match(/rgba?\([^)]*\)/)[0]);
      const bw = ['Top', 'Right', 'Bottom', 'Left'].map(s => parseFloat(cs['border' + s + 'Width'])); const bside = ['Top', 'Right', 'Bottom', 'Left'].find((s, i) => bw[i] > 0) || 'Top'; const bc = rgba(cs['border' + bside + 'Color']); const bcs = ['Top', 'Right', 'Bottom', 'Left'].map(s => rgba(cs['border' + s + 'Color']));
      const anyB = bw.some(w => w > 0) && bc[3] > 0;
      if ((fill[3] > 0 || anyB) && r.w > 0 && r.h > 0 && tag !== 'a' || ((fill[3] > 0 || anyB) && tag === 'a')) {
        L.rects.push({ ...r, bg: fill, bw, bc, bcs, rad: [cs.borderTopLeftRadius, cs.borderTopRightRadius, cs.borderBottomRightRadius, cs.borderBottomLeftRadius].map(parseFloat), tag }); op('rect', L.rects); }
      if (tag === 'a' && e.getAttribute('href')) L.links.push({ ...r, href: e.getAttribute('href'), txt: e.textContent.trim().slice(0, 30) });
      for (const n of e.childNodes) {
        if (n.nodeType === 3 && n.textContent.trim()) {
          const rg = document.createRange(); rg.selectNodeContents(n); const rects = [...rg.getClientRects()];
          const emit = (txt, rr) => { L.texts.push({ x: +(rr.x - o.x).toFixed(1), y: +(rr.y - o.y).toFixed(1), w: +rr.width.toFixed(1), h: +rr.height.toFixed(1), s: txt, fs: parseFloat(cs.fontSize), fw: +cs.fontWeight, c: cs.color, ls: parseFloat(cs.letterSpacing) || 0, tt: cs.textTransform, ff: cs.fontFamily.includes('Roboto') ? 'rc' : 'sys' }); op('text', L.texts); };
          if (rects.length <= 1) emit(n.textContent.replace(/\s+/g, ' ').trim(), rects[0] || rg.getBoundingClientRect());
          else { // wrapped text: group words by line
            const words = n.textContent.split(/(\s+)/); let pos = 0; const lines = [];
            for (const w of words) { if (w.trim()) { const wr = document.createRange(); wr.setStart(n, pos); wr.setEnd(n, pos + w.length); const q = wr.getClientRects()[0]; if (q) { let ln = lines.find(l => Math.abs(l.y - q.y) < 3); if (!ln) { ln = { y: q.y, x: q.x, h: q.height, right: q.right, s: [] }; lines.push(ln); } ln.s.push(w); ln.right = q.right; ln.x = Math.min(ln.x, q.x); } } pos += w.length; }
            lines.forEach(l => emit(l.s.join(' '), { x: l.x, y: l.y, width: l.right - l.x, height: l.h }));
          }
        }
      }
      for (const c of e.children) walk(c);
    };
    walk(root); return L;
  });
  require('fs').writeFileSync(out, JSON.stringify(res));
  console.log(file, 'rects', res.rects.length, 'texts', res.texts.length, 'svgs', res.svgs.length, 'knobs', res.knobs.length, 'bars', res.bars.length);
  await b.close();
})();
