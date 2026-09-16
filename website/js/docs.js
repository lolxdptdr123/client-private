const CATS = [
  { id: "basics", label: "Basics", items: [{ id: "get-started", title: "Get started" }] },
  {
    id: "modules",
    label: "Modules",
    items: [{ id: "introduction", title: "Introduction" }],
    groups: [
      {
        id: "combat",
        label: "Combat",
        items: [
          "aim-assist", "anti-bot", "auto-block", "auto-clicker", "auto-refill", "auto-rod",
          "auto-weapon", "backtrack", "blink", "criticals", "keep-sprint", "lag-range",
          "reach", "sprint-reset", "throw", "velocity",
        ],
      },
      {
        id: "block",
        label: "Block",
        items: ["auto-tool", "block-in", "bridge-assist", "clutch", "fast-break", "fast-place"],
      },
      {
        id: "move",
        label: "Move",
        items: ["fast-stop", "inv-walk", "no-jump-delay", "no-slow", "quick-accel", "snap-tap", "sprint", "strafe"],
      },
      {
        id: "utility",
        label: "Utility",
        items: [
          "anti-debuff", "armor-switcher", "bow-boost", "chest-stealer", "enemies", "friends",
          "inv-manager", "no-item-release", "ping-fix", "right-clicker", "scroll", "tick-locker",
        ],
      },
      {
        id: "visual",
        label: "Visual",
        items: [
          "array-list", "block-esp", "chams", "esp", "gui", "indicators", "item-esp", "nametags",
          "no-hurt-cam", "notifications", "player-esp", "pointers", "storage-esp", "tracer", "trajectories",
        ],
      },
    ],
  },
];

const PAGES = {
  "get-started": {
    title: "Get started",
    crumb: "Basics",
    html: `
      <p>This page assumes you already have a Helix license. Start Minecraft 1.7.10 or 1.8.9, then start the loader and wait until it reports ready.</p>
      <h2 id="menu">Opening the menu</h2>
      <p>After the client is attached you should see an in-game notification with the menu key. The default bind is <kbd>Right Shift</kbd>. You can change it under Visual → GUI.</p>
      <h2 id="navigation">Navigation</h2>
      <p>The top of the menu switches pages: Combat, Move, Visual, Utility, Profiles, and Unload. Hold the last item to unload the client.</p>
      <h2 id="modules">Modules</h2>
      <p>Each page lists modules. Click the switch to toggle. Press the expand control to show settings. Click the bind button, then press a key (or Escape to clear).</p>
      <p>Binds toggle on press. Hold-mode is available on the bind itself: the module stays on while the key is held.</p>
      <h2 id="settings">Settings</h2>
      <p>Sliders, toggles and dropdowns match the names on this docs site. Hold Ctrl and click a slider to type an exact value.</p>
      <h2 id="profiles">Profiles</h2>
      <p>Most module settings are not saved until you store them in a profile. GUI settings are stored separately and survive profile switches.</p>
    `,
    toc: [
      ["menu", "Opening the menu"],
      ["navigation", "Navigation"],
      ["modules", "Modules"],
      ["settings", "Settings"],
      ["profiles", "Profiles"],
    ],
  },
  introduction: {
    title: "Introduction",
    crumb: "Modules",
    html: `
      <p>Only modules that ship in this client are documented here. Open a name in the sidebar for its overview and the settings exposed in the menu.</p>
      <p>Combat covers hitting and targeting. Block covers placing and breaking. Move covers walking and sprint. Utility covers inventory and lists. Visual covers ESP, HUD and the GUI module.</p>
    `,
    toc: [],
  },
};

function page(title, overview, settings) {
  const toc = [["overview", "Overview"]];
  let html = `<p id="overview">${overview}</p>`;
  if (settings.length) {
    html += `<h2 id="settings">Settings</h2>`;
    toc.push(["settings", "Settings"]);
    for (const [name, text] of settings) {
      const id = name.toLowerCase().replace(/[^a-z0-9]+/g, "-");
      html += `<div class="setting" id="${id}"><strong>${name}</strong><p>${text}</p></div>`;
      toc.push([id, name]);
    }
  }
  return { title, crumb: "Modules", html, toc };
}

