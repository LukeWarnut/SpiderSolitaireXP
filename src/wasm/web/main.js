/* Boot: start the wasm module, fetch card bitmaps over HTTP, mount IDBFS,
 * then run the game. Modal Host calls use ASYNCIFY, so the rAF loop pauses
 * while a dialog is up (re-entering wasm during a sleep corrupts it). */

"use strict";

(async () => {
    const canvas = document.getElementById("board");
    const ctx = canvas.getContext("2d");

    let cw = 1024;
    let ch = 720;
    let looping = false;
    let wasmBusy = false;
    let Module = null;

    function fail(err) {
        console.error(err);
        const root = document.getElementById("overlay-root");
        const p = document.createElement("div");
        p.className = "dialog-backdrop";
        p.style.zIndex = "9999";
        p.innerHTML = "<div class=\"dialog\"><div class=\"dialog-title\">Spider</div>" +
            "<div class=\"dialog-body\"></div></div>";
        p.querySelector(".dialog-body").textContent = String(err && err.stack ? err.stack : err);
        root.appendChild(p);
    }

    try {
        Module = await SpiderModule();
        SpiderUI.init(Module);

        const names = [];
        for (let i = 1; i <= 52; i++) {
            names.push("CARD" + i);
        }
        names.push("CARDBACK", "108", "FELT");
        for (const name of names) {
            const res = await fetch("assets/bitmaps/" + name + ".bmp");
            if (!res.ok) {
                throw new Error("cannot load assets/bitmaps/" + name + ".bmp (" + res.status + ")");
            }
            const buf = new Uint8Array(await res.arrayBuffer());
            if (!Module.addBitmap(name, buf)) {
                throw new Error("cannot decode " + name + ".bmp");
            }
        }

        const FS = Module.FS;
        try {
            FS.mkdir("/spider");
        } catch (e) {
            /* already exists after a hot reload */
        }
        FS.mount(FS.filesystems.IDBFS, {}, "/spider");
        await new Promise((resolve, reject) => {
            FS.syncfs(true, (err) => (err ? reject(err) : resolve()));
        });

        SpiderRender.init(Module);

        function applyTransform() {
            const dpr = window.devicePixelRatio || 1;
            ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
        }

        function paintFelt() {
            applyTransform();
            SpiderRender.felt(ctx, cw, ch);
        }

        function resize() {
            const rect = canvas.getBoundingClientRect();
            const dpr = window.devicePixelRatio || 1;
            canvas.width = Math.max(1, Math.round(rect.width * dpr));
            canvas.height = Math.max(1, Math.round(rect.height * dpr));
            cw = rect.width;
            ch = rect.height;
            /* layout() re-enters wasm; skip it while ASYNCIFY is waiting on a dialog. */
            if (!wasmBusy) {
                Module.layout(cw, ch);
            }
            if (!looping) {
                paintFelt();
            }
        }
        window.addEventListener("resize", resize);
        resize();

        function paint() {
            applyTransform();
            Module.tick();
            SpiderRender.draw(ctx, Module.frame(), cw, ch);
        }

        function loop() {
            if (!looping) {
                return;
            }
            try {
                paint();
            } catch (e) {
                looping = false;
                fail(e);
                return;
            }
            requestAnimationFrame(loop);
        }

        function startLoop() {
            if (!looping) {
                looping = true;
                requestAnimationFrame(loop);
            }
        }

        async function runCommand(id) {
            looping = false;
            wasmBusy = true;
            try {
                const result = Module.postCommand(id);
                if (result && typeof result.then === "function") {
                    await result;
                }
            } finally {
                wasmBusy = false;
                startLoop();
            }
        }

        window.SpiderApp = {
            command(name) {
                return runCommand(Module.CommandId[name].value);
            },
        };

        /* Logical (CSS pixel) coordinates, matching the macOS port's client
         * points; the canvas is scaled by devicePixelRatio like Retina backing. */
        function point(e) {
            const rect = canvas.getBoundingClientRect();
            return [e.clientX - rect.left, e.clientY - rect.top];
        }

        canvas.addEventListener("contextmenu", (e) => e.preventDefault());
        canvas.addEventListener("pointerdown", (e) => {
            const [x, y] = point(e);
            if (e.button === 0) {
                canvas.setPointerCapture(e.pointerId);
                Module.mouseDown(1, x, y);
            } else if (e.button === 2) {
                Module.mouseDown(3, x, y);
            }
            e.preventDefault();
        });
        canvas.addEventListener("pointermove", (e) => {
            const [x, y] = point(e);
            Module.mouseMove(x, y, (e.buttons & 1) !== 0);
        });
        canvas.addEventListener("pointerup", (e) => {
            const [x, y] = point(e);
            if (e.button === 0) {
                Module.mouseUp(1, x, y);
            } else if (e.button === 2) {
                Module.mouseUp(3, x, y);
            }
        });

        window.addEventListener("keydown", (e) => {
            if (e.ctrlKey || e.metaKey || e.altKey) {
                if ((e.key === "z" || e.key === "Z") && (e.ctrlKey || e.metaKey)) {
                    runCommand(Module.CommandId.UNDO.value);
                    e.preventDefault();
                } else if ((e.key === "n" || e.key === "N") && (e.ctrlKey || e.metaKey)) {
                    runCommand(Module.CommandId.NEW.value);
                    e.preventDefault();
                } else if ((e.key === "s" || e.key === "S") && (e.ctrlKey || e.metaKey)) {
                    runCommand(Module.CommandId.SAVE.value);
                    e.preventDefault();
                } else if ((e.key === "o" || e.key === "O") && (e.ctrlKey || e.metaKey)) {
                    runCommand(Module.CommandId.OPEN.value);
                    e.preventDefault();
                }
                return;
            }
            const ids = { F2: "NEW", F3: "DIFFICULTY", F4: "STATS", F5: "OPTIONS", F1: "HELP" };
            const name = ids[e.key] || { d: "DEAL", D: "DEAL", m: "HINT", M: "HINT" }[e.key];
            if (name) {
                runCommand(Module.CommandId[name].value);
                e.preventDefault();
            }
        });

        /* Felt is already on the canvas. startup() shows the difficulty
         * dialog; do not tick/frame until it returns (ASYNCIFY). */
        wasmBusy = true;
        try {
            const started = Module.startGame();
            if (started && typeof started.then === "function") {
                await started;
            }
        } finally {
            wasmBusy = false;
        }
        resize();
        startLoop();
    } catch (e) {
        fail(e);
    }
})();
