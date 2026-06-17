const modes = new Set(["home", "mission"]);

const root = document.documentElement;
const tabs = Array.from(document.querySelectorAll(".mode-tab"));
const robotStatusTitle = document.querySelector("#robot-status-title");
const routeLegendLabel = document.querySelector("#route-legend-label");

function setMode(mode) {
  const nextMode = modes.has(mode) ? mode : "home";
  root.dataset.mode = nextMode;

  if (robotStatusTitle) {
    robotStatusTitle.textContent = "Robot Status";
  }

  if (routeLegendLabel) {
    routeLegendLabel.textContent = nextMode === "home" ? "Inspection Loop" : "Route A (Recommended by the Robot)";
  }

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

const initialMode = new URLSearchParams(window.location.search).get("mode") || "home";
setMode(initialMode);
