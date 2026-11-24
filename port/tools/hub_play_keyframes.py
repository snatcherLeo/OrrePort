"""2026-11-09: the hub's Play motion ("Lift and spin", style 04 of docs/design/mockups/19-disc-spin-styles.html,
snatcherLeo: "I love option 4, Lift and Spin") as RmlUi keyframes, printed for port/ui/hub-play.rcss.

RmlUi has no cubic-bezier() timing, but it applies an animation's tween to each keyframe segment
(ElementAnimation::GetInterpolationFactorAndKeys), so each mockup curve is sampled every 25 ms and played with a linear
tween. Each layer's animation runs 2 s from the moment Play is pressed: its mockup delay is held as the start value,
and after the mockup's 1 s it holds the end value (RmlUi shows the element's own style during a delay and after an
animation ends, so delays and endings are written into the keyframes instead; the hub hands the window to the game at
1.05 s, while every animation still holds its end value).

    python -I port/tools/hub_play_keyframes.py > (paste into port/ui/hub-play.rcss)
"""


def bezier(x1, y1, x2, y2):
    """CSS cubic-bezier(x1, y1, x2, y2) as a function of progress 0..1"""

    def coord(t, a, b):
        return 3 * a * (1 - t) ** 2 * t + 3 * b * (1 - t) * t * t + t ** 3

    def ease(x):
        if x <= 0:
            return 0.0
        if x >= 1:
            return 1.0
        lo, hi = 0.0, 1.0
        for _ in range(60):
            mid = (lo + hi) / 2
            if coord(mid, x1, x2) < x:
                lo = mid
            else:
                hi = mid
        return coord((lo + hi) / 2, y1, y2)

    return ease


def linear(x):
    return min(1.0, max(0.0, x))


def segment(t, start, length, curve):
    """progress of a mockup step that starts at `start` s and lasts `length` s"""
    return curve((t - start) / length)


LIFT = bezier(0.22, 1, 0.36, 1)
SPIN = bezier(0.5, 0, 0.9, 0.5)
TRAIL = bezier(0.5, 0, 1, 0.6)
EASE_IN = bezier(0.42, 0, 1, 1)
BLACK = bezier(0.4, 0, 1, 1)


def lift(t):  # 0% 0, 20% -6px, 100% -6px over 1 s; the curve on each segment
    return -6 * segment(t, 0, 0.2, LIFT)


def spin(t):  # 2 turns from 0.15 s over 0.85 s
    return 720 * segment(t, 0.15, 0.85, SPIN)


def sharp(t):  # 1 -> 0.35 at 60% -> 0, linear, from 0.25 s over 0.65 s
    u = (t - 0.25) / 0.65
    if u <= 0:
        return 1.0
    if u <= 0.6:
        return 1 - 0.65 * (u / 0.6)
    return max(0.0, 0.35 * (1 - (u - 0.6) / 0.4))


def ghost_opacity(t):  # 0 -> 0.2 at 40% -> 0.12, from 0.25 s over 0.55 s, the trail curve per segment
    u = (t - 0.25) / 0.55
    if u <= 0:
        return 0.0
    if u <= 0.4:
        return 0.2 * TRAIL(u / 0.4)
    return 0.2 - 0.08 * TRAIL(min(1.0, (u - 0.4) / 0.6))


def ghost_turn(offset):  # 0 -> offset over the same 0.55 s, one segment
    return lambda t: offset * segment(t, 0.25, 0.55, TRAIL)


def smear(t):  # fades in from 0.55 s over 0.35 s, ease-in
    return segment(t, 0.55, 0.35, EASE_IN)


def black(t):  # from 0.6 s over 0.4 s
    return segment(t, 0.6, 0.4, BLACK)


def keyframes(name, prop, fn, unit, fmt="{:.3f}", step=0.025, total=2.0):
    steps = [i * step for i in range(int(round(1.0 / step)) + 1)]  # 0 .. 1.0 s
    parts = []
    last = None
    held = None  # the last keyframe skipped as unchanged: written out when the value moves again, so it holds until then
    for t in steps:
        v = fmt.format(fn(t))
        pct = t / total * 100
        frame = "{:g}% {{ {} }}".format(round(pct, 3), prop.format(v + unit))
        if v == last and t not in (0.0, 1.0):
            held = frame
            continue
        if held is not None:
            parts.append(held)
            held = None
        parts.append(frame)
        last = v
    parts.append("100% {{ {} }}".format(prop.format(fmt.format(fn(1.0)) + unit)))
    lines = ["@keyframes {} {{".format(name)]
    for i in range(0, len(parts), 4):
        lines.append("  " + " ".join(parts[i:i + 4]))
    lines.append("}")
    return "\n".join(lines)


