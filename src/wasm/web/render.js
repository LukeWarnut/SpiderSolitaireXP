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

    function drawCard(ctx, sprite) {
        const c = cardCanvas(sprite.code);
        if (c) {
            ctx.drawImage(c, sprite.x, sprite.y);
        }
    }

    function drawSprites(ctx, list) {
        for (let i = 0; i < list.size(); i++) {
            drawCard(ctx, list.get(i));
        }
    }

    function drawScore(ctx, frame) {
        const x = frame.score_x;
        const y = frame.score_y;
        const w = frame.score_w;
        const h = frame.score_h;
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
            ["Score:", String(frame.score), y + 0x1e],
            ["Moves:", String(frame.moves), y + 0x32],
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
            const pat = ctx.createPattern(feltPattern, "repeat");
            ctx.fillStyle = pat;
            ctx.fillRect(0, 0, w, h);
        } else {
            ctx.fillStyle = "#008000";
            ctx.fillRect(0, 0, w, h);
        }
    }

    function draw(ctx, frame, w, h) {
        felt(ctx, w, h);

        drawSprites(ctx, frame.board);
        if (frame.show_score) {
            drawScore(ctx, frame);
        }
        drawSprites(ctx, frame.front);

        /* blink_move: InvertRect over what is already drawn. */
        if (frame.hint_on) {
            ctx.save();
            ctx.globalCompositeOperation = "difference";
            ctx.fillStyle = "#fff";
            ctx.fillRect(frame.hint_x, frame.hint_y, frame.hint_w, frame.hint_h);
            ctx.restore();
        }

        /* AnimState::paint: the text (prep_blit) under the particles. */
        if (frame.win_text) {
            const r = Math.round(frame.win_r * 255);
            const g = Math.round(frame.win_g * 255);
            const b = Math.round(frame.win_b * 255);
            ctx.font = "800 64px Arial, sans-serif";
            ctx.textAlign = "center";
            ctx.textBaseline = "middle";
            ctx.fillStyle = `rgb(${r},${g},${b})`;
            ctx.fillText("You Won!", w / 2, h / 2);
        }
        for (let i = 0; i < frame.fx.size(); i++) {
            const p = frame.fx.get(i);
            const r = Math.round(p.r * 255);
            const g = Math.round(p.g * 255);
            const b = Math.round(p.b * 255);
            ctx.beginPath();
            ctx.arc(p.x, p.y, p.rad, 0, Math.PI * 2);
            ctx.fillStyle = `rgb(${r},${g},${b})`;
            ctx.fill();
            /* The DC's default 1px black pen. */
            ctx.lineWidth = 1;
            ctx.strokeStyle = "#000";
            ctx.stroke();
        }
        ctx.textAlign = "start";
    }

    return { init, felt, draw, CARD_W, CARD_H };
})();
