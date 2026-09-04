#!/usr/bin/env python3
"""Builds the Pakku user manual as a PDF.

    .venv/bin/python docs/manual/build_manual.py

The response figures come from docs/manual/data, measured on the plugin itself
by pakku_figures. The screenshots come from pakku_guishot. No curve here is
drawn by hand: if the DSP changes, run both again and the manual follows.
"""
from __future__ import annotations

import sys
import textwrap
from pathlib import Path

import matplotlib
matplotlib.use("Agg")

import matplotlib.pyplot as plt
import numpy as np
from matplotlib import font_manager
from matplotlib.backends.backend_pdf import PdfPages
from matplotlib.patches import FancyArrowPatch, FancyBboxPatch, Rectangle

ROOT = Path(__file__).resolve().parents[2]
IMG = ROOT / "docs/manual/img"
DATA = ROOT / "docs/manual/data"
OUT = ROOT / "docs/Pakku-Manual.pdf"

VERSION = "1.0.0"

# --preview saves each page as a PNG for checking
PREVIEW = (Path(__file__).resolve().parents[2] / "docs/manual/preview"
           if "--preview" in sys.argv else None)

# ---- identidade -------------------------------------------------------------
INK = "#12222b"
BODY = "#33454f"
MUTE = "#6d828d"
ACCENT = "#0d7f93"
ACCENT_HI = "#16a8c1"
RULE = "#d3dee3"
PANEL = "#f1f6f8"
DARK = "#0a151c"
WARN = "#b8842a"

PAGE_W, PAGE_H = 8.27, 11.69          # A4
MARGIN = 0.82
COL_W = PAGE_W - 2 * MARGIN

_michroma = ROOT / "resources/fonts/Michroma-Regular.ttf"
if _michroma.exists():
    font_manager.fontManager.addfont(str(_michroma))
    DISPLAY = "Michroma"
else:
    DISPLAY = "DejaVu Sans"

plt.rcParams.update({
    "font.family": "DejaVu Sans",
    "pdf.fonttype": 42,               # vector, selectable text
    "axes.linewidth": 0.7,
})