Object.assign(PAGES, {
  "aim-assist": page("Aim Assist", "Pulls your crosshair towards other players.", [
    ["Mode", "Blatant or Legit."],
    ["Speed", "How quickly the crosshair moves."],
    ["FOV", "Min / max field of view used to pick a target."],
    ["Distance", "Min / max range, hidden when priority is HurtTime."],
    ["Priority", "Distance, FOV or HurtTime."],
    ["Multipoint", "How far onto the hitbox the aim can sit."],
    ["Target players", "Include other players."],
    ["Allow invisible / naked", "Optional target filters."],
    ["Require click", "Only assist while attacking."],
    ["Weapons only", "Only while holding a configured weapon."],
    ["Break blocks", "Whether assisting continues while mining."],
    ["Keep on target", "Optional hold bind to stick to the current target."],
  ]),
  "anti-bot": page("AntiBot", "Filters bots from targeting and visuals.", [
    ["Min ticks", "How long an entity must exist before it can be a real player."],
    ["Check tab", "Use the tab list as an extra filter."],
    ["Check packets", "Ignore entities that never sent movement for a while."],
    ["Packet grace", "Grace window used by the packet check."],
  ]),
  "auto-block": page("Auto Block", "Predicts incoming hits and blocks your sword.", [
    ["Range", "Distance used to decide when to block."],
    ["Max hurt time", "Stop blocking after this hurt-time window."],
    ["Max hold", "How long a block is held."],
    ["Force anim", "Force the block animation."],
    ["Force anim in range", "Same, only when a target is in range."],
    ["Lag chance / max", "Optional extra delay on the block."],
    ["Prevent delayed attacks", "Skip attacking while a block is still resolving."],
    ["Block again immediately", "Allow another block right after releasing."],
    ["Conditions", "Optional LMB, RMB or damaged-only gates."],
  ]),
  "auto-clicker": page("Clicker", "Clicks for you when holding down left-click. Shown in the menu as Auto Clicker.", [
    ["Mode", "Blatant, Butterfly or Jitter."],
    ["CPS", "Clicks per second (5–25)."],
    ["Exhaust", "Extra variation on Butterfly and Jitter."],
    ["Require click", "Only while the physical mouse is held."],
    ["Weapons only", "Only while holding a configured weapon."],
  ]),
  "auto-refill": page("AutoRefill", "Refills potions or soup from your inventory.", [
    ["Mode", "Blatant, Legit or Semi Blatant."],
    ["Item mode", "Potion, Soup or Both."],
    ["Speed", "How fast slots are moved (0–10)."],
    ["Random mode", "Pick a refill style at random."],
    ["Dynamic speed", "Vary speed while refilling."],
    ["Transition", "Play the inventory animation between moves."],
  ]),
  "auto-rod": page("AutoRod", "Automatically rods players out of melee range.", [
    ["FOV / max look FOV", "How far off-center a target can be."],
    ["Max range", "Furthest distance to throw."],
    ["Cooldown", "Delay between throws."],
    ["Ignore eating", "Do not interrupt eating."],
    ["Move fix", "Correct movement while looking at the target."],
    ["Only if not in reach", "Skip if they are already in melee range."],
    ["Melee reach", "Distance treated as melee."],
    ["Click / rotations", "Whether to click and aim the throw."],
    ["Lucky throw", "Optional extra throw variation."],
    ["Targets", "Players, enemies-only, or mobs."],
    ["ESP", "Highlight the current rod target."],
  ]),
  "auto-weapon": page("Auto Weapon", "Selects your weapon when aiming on a player.", [
    ["Activation", "Delay before swapping, in milliseconds."],
  ]),
  backtrack: page("Backtrack", "Holds incoming packets after you hit a player so they stay hittable.", [
    ["Mode", "Lag, Smooth or Advanced."],
    ["Lag", "Delay in ticks, cooldown, optional distance window."],
    ["Smooth", "Delay, force-flush time, optional sprint-only."],
    ["Advanced", "Min / max delay, lag spacing, hurt-time stop, disable-on rules."],
    ["Draw box", "Show a box on the delayed position."],
  ]),
  blink: page("Blink", "Holds your packets so you teleport when they are released.", [
    ["Direction", "OutBound, InBound or Both."],
    ["Auto send delay", "Flush after this many milliseconds."],
    ["Disable on local / target damage", "Cancel blink when damage is applied."],
    ["Draw ESP", "Show the real position while blinking."],
  ]),
  criticals: page("Criticals", "Increases chance of landing critical hits.", [
    ["Mode", "Packet or Timer."],
    ["Chance", "Percent of hits that try to crit."],
    ["Timer speed", "Used in Timer mode."],
    ["Max queue time", "How long packet mode may wait."],
  ]),
  "keep-sprint": page("KeepSprint", "Keeps sprint after hitting to deal more knockback.", [
    ["Mode", "Dynamic or Static."],
    ["Speed", "Sprint speed after the hit."],
    ["Chance", "How often KeepSprint applies."],
    ["Weapons only", "Only while holding a weapon."],
    ["Only on behind", "Only when hitting from behind."],
  ]),
  "lag-range": page("LagRange", "Delays your packets while you close in so you appear further than you are.", [
    ["Mode", "Static or Dynamic."],
    ["Activation / flush distance", "When lag starts and when it is released."],
    ["Delay", "How long packets are held."],
    ["Only weapon / only sprinting", "Optional conditions."],
    ["Draw box", "Show the lagged position."],
  ]),
  reach: page("Reach", "Extends the distance from which you can attack.", [
    ["Distance", "Attack range in blocks."],
    ["Activate ticks", "How many ticks the extra range stays on."],
    ["Only sprinting", "Require sprint."],
  ]),
  "sprint-reset": page("Sprint Reset", "Automatically restarts your sprint after you hit a player.", [
    ["Mode", "WTap, Sneak or NoStop."],
    ["Delay / stop", "Timing between the hit and the reset."],
    ["Randomize", "Vary the timing."],
    ["Wait for damage", "Wait until hurt-time before resetting."],
    ["Holding weapon", "Require a weapon."],
  ]),
  throw: page("Throw", "Throws pots, soup, pearls and debuffs.", [
    ["Health / soup / debuff / pearl", "Enable each throw type."],
    ["Speed", "How fast that type is thrown."],
    ["Smart / double", "Optional extra throw logic per type."],
    ["Soup auto drop", "Drop bowls after soup."],
    ["Binds", "Dedicated key for each throw type."],
  ]),
  velocity: page("Velocity", "Reduces the amount of knockback you take.", [
    ["Mode", "Blatant, Reverse, Jump or Reduce."],
    ["Horizontal / vertical", "Percent kept in Blatant mode."],
    ["Reverse strength", "Used in Reverse mode."],
    ["Reduce H", "Horizontal percent in Reduce mode."],
    ["Jump delay", "Delay before the jump mode fires."],
    ["Chance", "How often velocity applies."],
    ["Weapons only / moving forward / looking at player / mouse pressed", "Optional conditions."],
  ]),
  "auto-tool": page("AutoTool", "Switches to the best hotbar tool for the block you look at. Lunar 1.7 / 1.8.", [
    ["Switch back", "Return to the previous slot when you stop mining."],
    ["Only mining", "Only while breaking a block."],
    ["Prefer silk", "Prefer silk-touch tools when available."],
    ["Delay", "Wait before swapping."],
  ]),
  "block-in": page("Block In", "Places blocks around you. Useful for Bed Wars. Lunar 1.7 / 1.8.", [
    ["Speed", "How fast blocks are placed."],
    ["Only on ground", "Require standing on a block."],
  ]),
  "bridge-assist": page("BridgeAssist", "Auto-sneaks at block edges while bridging. Lunar 1.7 / 1.8.", [
    ["Edge offset", "How close to the edge before sneaking."],
    ["Unsneak delay", "How long to stay crouched."],
    ["Pitch", "Look-down angle required."],
    ["Only blocks / looking down / sneak on jump", "Optional conditions."],
  ]),
  clutch: page("Clutch", "Places blocks under you when you are about to fall. Lunar 1.7 / 1.8.", [
    ["Range / FOV / min height", "When clutch may fire."],
    ["Click speed / randomization", "Placement pacing."],
    ["Select blocks", "No, on depletion, or always pick a block."],
    ["Only sideways / mid air / on hurt / backwards", "Optional conditions."],
    ["Snap delay / duration", "Aim snap timing."],
    ["Keep jump dir / disable after", "Movement after the clutch."],
  ]),
  "fast-break": page("FastBreak", "Breaks blocks faster than vanilla.", [
    ["Mode", "Normal or Timer."],
    ["Power", "0–100 in Normal mode."],
    ["Multiplier", "Timer mode speed."],
  ]),
  "fast-place": page("FastPlace", "Places blocks faster than vanilla.", [
    ["Mode", "Delay or Click."],
    ["Tick delay", "0–3 in Delay mode."],
    ["Only block", "Ignore non-block items."],
    ["Average CPS", "Click mode rate."],
    ["Hold to click / exhaust", "Click-mode extras."],
  ]),
  "fast-stop": page("FastStop", "Counter-strafes on key release.", [
    ["Axis", "Both, Strafe, or Fwd/Back."],
    ["Disable on sneak", "Turn off while sneaking."],
  ]),
  "inv-walk": page("InvWalk", "Walk while a GUI is open.", [
    ["Mode", "Legit or Blatant."],
  ]),
  "no-jump-delay": page("NoJumpDelay", "Removes vanilla jump cooldown.", []),
  "no-slow": page("NoSlow", "Removes or reduces slowdown while using items.", [
    ["Swords / bows / consumables", "Percent of vanilla slow kept for each type."],
  ]),
  "quick-accel": page("QuickAccel", "Accelerates from standstill faster.", [
    ["Disable on sneak", "Turn off while sneaking."],
  ]),
  "snap-tap": page("SnapTap", "Last opposite key wins while both are held.", [
    ["Axis", "Both, Strafe, or Fwd/Back."],
    ["Only on ground / disable on sneak", "Optional conditions."],
  ]),
  sprint: page("Sprint", "Automatically sprints without holding the sprint bind.", [
    ["Using item / backwards / sideways / in inventory", "When sprint is allowed."],
  ]),
  strafe: page("Strafe", "Strafe faster in different directions than vanilla.", [
    ["On ground / in air / on jump", "Strength in each state."],
    ["Max hurt time", "Skip while recently damaged."],
    ["Holding weapon", "Require a weapon."],
  ]),
  "anti-debuff": page("Anti Debuff", "Hides negative visual effects.", [
    ["Blindness / nausea", "Which overlays to hide."],
  ]),
  "armor-switcher": page("Armor Switcher", "Equips the selected armor set.", [
    ["Speed", "How fast pieces are swapped."],
    ["Bind", "Key that triggers a switch."],
    ["Piece IDs", "Which item IDs belong to each armor slot."],
  ]),
  "bow-boost": page("BowBoost", "Bind fires a bow shot like a macro: switch, shoot, switch back.", [
    ["Charge ticks", "How long to draw the bow."],
    ["Delay", "Pause between steps."],
    ["Switch item", "Swap to the bow automatically."],
    ["Look up / pitch", "Optional look angle while shooting."],
  ]),
  "chest-stealer": page("ChestStealer", "Moves the cursor and shift-clicks chest slots. Lunar 1.7 / 1.8.", [
    ["Delay min / max / first / close", "Timing between clicks and closing."],
    ["Auto close", "Close the chest when done."],
    ["Name check", "Only steal from chests with expected names."],
    ["Randomize / intelligent", "Extra click variation and skip logic."],
  ]),
  enemies: page("Enemies", "Focus combat and visuals on marked enemies.", [
    ["Add / nearby / clear keys", "Binds for managing the list."],
    ["Nearby radius", "Range used by add-nearby."],
    ["Name input", "Type a name to add."],
  ]),
  friends: page("Friends", "Skip friends in combat and visuals.", [
    ["Add / nearby / clear keys", "Binds for managing the list."],
    ["Nearby radius", "Range used by add-nearby."],
  ]),
  "inv-manager": page("Inv Manager", "Manages armor and hotbar while your inventory is open.", [
    ["Delay after open", "Wait before the first move."],
    ["Speed / smart speed / randomize", "How fast slots are sorted."],
    ["Equip armor / sort hotbar / smart fallbacks", "What gets rearranged."],
    ["Hotbar layout", "Preferred item types per slot."],
  ]),
  "no-item-release": page("No Item Release", "Keeps using an item instead of releasing it early.", [
    ["Mode", "Consumable, Sword or All."],
  ]),
  "ping-fix": page("Ping Fix", "Hides lag from Backtrack, LagRange and Blink on /ping.", []),
  "right-clicker": page("Right Clicker", "Clicks for you when holding right-click.", [
    ["CPS", "Clicks per second."],
    ["Blatant / exhaust", "Click pattern extras."],
  ]),
  scroll: page("Scroll", "Hotkey-scrolls to whitelisted items.", [
    ["Scroll bind", "Key that starts a scroll."],
    ["Delay", "Pause between hotbar steps."],
    ["Whitelist", "Which item types are allowed."],
    ["Hotbar keys / sword slot", "Optional per-slot binds."],
  ]),
  "tick-locker": page("TickLocker", "Locks mining to a selected block. 1.8 only (Lunar 1.8.9).", [
    ["Render selected block", "Show the locked block."],
    ["Outline / fill / colors", "How the lock is drawn."],
    ["Target key", "Bind used to pick the block."],
  ]),
  "array-list": page("ArrayList", "Lists enabled modules on your HUD.", [
    ["Background / bar", "Decor behind each row."],
    ["Name / suffix colors", "Module name vs settings text."],
    ["Font size / scroll speed", "Size and animation."],
    ["Title", "Optional header text and color."],
  ]),
  "block-esp": page("Block ESP", "Scans and highlights selected blocks.", [
    ["Range (chunks)", "How far to scan."],
    ["Limit per chunk", "Cap on highlighted blocks."],
    ["IDs / colors", "Which block IDs to show and in which color."],
  ]),
  chams: page("Chams", "Renders players through walls.", [
    ["Players / mobs / animals / villagers / armor stands / invisibles", "What to draw."],
    ["Render texture / glow", "Fill style."],
    ["Hide friends / enemies only", "List filters."],
    ["Colors", "Neutral, friend and enemy tints."],
  ]),
  esp: page("ESP", "Draws boxes around other players.", [
    ["Render mode", "2D, 3D or Both."],
    ["3D / 2D mode", "Outline, fill or both."],
    ["Health bar", "Show health beside the box."],
    ["Hide friends / enemies only / max distance", "Filters."],
    ["Colors and widths", "Outline, fill, friend, enemy, health bar."],
  ]),
  gui: page("GUI", "Configures the cheat menu. Saved separately from profiles.", [
    ["Open bind", "Default Right Shift."],
    ["Unload bind", "Default End."],
    ["Scale", "Menu size."],
    ["Accent", "Highlight color."],
    ["Allow input / compact / wide", "Layout extras."],
  ]),
  indicators: page("Indicators", "Warns when fireballs, pearls or arrows spawn.", [
    ["Fireballs / pearls / arrows", "Which projectiles to watch."],
    ["Coming closer", "Only warn when they move toward you."],
  ]),
  "item-esp": page("Item ESP", "Highlights dropped items in the world.", [
    ["Color", "Highlight tint."],
    ["Max distance", "How far items are shown."],
  ]),
  nametags: page("Nametags", "Custom nametags with health and distance.", [
    ["Show names / health / distance / background / health bar / outline", "What to draw."],
    ["Hide friends / enemies only / max distance", "Filters."],
    ["Scale, sizes and colors", "Layout of the tag."],
  ]),
  "no-hurt-cam": page("No Hurt Cam", "Removes the camera shake when you take damage.", []),
  notifications: page("Notifications", "Shows enable/disable toasts.", [
    ["Hide if in game / hide if hold bind", "When toasts are suppressed."],
    ["Categories", "Combat, Visual, Utility, Blocks."],
    ["Duration / anim speed", "How long toasts stay and how they slide."],
  ]),
  "player-esp": page("Player ESP", "Shows armor, potions and held items.", [
    ["Armor / potions / held item / skeleton / outline / gapple", "What to draw."],
    ["Outline mode / glow", "3D parts or 2D box."],
    ["Hide friends / enemies only / max distance", "Filters."],
    ["Scale and colors", "Skeleton and outline styling."],
  ]),
  pointers: page("Pointers", "Arrows around your crosshair pointing to other players.", [
    ["Range / ignore FOV", "Who gets an arrow."],
    ["Hide friendlies", "Skip friends."],
    ["Style", "2D or 3D."],
    ["Color mode", "Distance, nametag or manual."],
    ["Scale / radius", "Size of the ring."],
  ]),
  "storage-esp": page("Storage ESP", "Highlights chests and other storage.", [
    ["Render mode", "2D, 3D or Both."],
    ["Show labels / max distance", "Text and range."],
    ["Types", "Chest, ender chest, furnace, dispenser, dropper, hopper."],
    ["Colors", "Per-container tint."],
  ]),
  tracer: page("Tracer", "Draws lines from you to other players.", [
    ["Hide friends / enemies only / max distance", "Filters."],
    ["Width and colors", "Line styling."],
  ]),
  trajectories: page("Trajectories", "Predicts projectile paths.", [
    ["Bow / potion / pearl / snowball / egg / rod", "Which items get an arc."],
    ["Line width / color", "How the path is drawn."],
  ]),
});

