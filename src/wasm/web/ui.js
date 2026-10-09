/* HTML menus and dialogs behind the wasm Host. The modal dialogs return
 * Promises; wasm_host.cpp awaits them under ASYNCIFY, so the game unwinds
 * into the browser event loop while a dialog is up. */

"use strict";

const SpiderUI = (() => {
    const ANSWER = { OK: 1, CANCEL: 2, YES: 6, NO: 7 };

    const MENU_DEF = [
        ["Game", [
            { id: "new", label: "New Game", accel: "F2", cmd: "NEW" },
            { id: "restart", label: "Restart This Game", cmd: "RESTART" },
            "sep",
            { id: "undo", label: "Undo", accel: "Ctrl+Z", cmd: "UNDO" },
            { id: "deal", label: "Deal Next Row", accel: "D", cmd: "DEAL" },
            { id: "hint", label: "Show an Available Move", accel: "M", cmd: "HINT" },
            "sep",
            { label: "Difficulty…", accel: "F3", cmd: "DIFFICULTY" },
            { label: "Statistics…", accel: "F4", cmd: "STATS" },
            { label: "Options…", accel: "F5", cmd: "OPTIONS" },
            "sep",
            { id: "save", label: "Save This Game", cmd: "SAVE" },
            { id: "open", label: "Open Last Saved Game", cmd: "OPEN" },
        ]],
        ["Help", [
            { label: "Spider Help", accel: "F1", cmd: "HELP" },
            { label: "About Spider", cmd: "ABOUT" },
        ]],
    ];

    let module = null;
    const menuItems = new Map();
    let openDrop = null;

    function sendCommand(name) {
        closeMenus();
        if (window.SpiderApp && window.SpiderApp.command) {
            return window.SpiderApp.command(name);
        }
        if (module) {
            return module.postCommand(module.CommandId[name].value);
        }
    }

    function closeMenus() {
        if (openDrop) {
            openDrop.classList.remove("show");
            document.querySelectorAll(".menu-top.open").forEach((b) => b.classList.remove("open"));
            openDrop = null;
        }
    }

    function buildMenu() {
        const bar = document.getElementById("menubar");
        bar.textContent = "";
        for (const [title, entries] of MENU_DEF) {
            const top = document.createElement("button");
            top.className = "menu-top";
            top.textContent = title;
            const drop = document.createElement("div");
            drop.className = "menu-drop";
            for (const entry of entries) {
                if (entry === "sep") {
                    const sep = document.createElement("div");
                    sep.className = "menu-sep";
                    drop.appendChild(sep);
                    continue;
                }
                const item = document.createElement("button");
                item.className = "menu-item";
                const label = document.createElement("span");
                label.textContent = entry.label;
                item.appendChild(label);
                if (entry.accel) {
                    const accel = document.createElement("span");
                    accel.className = "accel";
                    accel.textContent = entry.accel;
                    item.appendChild(accel);
                }
                item.addEventListener("click", () => sendCommand(entry.cmd));
                drop.appendChild(item);
                if (entry.id) {
                    menuItems.set(entry.id, item);
                }
            }
            top.addEventListener("click", (e) => {
                e.stopPropagation();
                const was = drop.classList.contains("show");
                closeMenus();
                if (!was) {
                    drop.classList.add("show");
                    top.classList.add("open");
                    openDrop = drop;
                }
            });
            bar.appendChild(top);
            bar.appendChild(drop);
        }
        document.addEventListener("click", (e) => {
            if (!e.target.closest("#menubar")) {
                closeMenus();
            }
        });
    }

    /* MenuState from the game: which entries are clickable. */
    function syncMenu(state) {
        for (const [id, enabled] of Object.entries(state)) {
            const item = menuItems.get(id);
            if (item) {
                item.disabled = !enabled;
            }
        }
    }

    /* ---- dialogs ------------------------------------------------------ */

    function dialog(title, bodyNode, buttons) {
        return new Promise((resolve) => {
            const root = document.getElementById("overlay-root");
            const backdrop = document.createElement("div");
            backdrop.className = "dialog-backdrop";
            const box = document.createElement("div");
            box.className = "dialog";
            const head = document.createElement("div");
            head.className = "dialog-title";
            head.textContent = title;
            const body = document.createElement("div");
            body.className = "dialog-body";
            if (typeof bodyNode === "string") {
                body.textContent = bodyNode;
            } else {
                body.appendChild(bodyNode);
            }
            const row = document.createElement("div");
            row.className = "dialog-buttons";
            const done = (value) => {
                backdrop.remove();
                resolve(value);
            };
            for (const [label, value, isDefault] of buttons) {
                const b = document.createElement("button");
                b.textContent = label;
                b.addEventListener("click", () => done(value));
                row.appendChild(b);
                if (isDefault) {
                    setTimeout(() => b.focus(), 0);
                }
            }
            box.appendChild(head);
            box.appendChild(body);
            box.appendChild(row);
            backdrop.appendChild(box);
            root.appendChild(backdrop);
        });
    }

    /* GameWin confirms: OK, Yes/No, or Yes/No/Cancel. */
    function confirm(text, kind) {
        if (kind === 0) {
            return dialog("Spider", text, [["OK", ANSWER.OK, true]]);
        }
        const buttons = [
            ["Yes", ANSWER.YES, true],
            ["No", ANSWER.NO, kind !== 1],
        ];
        if (kind === 2) {
            buttons.push(["Cancel", ANSWER.CANCEL, false]);
        }
        return dialog("Spider", text, buttons);
    }

    /* Dialog 119 (dlg_deal_level). Returns the mode, or -1 on Cancel. */
    async function difficulty(mode) {
        const box = document.createElement("div");
        const titles = [
            ["Easy: One Suit", 1],
            ["Medium: Two Suits", 2],
            ["Difficult: Four Suits", 4],
        ];
        const radios = [];
        for (const [label, value] of titles) {
            const l = document.createElement("label");
            const r = document.createElement("input");
            r.type = "radio";
            r.name = "spider-difficulty";
            r.checked = value === mode;
            l.appendChild(r);
            l.appendChild(document.createTextNode(" " + label));
            box.appendChild(l);
            radios.push([r, value]);
        }
        const ok = await dialog("Difficulty", box, [
            ["OK", true, true],
            ["Cancel", false, false],
        ]);
        if (!ok) {
            return -1;
        }
        for (const [r, value] of radios) {
            if (r.checked) {
                return value;
            }
        }
        return mode;
    }

    /* Dialog 117 (dlg_options). Bitmask of the six boxes, or -1 on Cancel. */
    async function options(csv) {
        const current = csv.split(",").map((v) => v === "1");
        const titles = [
            "Animate when dealing cards",
            "Automatically save game on exit",
            "Automatically open previous game at startup",
            "Prompt before saving a game",
            "Prompt before opening a saved game",
            "Use sound effects",
        ];
        const box = document.createElement("div");
        const checks = [];
        for (let i = 0; i < titles.length; i++) {
            const l = document.createElement("label");
            const c = document.createElement("input");
            c.type = "checkbox";
            c.checked = !!current[i];
            l.appendChild(c);
            l.appendChild(document.createTextNode(" " + titles[i]));
            box.appendChild(l);
            checks.push(c);
        }
        const ok = await dialog("Spider Options", box, [
            ["OK", true, true],
            ["Cancel", false, false],
        ]);
        if (!ok) {
            return -1;
        }
        let bits = 0;
        checks.forEach((c, i) => {
            if (c.checked) {
                bits |= 1 << i;
            }
        });
        return bits;
    }

    /* Dialog 118 (dlg_stats): the preformatted table from the wasm host.
     * Returns true for Reset, false for OK. */
    async function stats(body) {
        const table = document.createElement("table");
        const heads = ["", "High Score", "Wins", "Losses", "Win Rate", "Most Wins", "Most Losses", "Current"];
        const headRow = document.createElement("tr");
        for (const h of heads) {
            const td = document.createElement("td");
            td.className = "level-head";
            td.textContent = h;
            headRow.appendChild(td);
        }
        table.appendChild(headRow);
        for (const line of body.trimEnd().split("\n")) {
            const row = document.createElement("tr");
            for (const cell of line.split("\t")) {
                const td = document.createElement("td");
                td.textContent = cell;
                row.appendChild(td);
            }
            table.appendChild(row);
        }
        return await dialog("Spider Statistics", table, [
            ["OK", false, true],
            ["Reset", true, false],
        ]);
    }

    /* Dialog 130 (dlg_timer): non-blocking, so the fireworks keep running. */
    function won() {
        dialog("Congratulations, you won!", "Do you want to start another game?", [
            ["Yes", true, true],
            ["No", false, false],
        ]).then((yes) => {
            if (yes) {
                sendCommand("NEW");
            }
        });
    }

    /* Dialog 107 (dlg_about): bitmap 106 and the copyright string. */
    function about() {
        const box = document.createElement("div");
        const img = document.createElement("img");
        img.className = "dialog-art";
        img.src = "assets/bitmaps/106.bmp";
        img.alt = "";
        box.appendChild(img);
        box.appendChild(document.createTextNode("© 1998-2000 Microsoft Corporation.\nAll rights reserved."));
        dialog("About Spider", box, [["OK", true, true]]);
    }

    /* spider.chm is not shipped; the same summary the macOS port shows. */
    function help() {
        dialog(
            "Spider Help",
            "Remove all the cards from the ten stacks by building runs from king down to ace in one suit. " +
                "A completed run leaves the table.\n\n" +
                "Drag a card, or a run of one suit in descending order, onto a card one rank higher, or onto an " +
                "empty stack. Click the stock (lower right) to deal a new row; every stack must have a card " +
                "first. Click the score box or press M for a hint. Right-click a face-up card to see it in " +
                "full.\n\n" +
                "You start with 500 points, lose one per move, and gain 100 per completed run.",
            [["OK", true, true]]
        );
    }

    function init(mod) {
        module = mod;
        buildMenu();
    }

    return { init, syncMenu, confirm, difficulty, options, stats, won, about, help };
})();
