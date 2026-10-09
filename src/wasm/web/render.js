/* Canvas2D renderer: replicates Renderer::Impl::build from the Metal port.
 * Card art comes from the wasm image table as offscreen canvases; the felt
 * is a 63x64-cell pattern of the FELT bitmap, like draw_felt. */

"use strict";

const SpiderRender = (() => {
    const CARD_W = 0x47;
    const CARD_H = 0x60;
    const CODE_BACK = 0x68;
    const CODE_EMPTY = 0x6c;

    let images = null; /* name -> canvas */
    let feltPattern = null;
    let feltFill = null; /* createPattern is per-context; cache until resize */

    /* Upload the wasm image table once. */
    function init(Module) {
        images = new Map();
        const n = Module.imageCount();
        for (let i = 0; i < n; i++) {
            const name = Module.imageName(i);
            const w = Module.imageWidth(i);
            const h = Module.imageHeight(i);
            const pixels = Module.imagePixels(i);
            const canvas = document.createElement("canvas");
            canvas.width = w;
            canvas.height = h;
            const ctx = canvas.getContext("2d");
            const data = new ImageData(new Uint8ClampedArray(pixels), w, h);
            ctx.putImageData(data, 0, 0);
            images.set(name, canvas);
        }
        const felt = images.get("FELT");
        if (felt) {
            /* draw_felt tiles 63x64 cells of the 64x64 FELT bitmap. */
            const tile = document.createElement("canvas");
            tile.width = 63;
            tile.height = 64;
            tile.getContext("2d").drawImage(felt, 0, 0);
            feltPattern = tile;
        }
    }

    function cardCanvas(code) {
        if (code >= 1 && code <= 52) {
            return images.get("CARD" + code);
        }
        if (code === CODE_BACK) {
            return images.get("CARDBACK");
        }
        if (code === CODE_EMPTY) {
            return images.get("108");
        }
        return null;
    }

    function drawCardXY(ctx, code, x, y) {
        const c = cardCanvas(code);
        if (c) {
            ctx.drawImage(c, x, y);
        }
    }

    function drawScore(ctx, x, y, w, h, score, moves) {
        /* paint_board: RGB(0,127,0) fill, black frame, white labels. */
        ctx.fillStyle = "rgb(0,127,0)";
        ctx.fillRect(x, y, w, h);
        ctx.strokeStyle = "#000";
        ctx.lineWidth = 1;
        ctx.strokeRect(x + 0.5, y + 0.5, w - 1, h - 1);
        ctx.fillStyle = "#fff";
        ctx.font = "bold 11px Tahoma, sans-serif";
        ctx.textBaseline = "top";
        const rows = [
            ["Score:", String(score), y + 0x1e],
            ["Moves:", String(moves), y + 0x32],
        ];
        for (const [label, value, top] of rows) {
            ctx.textAlign = "right";
            ctx.fillText(label, x + 100, top);
            ctx.textAlign = "left";
            ctx.fillText(value, x + 0x6e, top);
        }
    }

    function felt(ctx, w, h) {
        if (feltPattern) {
            if (!feltFill) {
                feltFill = ctx.createPattern(feltPattern, "repeat");
            }
            ctx.fillStyle = feltFill;
            ctx.fillRect(0, 0, w, h);
        } else {
            ctx.fillStyle = "#008000";
            ctx.fillRect(0, 0, w, h);
        }
    }

    function invalidate() {
        feltFill = null;
    }

    /* Packed buffer from Module.step(). See host/main.cpp. Paint order
     * matches paint_hdc: felt, columns, score, stock/drag, hint, win, fx. */
    function drawPacked(ctx, data, w, h) {
        felt(ctx, w, h);
        let i = 0;
        const nboard = data[i++];
        for (let n = 0; n < nboard; n++) {
            drawCardXY(ctx, data[i], data[i + 1], data[i + 2]);
            i += 3;
        }
        const nfront = data[i++];
        const frontAt = i;
        i += nfront * 3;
        const show = data[i++];
        const sx = data[i++];
        const sy = data[i++];
        const sw = data[i++];
        const sh = data[i++];
        const score = data[i++];
        const moves = data[i++];
        if (show) {
            drawScore(ctx, sx, sy, sw, sh, score, moves);
        }
        for (let n = 0; n < nfront; n++) {
            const o = frontAt + n * 3;
            drawCardXY(ctx, data[o], data[o + 1], data[o + 2]);
        }
        const hintOn = data[i++];
        const hx = data[i++];
        const hy = data[i++];
        const hw = data[i++];
        const hh = data[i++];
        if (hintOn) {
            ctx.save();
            ctx.globalCompositeOperation = "difference";
            ctx.fillStyle = "#fff";
            ctx.fillRect(hx, hy, hw, hh);
            ctx.restore();
        }
        const winText = data[i++];
        const wr = data[i++];
        const wg = data[i++];
        const wb = data[i++];
        if (winText) {
            ctx.font = "800 64px Arial, sans-serif";
            ctx.textAlign = "center";
            ctx.textBaseline = "middle";
            ctx.fillStyle = `rgb(${Math.round(wr * 255)},${Math.round(wg * 255)},${Math.round(wb * 255)})`;
            ctx.fillText("You Won!", w / 2, h / 2);
        }
        const nfx = data[i++];
        for (let n = 0; n < nfx; n++) {
            const x = data[i++];
            const y = data[i++];
            const rad = data[i++];
            const r = data[i++];
            const g = data[i++];
            const b = data[i++];
            ctx.beginPath();
            ctx.arc(x, y, rad, 0, Math.PI * 2);
            ctx.fillStyle = `rgb(${Math.round(r * 255)},${Math.round(g * 255)},${Math.round(b * 255)})`;
            ctx.fill();
            ctx.lineWidth = 1;
            ctx.strokeStyle = "#000";
            ctx.stroke();
        }
        ctx.textAlign = "start";
    }

    return { init, felt, invalidate, drawPacked, CARD_W, CARD_H };
})();
