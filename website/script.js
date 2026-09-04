const reducedMotion = window.matchMedia("(prefers-reduced-motion: reduce)").matches;
const root = document.documentElement;
const header = document.querySelector("[data-header]");
const primaryDownload = document.querySelector("[data-primary-download]");
const primaryPlatform = document.querySelector("[data-primary-platform]");

const isWindows = /Win/i.test(navigator.userAgent);
if (isWindows && primaryDownload && primaryPlatform) {
  primaryDownload.href = "downloads/Pakku-1.0.0-Windows.zip";
  primaryPlatform.textContent = "Windows · VST3 · v1.0.0";
}

const setHeader = () => header?.classList.toggle("is-scrolled", window.scrollY > 24);
setHeader();
window.addEventListener("scroll", setHeader, { passive: true });

const revealObserver = new IntersectionObserver(
  (entries) => entries.forEach((entry) => {
    if (entry.isIntersecting) {
      entry.target.classList.add("is-visible");
      revealObserver.unobserve(entry.target);
    }
  }),
  { threshold: 0.14, rootMargin: "0px 0px -5%" }
);
document.querySelectorAll(".reveal").forEach((el) => revealObserver.observe(el));

if (!reducedMotion) {
  window.addEventListener("pointermove", (event) => {
    root.style.setProperty("--mouse-x", `${event.clientX}px`);
    root.style.setProperty("--mouse-y", `${event.clientY}px`);
  }, { passive: true });

  document.querySelectorAll("[data-tilt]").forEach((element) => {
    element.addEventListener("pointermove", (event) => {
      const rect = element.getBoundingClientRect();
      const x = (event.clientX - rect.left) / rect.width - 0.5;
      const y = (event.clientY - rect.top) / rect.height - 0.5;
      const base = element.classList.contains("plugin-stage") ? "translate(-50%, -47%) " : "";
      element.style.transform = `${base}perspective(1400px) rotateX(${4 - y * 4}deg) rotateY(${x * 4}deg)`;
    });
    element.addEventListener("pointerleave", () => {
      element.style.transform = "";
    });
  });
}

const canvas = document.querySelector("[data-wave]");
if (canvas) {
  const ctx = canvas.getContext("2d");
  let time = 0;
  const resize = () => {
    const ratio = Math.min(window.devicePixelRatio || 1, 2);
    canvas.width = Math.floor(canvas.clientWidth * ratio);
    canvas.height = Math.floor(canvas.clientHeight * ratio);
    ctx.setTransform(ratio, 0, 0, ratio, 0, 0);
  };
  const draw = () => {
    const width = canvas.clientWidth;
    const height = canvas.clientHeight;
    ctx.clearRect(0, 0, width, height);
    const center = height / 2;
    const gradient = ctx.createLinearGradient(0, 0, width, 0);
    gradient.addColorStop(0, "rgba(36,107,254,0)");
    gradient.addColorStop(.25, "rgba(36,107,254,.55)");
    gradient.addColorStop(.5, "rgba(54,199,255,.95)");
    gradient.addColorStop(.75, "rgba(36,107,254,.55)");
    gradient.addColorStop(1, "rgba(36,107,254,0)");
    ctx.strokeStyle = gradient;
    ctx.lineWidth = 1;
    ctx.beginPath();
    for (let x = 0; x <= width; x += 3) {
      const envelope = Math.sin(Math.PI * x / width) ** 3;
      const pulse = Math.sin(x * .055 + time) * 22 + Math.sin(x * .017 - time * .6) * 15 + Math.sin(x * .16) * 4;
      const y = center + pulse * envelope;
      x === 0 ? ctx.moveTo(x, y) : ctx.lineTo(x, y);
    }
    ctx.stroke();
    time += .018;
    if (!reducedMotion) requestAnimationFrame(draw);
  };
  resize();
  draw();
  window.addEventListener("resize", resize);
}
