/* Web Audio for the six WAVE resources (124-129). Decoded lazily on the
 * first user gesture so the AudioContext satisfies the autoplay policy. */

"use strict";

const SpiderAudio = (() => {
    let actx = null;
    const buffers = new Map();
    let unlocked = false;

    function ensure() {
        if (actx === null) {
            const AC = window.AudioContext || window.webkitAudioContext;
            if (!AC) {
                return null;
            }
            actx = new AC();
        }
        return actx;
    }

    async function load(id) {
        if (buffers.has(id)) {
            return buffers.get(id);
        }
        try {
            const res = await fetch("assets/sounds/" + id + ".wav");
            const raw = await res.arrayBuffer();
            const buf = await actx.decodeAudioData(raw);
            buffers.set(id, buf);
            return buf;
        } catch (e) {
            console.warn("audio: " + id, e);
            buffers.set(id, null);
            return null;
        }
    }

    function unlock() {
        if (unlocked || !ensure()) {
            return;
        }
        unlocked = true;
        if (actx.state === "suspended") {
            actx.resume();
        }
        for (let id = 124; id <= 129; id++) {
            load(id);
        }
    }

    function play(id) {
        if (!ensure()) {
            return;
        }
        const cached = buffers.get(id);
        if (cached === null) {
            return;
        }
        const start = (buf) => {
            const src = actx.createBufferSource();
            src.buffer = buf;
            src.connect(actx.destination);
            src.start();
        };
        if (cached) {
            start(cached);
        } else {
            load(id).then((buf) => {
                if (buf) {
                    start(buf);
                }
            });
        }
    }

    window.addEventListener("pointerdown", unlock, { capture: true });
    window.addEventListener("keydown", unlock, { capture: true });

    return { play };
})();