const TITLES = {};
for (const [id, p] of Object.entries(PAGES)) TITLES[id] = p.title;

function currentId() {
  return (location.hash.replace(/^#\/?/, "") || "get-started");
}

function renderSidebar(active, query) {
  const q = (query || "").trim().toLowerCase();
  const side = document.getElementById("docs-side");
  let html = "";
  for (const cat of CATS) {
    html += `<button class="docs-group" type="button">${cat.label}</button>`;
    for (const item of cat.items) {
      const title = PAGES[item.id]?.title || item.title;
      if (q && !title.toLowerCase().includes(q)) continue;
      html += `<a href="#${item.id}" class="${item.id === active ? "active" : ""}">${title}</a>`;
    }
    if (cat.groups) {
      for (const g of cat.groups) {
        const links = g.items
          .map((id) => {
            const title = PAGES[id]?.title || id;
            if (q && !title.toLowerCase().includes(q)) return "";
            return `<a href="#${id}" class="${id === active ? "active" : ""}">${title}</a>`;
          })
          .join("");
        if (!links && q) continue;
        html += `<button class="docs-group nested" type="button">${g.label}</button>${links}`;
      }
    }
  }
  side.innerHTML = html;
}

function renderPage() {
  const id = currentId();
  const pageData = PAGES[id] || PAGES["get-started"];
  document.title = `${pageData.title} | Helix Docs`;
  document.getElementById("docs-crumb").textContent = pageData.crumb || "Docs";
  document.getElementById("docs-title").textContent = pageData.title;
  document.getElementById("docs-body").innerHTML = pageData.html;
  const toc = document.getElementById("docs-toc");
  if (!pageData.toc.length) {
    toc.innerHTML = "";
  } else {
    toc.innerHTML =
      `<h2>On this page</h2>` +
      pageData.toc.map(([hid, label]) => `<a href="#${id}" data-jump="${hid}">${label}</a>`).join("");
    toc.querySelectorAll("[data-jump]").forEach((a) => {
      a.addEventListener("click", (e) => {
        e.preventDefault();
        document.getElementById(a.dataset.jump)?.scrollIntoView({ behavior: "smooth", block: "start" });
      });
    });
  }
  renderSidebar(id, document.getElementById("docs-search").value);
  window.scrollTo(0, 0);
}

document.getElementById("docs-search").addEventListener("input", () => {
  renderSidebar(currentId(), document.getElementById("docs-search").value);
});

window.addEventListener("hashchange", renderPage);
renderPage();
