// Screenshots every interactive mockup page of a repo into docs/tour/gallery_N.png and writes docs/tour/gallery.json
const fs = require('fs'), path = require('path');
const { chromium } = require('/home/claude/.npm-global/lib/node_modules/playwright');
(async () => {
  const files = process.argv.slice(2);
  fs.mkdirSync('docs/tour', { recursive: true });
  const b = await chromium.launch({ executablePath: '/opt/pw-browsers/chromium' });
  const out = [];
  let n = 0;
  for (const f of files) {
    const p = await b.newPage({ viewport: { width: 1600, height: 1000 } });
    await p.goto('file://' + path.resolve(f));
    await p.waitForTimeout(1500);
    const title = await p.title();
    await p.addStyleTag({ content: 'a[href]{visibility:hidden!important}' });
    const box = await p.evaluate(() => {
      const x = document.querySelector('x-dc'); if (!x) return null;
      let best = null, area = 0;
      for (const c of x.querySelectorAll('div')) { const r = c.getBoundingClientRect(); if (r.width * r.height > area) { area = r.width * r.height; best = r; } }
      return best && { x: best.x, y: best.y, width: best.width, height: best.height };
    });
    if (!box) { console.log('skip', f); await p.close(); continue; }
    n++;
    const img = `gallery_${n}.png`;
    await p.screenshot({ path: path.join('docs/tour', img), clip: box });
    out.push({ title, img, file: path.basename(f), w: Math.round(box.width), h: Math.round(box.height) });
    await p.close();
  }
  fs.writeFileSync('docs/tour/gallery.json', JSON.stringify(out, null, 1));
  console.log(JSON.stringify(out.map(o => [o.title, o.w, o.h])));
  await b.close();
})();
