// Offline shell: the console is static, so cache it and serve it network-first.
const CACHE = 'car-alarm-console-v1';
const FILES = ['./', 'index.html', 'ml.js', 'manifest.webmanifest', 'icon-180.png', 'icon-192.png', 'icon-512.png'];

self.addEventListener('install', (e) => {
  e.waitUntil(caches.open(CACHE).then((c) => c.addAll(FILES)).then(() => self.skipWaiting()));
});
self.addEventListener('activate', (e) => {
  e.waitUntil(caches.keys().then((keys) => Promise.all(keys.filter((k) => k !== CACHE).map((k) => caches.delete(k)))).then(() => self.clients.claim()));
});
self.addEventListener('fetch', (e) => {
  if (e.request.method !== 'GET' || new URL(e.request.url).origin !== location.origin) return;
  e.respondWith(
    fetch(e.request).then((r) => { const copy = r.clone(); caches.open(CACHE).then((c) => c.put(e.request, copy)); return r; })
      .catch(() => caches.match(e.request).then((r) => r || caches.match('index.html')))
  );
});