blocks = [
    keyframes("play-lift", "transform: translateY({});", lift, "dp"),
    keyframes("play-spin", "transform: rotate({});", spin, "deg", "{:.2f}"),
    keyframes("play-sharp", "opacity: {};", sharp, ""),
    keyframes("play-ghost-op", "opacity: {};", ghost_opacity, ""),
    keyframes("play-ghost1", "transform: rotate({});", ghost_turn(-8), "deg", "{:.2f}"),
    keyframes("play-ghost2", "transform: rotate({});", ghost_turn(-16), "deg", "{:.2f}"),
    keyframes("play-ghost3", "transform: rotate({});", ghost_turn(-24), "deg", "{:.2f}"),
    keyframes("play-smear", "opacity: {};", smear, ""),
    keyframes("play-black", "opacity: {};", black, ""),
]

# Play as a held button (snatcherLeo, 2026-11-13, mockup 44 take 3, "yep, that's perfect"): holding A / PLAY bounces the disc
# down and keeps it pressed in (hitch-down, 0.22 s: 7 px down at the deepest, settling at 4 px, scale 0.975 -> 0.985,
# cubic-bezier(0.3, 0, 0.2, 1)); letting go snaps it up to the lift's -6 px in 45 ms (cubic-bezier(0.22, 1, 0.36, 1))
# and the rest of the Play runs 0.11 s sooner, so the spin follows right behind (from 0.04 s instead of 0.15 s).
PRESS = bezier(0.3, 0, 0.2, 1)
SHIFT = 0.11


def pressed(t):  # (dy, scale) of the bounce down over 0.22 s: 0 -> (7, 0.975) at 55% -> (4, 0.985)
    u = PRESS(min(1.0, max(0.0, t / 0.22)))
    if u <= 0.55:
        k = u / 0.55
        return 7 * k, 1 - 0.025 * k
    k = (u - 0.55) / 0.45
    return 7 - 3 * k, 0.975 + 0.010 * k


def snap(t):  # (dy, scale) from the pressed pose (4, 0.985) to the lift (-6, 1) in 45 ms
    k = segment(t, 0, 0.045, LIFT)
    return 4 - 10 * k, 0.985 + 0.015 * k


def pose_keyframes(name, fn, length, step):
    """keyframes of translateY + scale over `length` s, sampled every `step` s, the animation lasting `length` s"""
    lines = ["@keyframes {} {{".format(name)]
    parts = []
    n = int(round(length / step))
    for i in range(n + 1):
        t = i * step
        dy, s = fn(t)
        parts.append("{:g}% {{ transform: translateY({:.3f}dp) scale({:.4f}); }}".format(round(t / length * 100, 3), dy, s))
    for i in range(0, len(parts), 3):
        lines.append("  " + " ".join(parts[i:i + 3]))
    lines.append("}")
    return "\n".join(lines)


def snap_lift(t):  # the snap within the 2 s timeline, then held up
    dy, s = snap(t)
    return dy, s


def shifted(fn):
    return lambda t: fn(t + SHIFT)


snap_blocks = [
    pose_keyframes("hitch-down", pressed, 0.22, 0.02),
    "@keyframes snap-lift {\n  " + " ".join(
        "{:g}% {{ transform: translateY({:.3f}dp) scale({:.4f}); }}".format(round(t / 2 * 100, 3), *snap(t))
        for t in [i * 0.005 for i in range(10)]) + "\n  100% { transform: translateY(-6.000dp) scale(1.0000); }\n}",
    keyframes("snap-spin", "transform: rotate({});", shifted(spin), "deg", "{:.2f}"),
    keyframes("snap-sharp", "opacity: {};", shifted(sharp), ""),
    keyframes("snap-ghost-op", "opacity: {};", shifted(ghost_opacity), ""),
    keyframes("snap-ghost1", "transform: rotate({});", shifted(ghost_turn(-8)), "deg", "{:.2f}"),
    keyframes("snap-ghost2", "transform: rotate({});", shifted(ghost_turn(-16)), "deg", "{:.2f}"),
    keyframes("snap-ghost3", "transform: rotate({});", shifted(ghost_turn(-24)), "deg", "{:.2f}"),
    keyframes("snap-smear", "opacity: {};", shifted(smear), ""),
    keyframes("snap-black", "opacity: {};", shifted(black), ""),
]
import sys

if len(sys.argv) > 1 and sys.argv[1] == "--snap":  # only the held-button blocks (2026-11-13)
    print("\n".join(snap_blocks))
else:
    print("\n".join(blocks + snap_blocks))