class Page:
    """A page with a cursor that walks down as content is added."""

    def __init__(self, pdf, number, chapter=""):
        self.pdf = pdf
        self.number = number
        self.chapter = chapter
        self.fig = plt.figure(figsize=(PAGE_W, PAGE_H))
        self.fig.patch.set_facecolor("white")
        self.y = 1 - MARGIN / PAGE_H
        self._furniture()

    # -- coordinates in inches, origin at the bottom left --
    def fx(self, inches):
        return inches / PAGE_W

    def fy(self, inches):
        return inches / PAGE_H

    def _furniture(self):
        if self.number <= 1:
            return
        self.fig.text(self.fx(MARGIN), self.fy(0.55), self.chapter,
                      fontsize=7.2, color=MUTE, va="center")
        self.fig.text(1 - self.fx(MARGIN), self.fy(0.55), f"{self.number}",
                      fontsize=7.2, color=MUTE, va="center", ha="right")
        self.fig.lines.append(plt.Line2D(
            [self.fx(MARGIN), 1 - self.fx(MARGIN)],
            [self.fy(0.75), self.fy(0.75)],
            transform=self.fig.transFigure, color=RULE, lw=0.7))

    # -- blocks ------------------------------------------------------------
    def title(self, text, kicker=None):
        if kicker:
            self.y -= self.fy(0.10)
            self.fig.text(self.fx(MARGIN), self.y, kicker.upper(), fontsize=8,
                          color=ACCENT, family=DISPLAY, va="top")
            self.y -= self.fy(0.32)
        self.fig.text(self.fx(MARGIN), self.y, text, fontsize=21, color=INK,
                      va="top", weight="bold")
        self.y -= self.fy(0.46)
        self.fig.lines.append(plt.Line2D(
            [self.fx(MARGIN), self.fx(MARGIN + 1.1)], [self.y, self.y],
            transform=self.fig.transFigure, color=ACCENT, lw=2.2))
        self.y -= self.fy(0.30)

    def heading(self, text):
        self.y -= self.fy(0.16)
        self.fig.text(self.fx(MARGIN), self.y, text, fontsize=12.5, color=INK,
                      va="top", weight="bold")
        self.y -= self.fy(0.30)

    def para(self, text, size=9.6, color=None, width=96, gap=0.16):
        color = color or BODY
        for line in textwrap.wrap(" ".join(text.split()), width):
            self.fig.text(self.fx(MARGIN), self.y, line, fontsize=size,
                          color=color, va="top")
            self.y -= self.fy(size / 58.0)
        self.y -= self.fy(gap)

    def bullets(self, items, size=9.6, width=90):
        for item in items:
            first = True
            for line in textwrap.wrap(" ".join(item.split()), width):
                self.fig.text(self.fx(MARGIN + (0.0 if first else 0.24)), self.y,
                              ("•  " if first else "") + line,
                              fontsize=size, color=BODY, va="top")
                self.y -= self.fy(size / 58.0)
                first = False
            self.y -= self.fy(0.05)
        self.y -= self.fy(0.12)

    def steps(self, items, size=9.6, width=86):
        for i, item in enumerate(items, 1):
            self.fig.text(self.fx(MARGIN + 0.06), self.y - self.fy(0.015), f"{i}",
                          fontsize=8.6, color="white", va="top", ha="center",
                          weight="bold",
                          bbox=dict(boxstyle="circle,pad=0.34", fc=ACCENT, ec="none"))
            first = True
            for line in textwrap.wrap(" ".join(item.split()), width):
                self.fig.text(self.fx(MARGIN + 0.30), self.y, line, fontsize=size,
                              color=BODY, va="top")
                self.y -= self.fy(size / 58.0)
                first = False
            self.y -= self.fy(0.13)
        self.y -= self.fy(0.08)

    def note(self, text, kind="info", width=88):
        colour = {"info": ACCENT, "warn": WARN}[kind]
        lines = textwrap.wrap(" ".join(text.split()), width)
        height = 0.20 + len(lines) * 0.155
        top = self.y
        ax = self.fig.add_axes([self.fx(MARGIN), top - self.fy(height),
                                self.fx(COL_W), self.fy(height)])
        ax.set_axis_off()
        ax.add_patch(FancyBboxPatch((0.002, 0.03), 0.996, 0.94,
                                    boxstyle="round,pad=0.004,rounding_size=0.012",
                                    fc=PANEL, ec=colour, lw=0.9,
                                    transform=ax.transAxes, clip_on=False))
        ax.plot([0.004, 0.004], [0.06, 0.94], color=colour, lw=3.2,
                transform=ax.transAxes, clip_on=False, solid_capstyle="butt")
        for i, line in enumerate(lines):
            ax.text(0.022, 0.80 - i * (0.155 / height), line, fontsize=9.0,
                    color=INK, va="top", transform=ax.transAxes)
        self.y = top - self.fy(height + 0.18)

    def table(self, rows, widths, header=None, size=9.0):
        x0 = MARGIN
        if header:
            for text, w in zip(header, widths):
                self.fig.text(self.fx(x0), self.y, text, fontsize=8.0,
                              color=ACCENT, va="top", weight="bold")
                x0 += w
            self.y -= self.fy(0.20)
            self.fig.lines.append(plt.Line2D(
                [self.fx(MARGIN), 1 - self.fx(MARGIN)], [self.y, self.y],
                transform=self.fig.transFigure, color=RULE, lw=0.7))
            self.y -= self.fy(0.16)

        for row in rows:
            x0 = MARGIN
            tallest = 1
            for text, w in zip(row, widths):
                chars = max(8, int(w * 12.4))
                lines = textwrap.wrap(" ".join(str(text).split()), chars) or [""]
                tallest = max(tallest, len(lines))
                for i, line in enumerate(lines):
                    self.fig.text(self.fx(x0), self.y - self.fy(i * size / 58.0),
                                  line, fontsize=size, color=BODY, va="top")
                x0 += w
            self.y -= self.fy(tallest * size / 58.0 + 0.13)
        self.y -= self.fy(0.10)

    def image(self, path, height, caption=None, callouts=(), arrows=()):
        """Places a screenshot and, over it, callouts in the plugin's own
        coordinates (1000 x 580), which is how the layout is written in code."""
        img = plt.imread(path)
        aspect = img.shape[0] / img.shape[1]
        width = min(COL_W, height / aspect)
        height = width * aspect

        ax = self.fig.add_axes([self.fx(MARGIN + (COL_W - width) / 2),
                                self.fy(0) + self.y - self.fy(height),
                                self.fx(width), self.fy(height)])
        # extent maps the image onto the plugin's coordinate system, which is
        # how the callouts are written — without it xlim crops the screenshot
        ax.imshow(img, extent=(0, 1000, 580, 0), aspect="auto")
        ax.set_xlim(0, 1000)
        ax.set_ylim(580, 0)
        ax.set_axis_off()
        ax.add_patch(Rectangle((0, 0), 1000, 580, fill=False, ec=RULE, lw=0.8,
                               clip_on=False))

        for n, (x, y) in enumerate(callouts, 1):
            ax.annotate(str(n), xy=(x, y), fontsize=7.4, color="white",
                        ha="center", va="center", weight="bold", zorder=6,
                        bbox=dict(boxstyle="circle,pad=0.30", fc=ACCENT_HI,
                                  ec="white", lw=1.1))

        for (x, y), (tx, ty), label in arrows:
            ax.add_patch(FancyArrowPatch((tx, ty), (x, y),
                                         arrowstyle="-|>", mutation_scale=9,
                                         color=ACCENT_HI, lw=1.3, zorder=5,
                                         shrinkA=2, shrinkB=2))
            ax.text(tx, ty, label, fontsize=7.2, color=ACCENT, weight="bold",
                    ha="center", va="bottom", zorder=6,
                    bbox=dict(boxstyle="round,pad=0.22", fc="white",
                              ec=ACCENT_HI, lw=0.7))

        self.y -= self.fy(height + 0.12)
        if caption:
            for line in textwrap.wrap(" ".join(caption.split()), 112):
                self.fig.text(self.fx(MARGIN), self.y, line, fontsize=8.0,
                              color=MUTE, va="top", style="italic")
                self.y -= self.fy(0.145)
            self.y -= self.fy(0.16)

    def axes(self, height, pad_left=0.55, pad_right=0.06):
        ax = self.fig.add_axes([self.fx(MARGIN + pad_left),
                                self.y - self.fy(height),
                                self.fx(COL_W - pad_left - pad_right),
                                self.fy(height)])
        ax.tick_params(labelsize=7.4, colors=MUTE, length=3)
        for side in ("top", "right"):
            ax.spines[side].set_visible(False)
        for side in ("left", "bottom"):
            ax.spines[side].set_color(RULE)
        ax.grid(True, color=RULE, lw=0.6, alpha=0.8)
        ax.set_axisbelow(True)
        # the x-axis labels live below the box: the cursor has to clear them,
        # otherwise the caption rides up over them
        self.y -= self.fy(height + 0.46)
        return ax

    def caption(self, text, width=112):
        for line in textwrap.wrap(" ".join(text.split()), width):
            self.fig.text(self.fx(MARGIN), self.y, line, fontsize=8.0, color=MUTE,
                          va="top", style="italic")
            self.y -= self.fy(0.145)
        self.y -= self.fy(0.16)

    def space(self, inches):
        self.y -= self.fy(inches)

    def close(self):
        # the footer sits at 0.75in; past that is content falling off the page
        if self.y < self.fy(0.95):
            over = (self.fy(0.95) - self.y) * PAGE_H
            print(f"  WARNING: page {self.number} ({self.chapter}) "
                  f"overflows the margin by {over:.2f} in")
        self.pdf.savefig(self.fig)
        if PREVIEW:
            PREVIEW.mkdir(parents=True, exist_ok=True)
            self.fig.savefig(PREVIEW / f"page-{self.number:02d}.png", dpi=110)
        plt.close(self.fig)


# ---- figures, all from the measured data ------------------------------------
def load(name):
    return np.genfromtxt(DATA / name, delimiter=",", names=True)


