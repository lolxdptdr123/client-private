const pages = {
  home: "index.html",
  purchase: "purchase.html",
  docs: "docs.html",
  changelog: "changelog.html",
  contact: "contact.html",
};

function navHtml(active) {
  return `
    <nav class="nav" id="nav">
      <a class="logo" href="index.html"><span class="logo-mark" aria-hidden="true"></span>Helix</a>
      <button class="nav-toggle" id="nav-toggle" aria-label="Menu">▾</button>
      <div class="nav-links">
        <a class="${active === "purchase" ? "active" : ""}" href="purchase.html">Purchase</a>
        <a class="${active === "docs" ? "active" : ""}" href="docs.html">Docs</a>
        <a class="${active === "changelog" ? "active" : ""}" href="changelog.html">Changelog</a>
        <a class="${active === "contact" ? "active" : ""}" href="contact.html">Contact</a>
      </div>
    </nav>`;
}

function footerHtml() {
  return `
    <footer class="footer">
      <div>
        <strong>helixware</strong>
        <p>Independent Minecraft client for 1.7.10 &amp; 1.8.9.<br>Demo marketing site — no live payments.</p>
      </div>
      <div class="footer-cols">
        <div>
          <a href="refund.html">Refund policy</a>
          <a href="privacy.html">Privacy policy</a>
          <a href="terms.html">Terms of service</a>
          <a href="requirements.html">Requirements</a>
        </div>
        <div>
          <a href="profiles.html">Profile generator</a>
          <a href="modules.html">Modules</a>
          <a href="docs.html">Download</a>
          <a href="contact.html">Contact</a>
          <a href="admin.html">Admin</a>
        </div>
      </div>
    </footer>`;
}

document.querySelectorAll("[data-nav]").forEach((el) => {
  el.innerHTML = navHtml(el.dataset.nav);
});
document.querySelectorAll("[data-footer]").forEach((el) => {
  el.innerHTML = footerHtml();
});

const nav = document.getElementById("nav");
const toggle = document.getElementById("nav-toggle");
if (toggle && nav) {
  toggle.addEventListener("click", () => nav.classList.toggle("open"));
}

document.querySelectorAll("[data-toggle]").forEach((btn) => {
  btn.addEventListener("click", () => btn.classList.toggle("on"));
});

document.querySelectorAll("[data-select]").forEach((group) => {
  group.querySelectorAll("[data-option]").forEach((opt) => {
    opt.addEventListener("click", () => {
      group.querySelectorAll("[data-option]").forEach((o) => o.classList.remove("selected"));
      opt.classList.add("selected");
    });
  });
});

const checkout = document.getElementById("checkout");
if (checkout) {
  checkout.addEventListener("submit", (e) => {
    e.preventDefault();
    const alert = document.getElementById("checkout-alert");
    if (alert) alert.classList.add("show");
  });
}

document.querySelectorAll("[data-custom-select]").forEach((wrap) => {
  const btn = wrap.querySelector(".custom-select-btn");
  const menu = wrap.querySelector(".custom-select-menu");
  btn.addEventListener("click", () => menu.classList.toggle("hidden"));
  menu.querySelectorAll("button").forEach((opt) => {
    opt.addEventListener("click", () => {
      menu.querySelectorAll("button").forEach((o) => o.classList.remove("active"));
      opt.classList.add("active");
      btn.firstChild.textContent = opt.dataset.value;
      menu.classList.add("hidden");
    });
  });
  document.addEventListener("click", (e) => {
    if (!wrap.contains(e.target)) menu.classList.add("hidden");
  });
});
if (contact) {
  contact.addEventListener("submit", (e) => {
    e.preventDefault();
    const alert = document.getElementById("contact-alert");
    if (alert) alert.classList.add("show");
  });
}

const gen = document.getElementById("generate-profile");
if (gen) {
  gen.addEventListener("click", () => {
    const name = document.getElementById("profile-name");
    const out = document.getElementById("profile-out");
    const label = (name && name.value.trim()) || "Custom";
    const preset = document.querySelector("[data-select='preset'] .selected")?.dataset.option || "subtle";
    out.textContent = `${label} · ${preset} · exported locally (nothing is uploaded)`;
    out.classList.add("show");
  });
}

function escapeHtml(s) {
  return String(s)
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;");
}

function formatLogDate(iso) {
  const d = new Date(`${iso}T00:00:00`);
  if (Number.isNaN(d.getTime())) return iso;
  return d.toLocaleDateString("en-US", { month: "long", day: "numeric", year: "numeric" });
}

function renderChangelog(entries, mount, { canDelete = false } = {}) {
  if (!mount) return;
  if (!entries.length) {
    mount.innerHTML = `<p class="lede">No entries yet.</p>`;
    return;
  }
  mount.innerHTML = entries
    .map((e) => {
      const del = canDelete
        ? `<button class="log-del" type="button" data-del="${e.id}">Delete</button>`
        : "";
      return `<article class="log">
        <div class="log-row">
          <div>
            <time datetime="${escapeHtml(e.date)}">${escapeHtml(formatLogDate(e.date))} — ${escapeHtml(e.version)}</time>
            <p>${escapeHtml(e.body)}</p>
          </div>
          ${del}
        </div>
      </article>`;
    })
    .join("");
}

const changelogList = document.getElementById("changelog-list");
if (changelogList) {
  fetch("/api/changelog")
    .then((r) => r.json())
    .then((data) => renderChangelog(data.entries || [], changelogList))
    .catch(() =>
      fetch("data/changelog.json")
        .then((r) => r.json())
        .then((entries) => renderChangelog(entries, changelogList))
    );
}

fetch("/api/contact")
  .then((r) => r.json())
  .then((c) => {
    const d = document.getElementById("discord-link");
    const t = document.getElementById("telegram-link");
    if (d && c.discord) d.href = c.discord;
    if (t && c.telegram) t.href = c.telegram;
  })
  .catch(() => {});

window.HelixAdmin = { renderChangelog, formatLogDate };

