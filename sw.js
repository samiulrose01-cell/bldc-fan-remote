const C="fan-remote-v1";const HOME_IP = "192.168.10.141";

function openHomeRemote(){
  window.location.href = "http://" + HOME_IP + "/";
}sself.addEventListener("install",e=>self.skipWaiting());self.addEventListener("activate",e=>e.waitUntil(self.clients.claim()));self.addEventListener("fetch",e=>{if(e.request.method==="GET")e.respondWith(fetch(e.request).catch(()=>caches.match(e.request)))})
