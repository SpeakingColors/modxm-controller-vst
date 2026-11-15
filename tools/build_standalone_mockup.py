#!/usr/bin/env python3
"""Export the MODX M mockup (the ten MODX2_*.dc.html pages) as ONE standalone HTML file.

  python3 tools/build_standalone_mockup.py docs/mockup/index.html

What goes in: the ten pages, the explicit behaviour layer (docs/mockups/interact.js), the generic control code
(arc knobs, faders, toggles, settings and info panels, taken from the marked blocks of docs/mockups/support.js)
and the Roboto Condensed fonts as data. What stays out: the Claude Design runtime (x-dc, helmet) and the
support.js file itself. Nothing is fetched from the network: every page runs inside an iframe built from a string,
the fonts become blob: URLs, and the parent page scales every page by the same factor so the zoom is identical.
"""
import base64, json, re, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MOCK = ROOT / 'docs' / 'mockups'
FONTS = ROOT / 'assets' / 'fonts'
PAGES = ['Home', 'Nav', 'AWM2', 'FMX', 'ANX', 'FX', 'Motion', 'Morph', 'Library', 'Output']


def page_body(name):
    s = (MOCK / f'MODX2_{name}.dc.html').read_text()
    a = s.index('<x-dc>') + len('<x-dc>')
    b = s.index('</x-dc>')
    body = s[a:b]
    body = re.sub(r'<helmet>.*?</helmet>', '', body, flags=re.S)   # fonts and base style are supplied by the shell
    return body.strip()


def runtime():
    s = (MOCK / 'support.js').read_text()

    def cut(a, b):
        i = s.index(a)
        j = s.index(b, i + 1) if b else len(s)
        return s[i:j]
    head = cut('(function(){\nvar INFO', '// Interactive knobs')          # settings and info panels
    head = re.sub(r"var a=document\.createElement\('a'\);a\.href='\.\./index\.html'.*?document\.body\.appendChild\(a\);", '', head, flags=re.S)  # no "All mockups" link
    head = head.replace("location.pathname.split('/').pop()", "(window.MODX_PAGE ? 'MODX2_' + window.MODX_PAGE : '')")
    head = head.replace("document.addEventListener('DOMContentLoaded',function(){", "(function(cb){cb();})(function(){", 1)
    layer = cut('// ===== Interactive layer', '// ===== SVG arc knobs')
    knobs = cut('// ===== SVG arc knobs', '// ===== Switch pills')
    out = '\n'.join([head, layer, knobs])
    # the export has no renderer to wait for: start the generic controls right after the page is built
    out = out.replace('setTimeout(initFaders,1200)', 'setTimeout(initFaders,30)')
    out = out.replace('initArcKnobs();initPoints();},1300)', 'initArcKnobs();initPoints();},40)')
    return out


def main(dest):
    fonts = {w: base64.b64encode((FONTS / f'RobotoCondensed-{w}.ttf').read_bytes()).decode()
             for w in ('Regular', 'Medium', 'Bold')}
    pages = {n: page_body(n) for n in PAGES}
    interact = (MOCK / 'interact.js').read_text()
    rt = runtime()

    def js(o):
        return json.dumps(o, ensure_ascii=False).replace('</', '<\\/').replace('\u2028', '\\u2028').replace('\u2029', '\\u2029')

    html = SHELL
    html = html.replace('/*FONTS*/', js(fonts))
    html = html.replace('/*PAGES*/', js(pages))
    html = html.replace('/*INTERACT*/', js(interact))
    html = html.replace('/*RUNTIME*/', js(rt))
    dest = Path(dest)
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_text(html)
    print(dest, round(len(html) / 1024), 'KB')