def _envelope(x, ms, win_ms=5.0):
    """Peak per window.

    The raw waveform is no good for comparison: one trace ends up eight times
    the size of another and they no longer share a scale. The window also has
    to be longer than one period of the fundamental, otherwise what shows is
    the carrier rippling rather than the envelope."""
    w = max(1, int(win_ms * 48000 / 1000))
    n = len(x) // w * w
    env = np.abs(x[:n]).reshape(-1, w).max(axis=1)
    return ms[:n:w][:len(env)], env


def fig_transients(page, mode):
    d = load("transients.csv")
    ax = page.axes(2.05)
    limit = 150 if mode == "attack" else 330

    if mode == "attack":
        series = [("dry", "Neutral", MUTE, 1.2),
                  ("attack_up", "Attack +0.85", ACCENT_HI, 1.5),
                  ("attack_down", "Attack −0.85", "#c4623f", 1.5)]
    else:
        series = [("dry", "Neutral", MUTE, 1.2),
                  ("length_up", "Length +0.85", ACCENT_HI, 1.5),
                  ("length_down", "Length −0.85", "#c4623f", 1.5)]

    _, ref = _envelope(d["dry"], d["ms"])
    ref = ref.max()

    for key, label, colour, lw in series:
        t, env = _envelope(d[key], d["ms"])
        db = 20 * np.log10(np.maximum(env, 1e-7) / ref)
        m = t <= limit
        ax.plot(t[m], db[m], color=colour, lw=lw, label=label)

    ax.axhline(0, color=RULE, lw=0.8, ls="--")
    ax.set_ylim(-45, 24)
    ax.set_xlim(0, limit)
    ax.set_xlabel("milliseconds", fontsize=8, color=MUTE)
    ax.set_ylabel("dB, against the untouched peak", fontsize=8, color=MUTE)
    ax.legend(fontsize=7.6, frameon=False, loc="upper right", labelcolor=BODY)


def fig_crossover(page):
    d = load("crossover.csv")
    ax = page.axes(1.82)
    m = (d["hz"] >= 20) & (d["hz"] <= 20000)

    for key, label, colour, lw in [("low", "Low band", "#2b7fb8", 1.3),
                                   ("mid", "Mid band", ACCENT_HI, 1.3),
                                   ("high", "High band", "#8a63c4", 1.3),
                                   ("sum", "All three summed", INK, 1.3)]:
        ls = "--" if key == "sum" else "-"
        ax.semilogx(d["hz"][m], d[key][m], color=colour, lw=lw, ls=ls, label=label)

    for f in (800, 8000):
        ax.axvline(f, color=RULE, lw=0.9, ls="--")
    ax.axhline(-6, color=RULE, lw=0.9, ls=":")

    ax.set_xlim(20, 20000)
    ax.set_ylim(-42, 6)
    ax.set_xticks([20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000])
    ax.set_xticklabels(["20", "50", "100", "200", "500", "1k", "2k", "5k", "10k", "20k"])
    ax.set_xlabel("frequency (Hz)", fontsize=8, color=MUTE)
    ax.set_ylabel("dB", fontsize=8, color=MUTE)
    ax.legend(fontsize=7.6, frameon=False, loc="lower center", ncol=4, labelcolor=BODY)


def fig_ceiling(page):
    d = load("ceiling.csv")
    ax = page.axes(2.25)
    ax.plot(d["in_db"], d["in_db"], color=RULE, lw=1.0, ls="--", label="No ceiling")
    ax.plot(d["in_db"], d["limiter_db"], color=ACCENT_HI, lw=1.4, label="Limiter")
    ax.plot(d["in_db"], d["softclip_db"], color="#c4623f", lw=1.4, label="Soft Clipper")
    ax.set_xlabel("input level (dBFS)", fontsize=8, color=MUTE)
    ax.set_ylabel("output peak (dBFS)", fontsize=8, color=MUTE)
    ax.set_xlim(-18, 12)
    ax.set_ylim(-18, 4)
    ax.legend(fontsize=7.6, frameon=False, loc="upper left", labelcolor=BODY)


def fig_flow(page):
    ax = page.axes(1.35, pad_left=0.0, pad_right=0.0)
    ax.set_axis_off()
    ax.set_xlim(0, 100)
    ax.set_ylim(0, 20)
    ax.grid(False)

    names = ["Input\nGain", "Crossover\n3 bands", "Transient\nShaping",
             "Tone\nPresence · Air", "NYC\nparallel", "Ceiling\nLimit · Clip",
             "Output\nGain"]
    half, step, cy = 5.6, 14.2, 13.0
    centres = [7.4 + i * step for i in range(len(names))]

    for label, x in zip(names, centres):
        ax.add_patch(FancyBboxPatch((x - half, cy - 4.0), half * 2, 8.0,
                                    boxstyle="round,pad=0.2,rounding_size=1.0",
                                    fc=PANEL, ec=ACCENT, lw=1.0))
        ax.text(x, cy, label, fontsize=6.9, color=INK, ha="center", va="center",
                linespacing=1.5)

    for a, b in zip(centres, centres[1:]):
        ax.add_patch(FancyArrowPatch((a + half, cy), (b - half, cy),
                                     arrowstyle="-|>", mutation_scale=8,
                                     color=MUTE, lw=1.0))

    # dry branch, carrying the same latency as the wet one
    x0, x1 = centres[0], centres[-1]
    ax.plot([x0, x0, x1, x1], [cy - 4.0, 4.6, 4.6, cy - 4.0], color=MUTE,
            lw=1.0, ls="--")
    ax.text((x0 + x1) / 2, 2.0,
            "dry path — delayed to match, so partial Mix never comb-filters",
            fontsize=7.0, color=MUTE, ha="center", va="center", style="italic")
    ax.text(x1 - 7.5, 5.7, "Mix", fontsize=7.2, color=ACCENT, ha="center",
            weight="bold")


