# Web build (WebGPU in the browser)

A separate target, `new_renderer_web`, built with the Emscripten toolchain.
The RHI uses the same WebGPU backend as the native build: Dawn's
[emdawnwebgpu](https://dawn.googlesource.com/dawn/+/refs/heads/main/src/emdawnwebgpu/)
port implements `webgpu/webgpu_cpp.h` on top of the browser's WebGPU API, so
the backend sources are shared.

## Build

```bat
scripts\build_web.bat
```

This configures `build/web` with the Emscripten toolchain
(`third_party/emsdk`), compiles the shaders with DXC and Tint (host tools)
and embeds the shaders and the models into the wasm.

The build needs network access the first time: Emscripten downloads the
`emdawnwebgpu` and `contrib.glfw3` ports.

## Deploy

The build stages everything a static host needs in `build/web/dist`:

```
index.html            the page
new_renderer_web.js   the loader
new_renderer_web.wasm the module (52 MB, the models are embedded in it)
```

Upload that folder to any static host. It is a plain static site: no
server-side code, no build step on the host.

### Vercel

```bat
npm i -g vercel
vercel login
vercel deploy --prod build\web\dist
```

Vercel serves the folder as-is, `index.html` becomes the root, and `.wasm`
is served with the `application/wasm` content type that WebGPU requires.

### Cloudflare Pages / Netlify

Both take the folder directly: drag `build/web/dist` onto the dashboard
(Cloudflare: *Workers & Pages -> Create -> Upload assets*, Netlify:
*app.netlify.com/drop*).

```bat
npx netlify deploy --dir=build\web\dist --prod
```

### GitHub Pages

Copy `build/web/dist` into a `gh-pages` branch. The wasm is 52 MB, which is
over GitHub's 50 MB warning (files up to 100 MB are allowed).

### Notes

- WebGPU needs a secure context, so the page has to be served over HTTPS
  (`http://localhost` is the only exception).
- The `.wasm` file must be served as `application/wasm`; all of the hosts
  above do that, a hand written nginx needs
  `types { application/wasm wasm; }`.
- No cross origin isolation headers are needed (no threads, no
  SharedArrayBuffer).
- Every page load downloads the whole 52 MB module because the models are
  embedded in it. If that matters, switch the models to a runtime download.

## Run locally

```bat
npx http-server build\web\dist -p 8080
```

then open <http://localhost:8080/>.

A WebGPU capable browser is required (Chrome/Edge 113+, Safari 18+, Firefox
141+ on Windows). The page shows the frame counter in the top left corner;
WebGPU validation errors are printed to the browser console.

## How it differs from the native build

| | native | web |
|---|---|---|
| surface | OS window handle | HTML canvas (`#canvas` in `web/shell.html`) |
| present | `surface.Present()` | the browser composites the canvas at the end of the frame |
| resolution | the window size | derived from the client (browser window size x device pixel ratio, capped at 4K) and then fixed |
| shaders / models | read from disk | embedded in the wasm, mounted into the in-memory filesystem |
| frame loop | `while (!glfwWindowShouldClose())` | `requestAnimationFrame` (see `newRendererWebFrame` in `src/main.cpp`) |
| adapter/device | requested synchronously | the browser is promise based, so the wait is done with Asyncify |
| free look | cursor is locked and re-centered | the mouse is never locked: the camera follows the mouse while the right button is held |

The render resolution is fixed for the lifetime of the page: the canvas
backing store is the render resolution and the canvas is scaled into the
browser window, so resizing the window changes how the image is displayed,
not the render targets.

`glfwSetCursorPos` cannot move the mouse pointer in a browser, so the
free-look camera measures its delta from the actual pointer position
instead of re-centering it.
