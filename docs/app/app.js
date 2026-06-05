const modes = {
  none: {
    title: "1. NO RELIABILITY / NO TRANSPARENCY",
    summary: "The operator only sees a robot-generated situation map, colleague status, and the highlighted route. No confidence score or supporting diagnostics are shown."
  },
  reliability: {
    title: "2. RELIABILITY ONLY",
    summary: "The operator sees the robot-generated route recommendation and an 82% confidence score, but not the evidence behind that estimate."
  },
  transparency: {
    title: "3. TRANSPARENCY ONLY",
    summary: "The operator sees supporting signals such as traversal history, battery, sensor quality, and gas status, but no explicit reliability score."
  },
  full: {
    title: "4. RELIABILITY + TRANSPARENCY (FULL INFORMATION)",
    summary: "The operator sees the robot-generated route recommendation, confidence, and supporting transparency signals."
  }
};

const root = document.documentElement;
const modeTitle = document.querySelector("#mode-title");
const modeSummary = document.querySelector("#mode-summary");
const tabs = Array.from(document.querySelectorAll(".mode-tab"));

function setMode(mode) {
  const nextMode = modes[mode] ? mode : "full";
  root.dataset.mode = nextMode;
  modeTitle.textContent = modes[nextMode].title;
  modeSummary.textContent = modes[nextMode].summary;

  tabs.forEach((tab) => {
    const isActive = tab.dataset.mode === nextMode;
    tab.classList.toggle("is-active", isActive);
    tab.setAttribute("aria-selected", String(isActive));
    tab.tabIndex = isActive ? 0 : -1;
  });

  const url = new URL(window.location.href);
  url.searchParams.set("mode", nextMode);
  window.history.replaceState({}, "", url);
}

tabs.forEach((tab) => {
  tab.addEventListener("click", () => setMode(tab.dataset.mode));
  tab.addEventListener("keydown", (event) => {
    const index = tabs.indexOf(tab);
    if (event.key === "ArrowRight" || event.key === "ArrowLeft") {
      event.preventDefault();
      const direction = event.key === "ArrowRight" ? 1 : -1;
      const next = tabs[(index + direction + tabs.length) % tabs.length];
      next.focus();
      setMode(next.dataset.mode);
    }
  });
});

const initialMode = new URLSearchParams(window.location.search).get("mode") || "full";
setMode(initialMode);