# ---- cover ------------------------------------------------------------------
def cover(pdf):
    fig = plt.figure(figsize=(PAGE_W, PAGE_H))
    fig.patch.set_facecolor(DARK)
    ax = fig.add_axes([0, 0, 1, 1])
    ax.set_axis_off()
    ax.set_xlim(0, 1)
    ax.set_ylim(0, 1)

    # the interface mark: straight bars at cap height, one of them accented
    shape = [0.38, 0.62, 0.88, 0.70, 1.00, 0.70, 0.88, 0.62, 0.38]
    cap_in = 0.40                      # inches, matching the wordmark cap height
    stem_in = cap_in / 12.0
    pitch_in = stem_in * 3.8

    cap = cap_in / PAGE_H
    stem = stem_in / PAGE_W
    pitch = pitch_in / PAGE_W

    x0, cy = 0.135, 0.792
    for i, h in enumerate(shape):
        colour = ACCENT_HI if h >= 1.0 else "#9fc0cd"
        ax.add_patch(Rectangle((x0 + i * pitch, cy - cap * h / 2),
                               stem, cap * h, fc=colour, ec="none"))

    ax.text(x0 + 9 * pitch + 0.030, cy, "PAKKU", fontsize=34, color="#e8f4f9",
            family=DISPLAY, va="center")

    ax.plot([0.135, 0.865], [0.742, 0.742], color="#1c3d4b", lw=1.0)
    ax.text(0.135, 0.716, "MULTIBAND TRANSIENT SHAPER", fontsize=9.5,
            color=ACCENT_HI, family=DISPLAY, va="center")

    ax.text(0.135, 0.645, "User Manual", fontsize=27, color="#e8f4f9", va="center")
    ax.text(0.135, 0.601,
            f"Version {VERSION}    ·    Audio Unit and VST3    ·    macOS and Windows",
            fontsize=10, color="#8fb2c0", va="center")

    img = plt.imread(IMG / "ui-single.png")
    iw = 0.76
    ih = iw * img.shape[0] / img.shape[1] * PAGE_W / PAGE_H
    inner = fig.add_axes([(1 - iw) / 2, 0.255, iw, ih])
    inner.imshow(img)
    inner.set_axis_off()

    ax.plot([0.30, 0.70], [0.175, 0.175], color="#1c3d4b", lw=1.0)
    ax.text(0.5, 0.140, "Made in Brazil by Kyantech Labs", fontsize=10,
            color="#8fb2c0", ha="center", va="center")
    ax.text(0.5, 0.110, "Free and open source", fontsize=10, color=ACCENT_HI,
            ha="center", va="center")

    pdf.savefig(fig)
    if PREVIEW:
        PREVIEW.mkdir(parents=True, exist_ok=True)
        fig.savefig(PREVIEW / "page-01.png", dpi=110)
    plt.close(fig)


