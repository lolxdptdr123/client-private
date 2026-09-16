const loginForm = document.getElementById("login-form");
const adminApp = document.getElementById("admin-app");
const loginError = document.getElementById("login-error");
const KEY = "helix-admin-password";

async function api(path, body) {
  const res = await fetch(path, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ password: sessionStorage.getItem(KEY), ...body }),
  });
  const data = await res.json().catch(() => ({}));
  if (!res.ok) throw new Error(data.error || "Request failed");
  return data;
}

async function loadLogs() {
  const res = await fetch("/api/changelog");
  const data = await res.json();
  const mount = document.getElementById("admin-log-list");
  window.HelixAdmin.renderChangelog(data.entries || [], mount, { canDelete: true });
  mount.querySelectorAll("[data-del]").forEach((btn) => {
    btn.addEventListener("click", async () => {
      await api("/api/changelog/delete", { id: btn.dataset.del });
      loadLogs();
    });
  });
}

async function loadContact() {
  const res = await fetch("/api/contact");
  const c = await res.json();
  document.getElementById("discord-url").value = c.discord || "";
  document.getElementById("telegram-url").value = c.telegram || "";
}

function showAdmin() {
  loginForm.classList.add("hidden");
  adminApp.classList.remove("hidden");
  const date = document.getElementById("log-date");
  date.value = new Date().toISOString().slice(0, 10);
  loadLogs();
  loadContact();
}

loginForm.addEventListener("submit", async (e) => {
  e.preventDefault();
  loginError.classList.remove("show");
  const password = document.getElementById("admin-password").value;
  try {
    const res = await fetch("/api/login", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ password }),
    });
    if (!res.ok) throw new Error("auth");
    sessionStorage.setItem(KEY, password);
    showAdmin();
  } catch {
    loginError.classList.add("show");
  }
});

document.getElementById("add-log").addEventListener("submit", async (e) => {
  e.preventDefault();
  await api("/api/changelog", {
    date: document.getElementById("log-date").value,
    version: document.getElementById("log-version").value,
    body: document.getElementById("log-body").value,
  });
  document.getElementById("log-body").value = "";
  document.getElementById("log-ok").classList.add("show");
  loadLogs();
});

document.getElementById("save-contact").addEventListener("submit", async (e) => {
  e.preventDefault();
  await api("/api/contact", {
    discord: document.getElementById("discord-url").value,
    telegram: document.getElementById("telegram-url").value,
  });
  document.getElementById("contact-ok").classList.add("show");
});

if (sessionStorage.getItem(KEY)) {
  fetch("/api/login", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ password: sessionStorage.getItem(KEY) }),
  })
    .then((r) => {
      if (r.ok) showAdmin();
      else sessionStorage.removeItem(KEY);
    })
    .catch(() => {});
}