SHELL = r"""<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>MODX M Controller (mockup)</title>
<style>
html, body { margin: 0; height: 100%; background: #0B0B0C; overflow: hidden; }
#stage { position: fixed; inset: 0; display: flex; overflow-x: hidden; overflow-y: auto; scrollbar-width: none; }
#stage::-webkit-scrollbar { display: none; }
#box { position: relative; flex: none; margin: auto; }
iframe { position: absolute; left: 0; top: 0; border: 0; background: #0B0B0C; transform-origin: 0 0; display: none; }
</style>
</head>
<body>
<div id="stage"><div id="box"></div></div>
<script>
(function () {
  var FONTS = /*FONTS*/;
  var PAGES = /*PAGES*/;
  var INTERACT = /*INTERACT*/;
  var RUNTIME = /*RUNTIME*/;
  var DESIGN_W = 1697;   // widest page (the engine pages)
  var box = document.getElementById('box'), frames = {}, pending = {}, cur = null, sizes = {}, STORE = {};

  var W = { Regular: 400, Medium: 500, Bold: 700 }, fontCss = '';
  Object.keys(FONTS).forEach(function (k) {
    fontCss += "@font-face{font-family:'Roboto Condensed';font-weight:" + W[k] + ";src:url(data:font/ttf;base64," + FONTS[k] + ") format('truetype')}";
  });
  var baseCss = fontCss + 'html,body{margin:0;background:#0B0B0C}a{color:#E4E6EA}a:hover{color:#FFFFFF}';

  // runs inside every page frame: talks to this shell only through postMessage, so it also works when the file is opened from disk
  function boot(name) {
    window.MODX_PAGE = name; window.MODX_HASH = ''; window.MODX_STORE = __STORE__;
    window.MODX_NAV = function (p, h) { parent.postMessage({ modx: 'go', page: p, hash: h || '' }, '*'); };
    window.addEventListener('message', function (e) {
      var d = e.data; if (!d || !d.modx) return;
      if (d.modx === 'focus') { window.MODX_HASH = d.hash ? encodeURIComponent(d.hash) : ''; if (window.__modxFocus) window.__modxFocus(); }
      if (d.modx === 'store') window.MODX_STORE[d.k] = d.v;
    });
    document.addEventListener('click', function (e) {   // plain links between pages: open the page inside the shell
      var a = e.target.closest && e.target.closest('a[href]'); if (!a) return;
      e.preventDefault(); if (e.defaultPrevented && e.__routed) return;
      var m = /MODX2_(\w+)\.dc\.html(?:#(.*))?/.exec(a.getAttribute('href') || ''); if (m) window.MODX_NAV(m[1], m[2] ? decodeURIComponent(m[2]) : '');
    });
    function report() {
      var el = document.body.firstElementChild; if (!el) return;
      var w = Math.ceil(Math.max(el.getBoundingClientRect().width, el.scrollWidth)), h = Math.ceil(Math.max(document.documentElement.scrollHeight, el.getBoundingClientRect().height));
      if (w && h) parent.postMessage({ modx: 'size', page: name, w: w, h: h }, '*');
    }
    window.addEventListener('load', function () {
      (document.fonts && document.fonts.ready ? document.fonts.ready : Promise.resolve()).then(function () {
        setTimeout(report, 300); setTimeout(report, 900);
        if (window.ResizeObserver) new ResizeObserver(report).observe(document.body);
      });
    });
  }

  function fit() {
    var sz = sizes[cur]; if (!sz) return;
    var s = Math.min(window.innerWidth / DESIGN_W, 2), f = frames[cur];   // one scale for every page, so the zoom never changes between pages
    box.style.width = Math.round(sz.w * s) + 'px'; box.style.height = Math.round(sz.h * s) + 'px';
    f.style.width = sz.w + 'px'; f.style.height = sz.h + 'px'; f.style.transform = 'scale(' + s + ')';
  }
  function build(name) {
    var f = document.createElement('iframe');
    f.setAttribute('title', 'MODX M ' + name);
    var boot_ = '(' + boot.toString().replace('__STORE__', JSON.stringify(STORE)) + ')(' + JSON.stringify(name) + ');';
    f.srcdoc = '<!doctype html><html lang="en"><head><meta charset="utf-8"><style>' + baseCss + '</style>'
      + '<script>' + boot_ + '<' + '/script></head><body>' + PAGES[name]
      + '<script>' + INTERACT + '<' + '/script><script>' + RUNTIME + '<' + '/script></body></html>';
    frames[name] = f; box.appendChild(f);
  }
  function reveal(name, hash) {
    Object.keys(frames).forEach(function (k) { frames[k].style.display = k === name ? 'block' : 'none'; });
    cur = name; document.title = 'MODX M Controller (mockup): ' + name; fit();
    if (hash) frames[name].contentWindow.postMessage({ modx: 'focus', hash: hash }, '*');
  }
  function show(name, hash) {
    if (!PAGES[name]) return;
    if (frames[name] && sizes[name]) { reveal(name, hash); return; }
    pending = { name: name, hash: hash || '' };
    if (!frames[name]) build(name);
    setTimeout(function () { if (pending.name === name) { pending = {}; sizes[name] = sizes[name] || { w: 1588, h: 641 }; reveal(name, hash); } }, 1500);
  }
  window.addEventListener('message', function (e) {
    var d = e.data; if (!d || !d.modx) return;
    if (d.modx === 'go') show(d.page, d.hash);
    else if (d.modx === 'size') {
      sizes[d.page] = { w: d.w, h: d.h };
      if (pending.name === d.page) { var p = pending; pending = {}; reveal(p.name, p.hash); } else if (cur === d.page) fit();
    } else if (d.modx === 'store') {
      STORE[d.k] = d.v;
      Object.keys(frames).forEach(function (k) { try { frames[k].contentWindow.postMessage(d, '*'); } catch (x) { } });
    }
  });
  window.addEventListener('resize', fit);
  var start = (location.hash || '').replace('#', '');
  show(PAGES[start] ? start : 'Home', '');
})();
</script>
</body>
</html>
"""


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else str(ROOT / 'docs' / 'mockup' / 'index.html'))