# ---- pages -------------------------------------------------------------------
def build():
    with PdfPages(OUT) as pdf:
        cover(pdf)
        n = 1

        # ---------------------------------------------------------------- CONTENTS
        n += 1
        p = Page(pdf, n, "Contents")
        p.title("Contents")
        entries = [
            ("What a transient shaper does", 3),
            ("Attack and Length", 4),
            ("Signal flow", 5),
            ("Install on macOS", 6),
            ("Install on Windows", 7),
            ("The interface", 8),
            ("Controls, one by one", 9),
            ("Working in three bands", 11),
            ("Reading the display", 12),
            ("The ceiling: Limiter or Soft Clipper", 13),
            ("Presets", 14),
            ("Settings", 15),
            ("Recipes", 16),
            ("Troubleshooting", 17),
            ("Specifications", 18),
        ]
        for label, page_no in entries:
            p.fig.text(p.fx(MARGIN), p.y, label, fontsize=10.6, color=BODY, va="top")
            p.fig.text(1 - p.fx(MARGIN), p.y, str(page_no), fontsize=10.6,
                       color=ACCENT, va="top", ha="right", weight="bold")
            p.y -= p.fy(0.055)
            p.fig.lines.append(plt.Line2D(
                [p.fx(MARGIN + 2.6), 1 - p.fx(MARGIN + 0.22)], [p.y, p.y],
                transform=p.fig.transFigure, color=RULE, lw=0.6, ls=":"))
            p.y -= p.fy(0.235)
        p.space(0.25)
        p.note("Everything in this manual was produced from the plugin itself. "
               "The screenshots come straight out of the interface and every curve "
               "is a measurement of the running DSP, not a drawing.")
        p.close()

        # ------------------------------------------------------------- CONCEPT
        n += 1
        p = Page(pdf, n, "Concept")
        p.title("What a transient shaper does", kicker="Concept")
        p.para("A compressor asks how loud something is and turns it down when it "
               "crosses a line. A transient shaper asks a different question: is "
               "this sound arriving, or is it leaving? It then treats the two "
               "halves separately.")
        p.para("Pakku decides by running two envelope followers side by side. One "
               "reacts almost instantly, the other lags behind. When a hit lands, "
               "the fast one shoots up while the slow one is still climbing, and "
               "the gap between them is the attack. As the sound decays the gap "
               "closes and reverses, and that difference is the tail. Gain is "
               "applied from those two differences, so the plugin never needs to "
               "know how loud the material is in absolute terms.")
        p.para("That is why a transient shaper works the same on a quiet room mic "
               "and a slammed snare: it responds to shape, not to level. It is "
               "also why there is no ratio and no makeup gain to chase.")
        p.heading("What that buys you")
        p.bullets([
            "Pull a kick forward in a mix without raising its level.",
            "Take room and bleed out of drums by shortening the tail, no gate, no chatter.",
            "Add sustain to a guitar or a synth that decays too fast.",
            "Soften a harsh pick or stick attack while leaving the body untouched.",
        ])
        p.note("Level-independent does not mean it ignores level entirely. The "
               "Threshold control sets a floor below which nothing is shaped, so "
               "noise and bleed between hits are left alone.")
        p.close()

        # ----------------------------------------------------------- ATTACK/TAIL
        n += 1
        p = Page(pdf, n, "Concept")
        p.title("Attack and Length", kicker="Concept")
        p.para("Two faders flank the display. TRANSIENTS on the left works on the "
               "arrival of a sound; LENGTH on the right works on what happens "
               "after. Both are centred at zero and go in either direction.")
        p.heading("Transients")
        p.para("Positive values sharpen the leading edge — the stick, the pluck, "
               "the consonant. Negative values round it off. The figure below is a "
               "single drum hit rendered through Pakku three times: untouched, "
               "with Attack pushed up, and with Attack pulled down.")
        fig_transients(p, "attack")
        p.caption("Measured through the plugin. The lift is steepest at the "
                  "onset and eases off over the following tens of milliseconds, "
                  "as the slow detector catches up with the fast one.")
        p.heading("Length")
        p.para("Positive values hold the tail open, which pulls up room, sustain "
               "and decay. Negative values close it early, which is the cleanest "
               "way to dry out a drum kit or tighten a bass note.")
        fig_transients(p, "length")
        p.caption("Same hit over a longer window. The first milliseconds are "
                  "identical in all three; the curves separate as the tail runs "
                  "on, which is exactly where Length works.")
        p.close()

        # --------------------------------------------------------------- CHAIN
        n += 1
        p = Page(pdf, n, "Concept")
        p.title("Signal flow", kicker="Concept")
        p.para("Knowing the order matters, because it explains why some controls "
               "interact and others do not.")
        fig_flow(p)
        p.space(0.10)
        p.table(
            [("Crossover", "Linkwitz–Riley, 4th order, 24 dB per octave. Each band "
                           "is 6 dB down at the crossover point and the three sum "
                           "back flat."),
             ("Transient shaping", "Runs per band in multiband mode, or once "
                                   "across the full range in single band mode."),
             ("Tone", "Presence and Air, applied after shaping so they colour the "
                      "result rather than feeding the detectors."),
             ("NYC", "Parallel compression blended in, New York style."),
             ("Ceiling", "The last thing in the chain, so nothing after it can "
                         "push the signal back over."),
             ("Dry path", "Delayed to match the wet path exactly. Without that, "
                          "any Mix below 100% would comb-filter.")],
            widths=[1.55, COL_W - 1.55], header=("STAGE", "WHAT IT DOES"))
        p.note("Reported latency is 239 samples at 48 kHz — 5 ms of lookahead the "
               "ceiling needs to catch a peak before it happens. Your host "
               "compensates for this automatically on playback.")
        p.close()

        # ---------------------------------------------------------- INSTALL MAC
        n += 1
        p = Page(pdf, n, "Installation")
        p.title("Install on macOS", kicker="Installation")
        p.para("Requires macOS 11 Big Sur or later. The build is universal, so it "
               "runs natively on both Apple Silicon and Intel.")
        p.steps([
            "Download Pakku-%s.pkg from the releases page." % VERSION,
            "Double-click the package. If macOS says it cannot verify the "
            "developer, right-click the file instead, choose Open, then confirm "
            "with Open in the dialog. You only need to do this once.",
            "On the second screen, tick the formats you want. Audio Unit covers "
            "Logic Pro, GarageBand and Live; VST3 covers Cubase, Reaper, Studio "
            "One, Bitwig and most others. Installing both is fine.",
            "Enter your password when asked — the plug-in folders belong to the "
            "system, so the installer needs permission to write there.",
            "Restart your host, or ask it to rescan plug-ins. Most only look on "
            "startup.",
        ])
        p.heading("Where things land")
        p.table(
            [("Audio Unit", "/Library/Audio/Plug-Ins/Components/Pakku.component"),
             ("VST3", "/Library/Audio/Plug-Ins/VST3/Pakku.vst3"),
             ("Presets", "~/Library/Audio/Presets/Kyantech Labs/Pakku/")],
            widths=[1.35, COL_W - 1.35], header=("ITEM", "PATH"))
        p.note("To uninstall, delete the two bundles above. Your presets live "
               "elsewhere and are left alone.", kind="info")
        p.close()

        # -------------------------------------------------------- INSTALL WINDOWS
        n += 1
        p = Page(pdf, n, "Installation")
        p.title("Install on Windows", kicker="Installation")
        p.para("Requires 64-bit Windows 10 or later. Windows hosts use VST3; "
               "Audio Unit is a macOS-only format and is not part of this "
               "installer.")
        p.steps([
            "Download Pakku-%s-Setup.exe from the releases page." % VERSION,
            "Run it. If SmartScreen puts up a blue panel, click More info, then "
            "Run anyway. This appears because the installer is not code signed "
            "yet, not because anything is wrong with it.",
            "Accept the elevation prompt. The shared VST3 folder is owned by the "
            "system, so the installer needs administrator rights.",
            "Restart your host, or rescan plug-ins.",
        ])
        p.heading("Where things land")
        p.table(
            [("VST3", r"C:\Program Files\Common Files\VST3\Pakku.vst3"),
             ("Presets", "%APPDATA%" + chr(92) + "Kyantech Labs" + chr(92) + "Pakku")],
            widths=[1.35, COL_W - 1.35], header=("ITEM", "PATH"))
        p.heading("Uninstalling")
        p.para("Pakku registers a normal uninstaller. Use Settings → Apps, or run "
               "it from the folder above. Your presets are kept.")
        p.note("Some hosts keep their own scan cache. If Pakku still does not "
               "appear after a restart, find the plug-in manager in your host and "
               "force a full rescan rather than an incremental one.", kind="warn")
        p.close()

        # ------------------------------------------------------------ INTERFACE
        n += 1
        p = Page(pdf, n, "The interface")
        p.title("The interface", kicker="Tour")
        p.para("Everything Pakku does is on one screen. Nothing is hidden behind "
               "a tab or a page.")
        p.image(IMG / "ui-single.png", 3.55,
                callouts=[(500, 41), (964, 41), (60, 300), (130, 300), (500, 250),
                          (872, 300), (940, 300), (93, 415), (907, 415),
                          (71, 512), (179, 512), (286, 512), (446, 512),
                          (607, 512), (714, 512), (821, 512), (928, 514)])
        p.space(0.02)
        p.table(
            [("1", "Preset selector", "10", "Band mode — Multi or Single"),
             ("2", "Settings and credits", "11", "Ceiling — Limiter or Soft Clipper"),
             ("3", "Input meter, with peak hold", "12", "NYC — parallel compression"),
             ("4", "TRANSIENTS — attack", "13", "Mix — dry against wet"),
             ("5", "Display — waveform and spectrum", "14", "Presence"),
             ("6", "LENGTH — sustain and tail", "15", "Air"),
             ("7", "Output meter", "16", "Threshold"),
             ("8", "Input gain", "17", "Bypass"),
             ("9", "Output gain", "", "")],
            widths=[0.28, 2.55, 0.30, COL_W - 3.13], size=8.6)
        p.note("Any knob: drag up and down to change it, double-click to return "
               "it to default. Hold Shift while dragging for fine control.")
        p.close()

        # ------------------------------------------------------------- CONTROLS
        n += 1
        p = Page(pdf, n, "Controls")
        p.title("Controls, one by one", kicker="Reference")
        p.table(
            [("TRANSIENTS", "−1.00 to +1.00", "0.00",
              "Attack. Positive sharpens the leading edge, negative rounds it. In "
              "multiband mode this fader edits whichever band is selected in the "
              "spectrum, and the caption changes to say which."),
             ("LENGTH", "−1.00 to +1.00", "0.00",
              "Sustain and tail. Positive holds the decay open, negative closes it "
              "early. Same band-following behaviour."),
             ("THRESHOLD", "−50 to 0 dB", "0.00 dB",
              "The floor below which nothing is shaped. Leave it at 0 to shape "
              "everything. Pull it down to keep bleed, room and noise between hits "
              "out of the detectors. Draggable straight off the dashed line in the "
              "waveform."),
             ("MIX", "0.00 to 1.00", "1.00",
              "Blends the processed signal against the untouched one. The dry path "
              "is delay-compensated, so partial settings stay phase-correct."),
             ("NYC", "0.00 to 0.50", "0.00",
              "Parallel compression folded in underneath, New York style. Adds "
              "density and weight without flattening the peaks you just shaped."),
             ("PRESENCE", "0 to 100", "0",
              "Upper midrange lift, after the shaping. Cuts through a dense mix "
              "without touching the detectors."),
             ("AIR", "0 to 100", "0",
              "High shelf for openness on top. Small amounts go a long way on "
              "cymbals and acoustic sources.")],
            widths=[1.15, 1.05, 0.70, COL_W - 2.90],
            header=("CONTROL", "RANGE", "DEFAULT", "WHAT IT DOES"), size=8.5)
        p.close()

        n += 1
        p = Page(pdf, n, "Controls")
        p.title("Controls, continued", kicker="Reference")
        p.table(
            [("INPUT GAIN", "−inf to +15 dB", "0.00 dB",
              "Level going in. Because shaping is level-independent, this is for "
              "gain staging and for driving the ceiling, not for changing how much "
              "the plugin shapes."),
             ("OUTPUT GAIN", "−inf to +15 dB", "0.00 dB",
              "Level coming out, after the ceiling."),
             ("MULTI / SINGLE", "two states", "Single",
              "Three bands with their own shaping, or one shaper across the whole "
              "range. In single band mode the spectrum steps aside and the "
              "waveform takes the whole display."),
             ("LIMIT / SOFT CLIP", "two states", "Soft Clip",
              "Which ceiling catches the peaks. See page 13."),
             ("BYPASS", "on or off", "off",
              "Takes the whole chain out. Latency stays reported either way, so "
              "the comparison is time-aligned and honest.")],
            widths=[1.15, 1.05, 0.70, COL_W - 2.90],
            header=("CONTROL", "RANGE", "DEFAULT", "WHAT IT DOES"), size=8.5)

        p.heading("Interface size")
        p.para("Three sizes, chosen in the settings panel and stored with the "
               "session: 850 × 493, 1000 × 580 and 1250 × 725. The whole interface "
               "scales as one piece, so proportions, line weights and type stay "
               "identical at every size. The window has no drag handle on purpose.")

        p.heading("Automation")
        p.para("Every audible control is exposed to the host and can be automated. "
               "The crossover frequencies are exposed but marked non-automatable, "
               "because sweeping a crossover during playback is rarely what anyone "
               "actually wants. Solo and Mute per band are exposed for recall but "
               "not for automation either.")
        p.close()

        # ------------------------------------------------------------ MULTIBAND
        n += 1
        p = Page(pdf, n, "Multiband")
        p.title("Working in three bands", kicker="Multiband")
        p.para("Switch the left toggle to MULTI and the display splits: the "
               "spectrum appears under the waveform with two draggable dividers, "
               "and each band gets its own attack and sustain.")
        p.image(IMG / "ui-multi.png", 1.95,
                callouts=[(300, 430), (372, 380), (530, 278), (130, 300)])
        p.table(
            [("1", "The selected band, lifted while the other two sit under a veil",
              "3", "Mute and Solo, per band"),
             ("2", "Crossover divider — drag it", "4", "The faders now edit band 1")],
            widths=[0.28, 3.05, 0.28, COL_W - 3.61], size=8.6)
        p.heading("How to work it")
        p.steps([
            "Click anywhere inside a band on the spectrum to select it. The band "
            "lights up, the other two dim, and the two faders rebind to it — the "
            "captions change to TRANSIENTS 1, LENGTH 1 and so on.",
            "Drag a divider to move a crossover point. The frequency is written "
            "alongside it while you drag.",
            "Use S to hear one band on its own while you dial it in, and M to drop "
            "a band out of the sum.",
            "Switch back to SINGLE at any time. Each mode keeps its own "
            "settings, so you can compare without losing either.",
        ])
        fig_crossover(p)
        p.caption("Measured at the default 800 Hz and 8 kHz. Each band is 6 dB "
                  "down at its crossover point; the three sum flat within "
                  "0.06 dB.")
        p.close()

        # --------------------------------------------------------------- DISPLAY
        n += 1
        p = Page(pdf, n, "Display")
        p.title("Reading the display", kicker="Display")
        p.image(IMG / "ui-multi.png", 2.60,
                callouts=[(500, 148), (250, 200), (500, 400), (690, 440)])
        p.table(
            [("1", "Threshold line — drag it", "3", "Live spectrum"),
             ("2", "Scrolling waveform", "4", "Frequency grid: 100 Hz, 1 kHz, 10 kHz")],
            widths=[0.28, 2.55, 0.28, COL_W - 3.11], size=8.6)
        p.heading("The waveform")
        p.para("The top half scrolls the signal as it plays. The dashed line "
               "across it is the Threshold, and anything reaching past it is "
               "picked out in colour — that is exactly the material being shaped. "
               "Move the cursor near the line and it thickens and grows handles: "
               "drag it to set Threshold without leaving the display. The knob "
               "follows, because both are the same parameter.")
        p.heading("The spectrum")
        p.para("The bottom half appears in multiband mode only. It shows the live "
               "spectrum with a frequency grid, the two crossover dividers, and "
               "the selected band lifted while the others sit under a veil.")
        p.note("In single band mode there is no crossover to show and no band to "
               "choose, so the spectrum steps aside and the waveform takes the "
               "whole display.")
        p.close()

        # ------------------------------------------------------------------ CEILING
        n += 1
        p = Page(pdf, n, "Ceiling")
        p.title("The ceiling", kicker="Limiter or Soft Clipper")
        p.para("Sharpening transients makes peaks. The ceiling is what stops them "
               "leaving the plugin above 0 dBFS, and it sits last in the chain so "
               "nothing after it can undo the job.")
        fig_ceiling(p)
        p.caption("Measured: a 220 Hz sine driven into the ceiling, output peak "
                  "on the vertical axis. The dashed line is where the signal "
                  "would land with nothing holding it.")
        p.table(
            [("0 dBFS", "0.00 dB", "−0.87 dB"),
             ("+6 dBFS", "+0.07 dB", "−0.01 dB"),
             ("+12 dBFS", "+0.10 dB", "0.00 dB")],
            widths=[1.5, 1.9, 1.9],
            header=("DRIVEN TO", "LIMITER OUTPUT", "SOFT CLIP OUTPUT"), size=8.6)
        p.heading("Limiter")
        p.para("Transparent and firm. It looks 5 ms ahead, so it catches a peak "
               "before it arrives instead of reacting after the fact, and it holds "
               "the line almost exactly — driving it 12 dB past the ceiling still "
               "lands within a tenth of a decibel. Use it when the transient shape "
               "you dialled in is the point and you want it kept.")
        p.heading("Soft Clipper")
        p.para("Musical rather than invisible. The knee opens below the threshold "
               "and the curve bends into it, so the loudest peaks round over and "
               "pick up harmonics on the way. The curve is odd-symmetric, which "
               "means odd harmonics only and no second-harmonic buzz. Use it when "
               "you want the drums to feel pushed and glued.")
        p.note("Threshold and the ceiling are separate ideas. Threshold decides "
               "what gets shaped; the ceiling decides what gets out. Setting one "
               "does not move the other.")
        p.close()

        # --------------------------------------------------------------- PRESETS
        n += 1
        p = Page(pdf, n, "Presets")
        p.title("Presets", kicker="Presets")
        p.para("Pakku ships with 31 factory presets. Init heads the list and is "
               "the neutral starting point — every parameter at its default. You "
               "can always get back to it.")
        p.image(IMG / "ui-menu.png", 2.35,
                caption="Click the preset name to open the list. Arrows on either "
                        "side step through it without opening anything.")
        p.heading("Saving your own")
        p.steps([
            "Set the plugin up the way you want it.",
            "Open the preset menu and choose Save As.",
            "Type a name. Factory names are refused, so you cannot overwrite one "
            "by accident.",
        ])
        p.heading("Where they live")
        p.table(
            [("Factory/", "The 31 shipped presets, written to disk on first run. "
                          "Kept in step with the copies compiled into the plugin, "
                          "so an update can deliver new ones and a deleted or "
                          "damaged file comes back on its own."),
             ("User/", "Yours. Saved from the plugin, or dropped in from anywhere "
                       "— a preset from a friend is just a file. Nothing in here "
                       "is ever touched by an update.")],
            widths=[0.95, COL_W - 0.95], header=("FOLDER", "WHAT IT HOLDS"))
        p.para("Show preset folder in the settings panel opens the parent of both. "
               "Rescan preset folder re-reads them, which is what you want after "
               "dropping a file in while the plugin is open.")
        p.note("A preset is a small binary file ending in .pkku. It stores real "
               "values — decibels, hertz, percent — rather than normalised ones, "
               "so files stay meaningful even if a range changes in a later "
               "version.")
        p.close()

        # ------------------------------------------------------------- SETTINGS
        n += 1
        p = Page(pdf, n, "Settings")
        p.title("Settings", kicker="Settings")
        p.para("The gear at the top right opens one panel with everything that is "
               "not a sound control. Press Escape, click the X, or click outside "
               "the card to close it.")
        p.image(IMG / "ui-settings.png", 2.85,
                callouts=[(440, 171), (472, 217), (472, 263), (472, 309), (670, 322)])
        p.table(
            [("1", "Interface size", "4", "Restore defaults"),
             ("2", "Show preset folder", "5", "Support the project"),
             ("3", "Rescan preset folder", "", "")],
            widths=[0.28, 2.55, 0.28, COL_W - 3.11], size=8.6)
        p.table(
            [("Interface size", "Three fixed sizes. Stored with the session, so a "
                                "project reopens the way you left it."),
             ("Show preset folder", "Opens the folder holding Factory and User."),
             ("Rescan preset folder", "Re-reads both folders and restores anything "
                                      "missing from Factory."),
             ("Restore defaults", "Returns every parameter to its default without "
                                  "loading a preset."),
             ("Pakku version", "Which build you are running."),
             ("Latency", "What the plugin reports to the host, in samples.")],
            widths=[1.55, COL_W - 1.55], header=("ROW", "WHAT IT DOES"), size=8.8)
        p.close()

        # -------------------------------------------------------------- RECIPES
        n += 1
        p = Page(pdf, n, "Recipes")
        p.title("Recipes", kicker="Starting points")
        p.para("None of these are rules. They are places to start, with the reason "
               "behind each move so you can tell when to break it.")

        recipes = [
            ("Kick forward in a dense mix",
             "Single band · Transients +0.35 · Length −0.15 · Soft Clip",
             "The attack lift puts the beater back in front without adding level, "
             "and the small tail cut stops the low end blooming into the snare. "
             "Soft Clip glues the result."),
            ("Snare with too much room",
             "Multi · band 1 Length −0.50 · band 2 Transients +0.30 · Threshold −18 dB",
             "The tail cut lives in the low band where the room sits; the attack "
             "lift lives in the mids where the crack is. Threshold keeps the "
             "decaying room out of the detectors so it fades instead of pumping."),
            ("Overheads, tighter",
             "Multi · band 3 Length −0.40 · Mix 0.70",
             "Only the cymbal band shortens. Mixing back to 70% keeps the kit "
             "sounding like a room rather than a sample library."),
            ("Bass that will not sit",
             "Single · Transients +0.25 · Length −0.20 · NYC 0.20 · Limiter",
             "Finger or pick definition up, note decay reined in, then parallel "
             "compression underneath to hold the floor steady."),
            ("Acoustic guitar, less pick",
             "Single · Transients −0.30 · Air 25",
             "Negative attack takes the edge off the pick without dulling the "
             "instrument; Air puts the sparkle back above it."),
            ("Drum bus glue",
             "Multi · gentle everything · NYC 0.15 · Soft Clip · Mix 0.85",
             "Small moves per band, parallel compression to fuse them, soft clip "
             "to round the peaks. Mix slightly under 100% keeps the original "
             "dynamics in view."),
        ]
        for name, settings, why in recipes:
            p.fig.text(p.fx(MARGIN), p.y, name, fontsize=10.2, color=INK,
                       va="top", weight="bold")
            p.y -= p.fy(0.20)
            p.fig.text(p.fx(MARGIN), p.y, settings, fontsize=8.6, color=ACCENT,
                       va="top")
            p.y -= p.fy(0.19)
            p.para(why, size=8.8, width=104, gap=0.16)
        p.close()

        # -------------------------------------------------------- TROUBLESHOOTING
        n += 1
        p = Page(pdf, n, "Troubleshooting")
        p.title("Troubleshooting", kicker="If something is off")
        p.table(
            [("The plugin does not show up in my host",
              "Force a full rescan rather than an incremental one — many hosts "
              "cache their plug-in list. On macOS confirm the bundle is in "
              "/Library/Audio/Plug-Ins, and remember Logic and GarageBand only "
              "load Audio Units, never VST3."),
             ("macOS refuses to open the installer",
              "Right-click the .pkg, choose Open, then confirm with Open. The "
              "build is not code signed yet, so Gatekeeper asks once."),
             ("Windows shows a blue SmartScreen panel",
              "Click More info, then Run anyway. Same reason as above."),
             ("It sounds thin or hollow at partial Mix",
              "That is the classic sign of an uncompensated dry path — but not "
              "here. Pakku delays the dry signal to match the wet one exactly. If "
              "you hear it, check whether your host is compensating latency on the "
              "track, and whether you have a second copy of the plugin in the "
              "chain."),
             ("Raising Threshold makes things louder, not quieter",
              "Threshold is not a ceiling. It sets the floor below which nothing "
              "is shaped, so raising it means less material gets shaped. If you "
              "want peaks held down, that is the Ceiling section."),
             ("Nothing happens when I move the faders",
              "Check the Threshold. If it is above the level of the material, the "
              "detectors never engage. Also check Mix, and whether Bypass is lit."),
             ("A factory preset went missing",
              "Open Settings and press Rescan preset folder. Factory presets are "
              "restored from copies inside the plugin, so nothing is ever lost "
              "for good."),
             ("My own preset disappeared after an update",
              "It should not — the User folder is never touched. Check that the "
              "file is in User/ and not in Factory/, which is kept in step with "
              "the plugin."),
             ("The interface is too small or too large",
              "Settings, first row. Three sizes, remembered with the session. "
              "There is no drag handle on the window by design.")],
            widths=[2.05, COL_W - 2.05], header=("SYMPTOM", "WHAT TO DO"), size=8.5)
        p.close()

        # ---------------------------------------------------------- SPECIFICATIONS
        n += 1
        p = Page(pdf, n, "Specifications")
        p.title("Specifications", kicker="Reference")
        p.table(
            [("Formats", "Audio Unit (macOS), VST3 (macOS and Windows)"),
             ("Channels", "Stereo in, stereo out"),
             ("Sample rates", "Any rate the host provides; all filters are "
                              "derived from it at prepare time"),
             ("Latency", "239 samples at 48 kHz — 5 ms of ceiling lookahead, "
                         "reported to the host for automatic compensation"),
             ("Crossover", "Linkwitz–Riley, 4th order, 24 dB per octave, with "
                           "allpass compensation on the low band so the three "
                           "bands sum flat within 0.06 dB"),
             ("Crossover range", "50 Hz to 15 kHz, both dividers"),
             ("Parameters", "29 exposed to the host"),
             ("Presets", "31 factory, unlimited user, .pkku format"),
             ("Interface", "Three sizes: 850 × 493, 1000 × 580, 1250 × 725"),
             ("macOS", "11 Big Sur or later, universal (Apple Silicon and Intel)"),
             ("Windows", "64-bit Windows 10 or later")],
            widths=[1.45, COL_W - 1.45], header=("", ""), size=8.8)

        p.heading("Licence and credits")
        p.para("Pakku is free and open source, released under the GNU Affero "
               "General Public License v3.0. The source, the issue tracker and "
               "every release live at github.com/danielalves96/pakku-vst.")
        p.para("Built on the JUCE framework. Interface icons from Phosphor Icons "
               "(MIT). Display typeface Michroma, under the SIL Open Font "
               "License. Full licence texts ship with the source.")

        p.space(0.10)
        p.note("Pakku is made in Brazil by Daniel Luiz Alves, under Kyantech "
               "Labs. It is free and it stays free. If it earns a place in your "
               "chain, you can support the next release at "
               "github.com/sponsors/danielalves96.")
        p.close()

    print(f"written: {OUT}")
    print(f"size: {OUT.stat().st_size / 1024:.0f} KB")


if __name__ == "__main__":
    build()
