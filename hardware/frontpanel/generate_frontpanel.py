"""Generates the initial front-panel board: frontpanel.kicad_pcb (+ project file).

Run with the Python interpreter bundled with KiCad 10 (it provides the pcbnew
module), found in the bin directory of the KiCad installation:

    <KiCad install dir>/bin/python generate_frontpanel.py

This is a one-shot starting point. Once the board has been edited by hand in
KiCad, don't re-run it - it overwrites frontpanel.kicad_pcb.

All coordinates below are in mm, in panel coordinates: origin at the top-left
corner of the panel, x to the right, y down, as seen from the FRONT (user side).
"""
import math
import os

import pcbnew as p

HERE = os.path.dirname(os.path.abspath(__file__))
PCB_PATH = os.path.join(HERE, "frontpanel.kicad_pcb")
FP_DIR = os.environ.get(
    "KICAD10_FOOTPRINT_DIR",
    os.path.normpath(os.path.join(os.path.dirname(p.__file__), "..", "..", "..", "share", "kicad", "footprints")),
)

# Where the panel sits on the KiCad sheet
SHEET_X, SHEET_Y = 20.0, 40.0

# --- Panel -------------------------------------------------------------------
PANEL_W, PANEL_H = 253.0, 60.0
PANEL_THICKNESS = 2.0
SLOT_DEPTH = 2.0            # hidden in the enclosure slots, all four edges assumed
COPPER_EDGE_CLEARANCE = 1.5 # keeps logic GND away from the (PE-earthed) enclosure
CY = PANEL_H / 2            # common vertical centre line

# --- Part centres (x), left to right ------------------------------------------
# WIO towards the switch and the XLR towards the right edge, leaving ~30 mm of
# free space on either side of the Twist board (room for the debug connector)
SWITCH_X = 30.5
WIO_X = 90.0
KNOB_X = 172.0
XLR_X = 228.0

# --- Mains push button ------------------------------------------------------------
# ASSUMPTION: the 10 mm round cap is what passes through the panel
SWITCH_HOLE_D = 10.5
SWITCH_SCREW_DX = 10.0        # two M3 screws, 20 mm apart horizontally
SWITCH_KEEPOUT_MARGIN = 8.0   # no copper within this distance of the mains part
KEEPOUT_CORNER_RADIUS = 4.0   # rounded corners of the copper-free areas (switch, XLR)
# Copper-free circle around the WIO and Twist mounting holes, so a screw head,
# washer or spacer can't reach the copper even if it scratches the soldermask:
# typical pan head + washer, plus margin
SCREW_KEEPOUT_D = {"M2": 6.0, "M3": 8.0}

HALF_PITCH = 2.54 / 2         # half the 40-pin header pitch

# --- WIO Terminal (from Seeed's back-cover drawing, v3.0) ----------------------
WIO_BODY = (72.0, 57.0)
WIO_SCREW_DX = 30.5          # two M2 holes at mid height, +/-30.5 mm from centre
WIO_HEADER_DY = -15.5        # 40-pin header centre, towards the top edge
# Verified on the device: seen from the front, pin 1 is bottom-left, pin 2 top-left;
# odd pins are the bottom row.
WIO_PIN1_LEFT = True
WIO_ODD_ROW_TOP = False

# --- SparkFun Qwiic Twist (from SparkFun's board file) ------------------------
# Hangs behind the panel, encoder towards the panel. Its encoder has no threaded
# bushing, so the board is held by 4 M3 screws through the panel (with spacers
# between panel and Twist); the socket carries the signals. The encoder sits on
# the Twist's BOTTOM side (SW1 is mirrored in SparkFun's board file), so seen
# from the front we look at the Twist's bottom: SparkFun's top view mirrored
# left-right.
TWIST_BOARD = (30.48, 25.4)       # encoder is in the centre of the board
TWIST_HOLE_DX, TWIST_HOLE_DY = 12.7, 10.16
TWIST_PIN_ROW_DY = 11.43          # pin row below the encoder
# Pin row in SparkFun's top view, left to right (x = 8.89 ... 21.59 mm);
# reversed below to get the order as seen from the front.
_TWIST_PINS_TOP_VIEW = ["GND", "+3V3", "I2C_SDA", "I2C_SCL", "TWIST_INT", "TWIST_RST"]
_TWIST_LABELS_TOP_VIEW = ["GND", "3.3V", "SDA", "SCL", "~{INT}", "~{RST}"]  # silkscreen, ~{} = overbar
TWIST_PINS = _TWIST_PINS_TOP_VIEW[::-1]
TWIST_PIN_LABELS = _TWIST_LABELS_TOP_VIEW[::-1]
ENCODER_HOLE_D = 7.5              # PLACEHOLDER - clearance for the 6 mm shaft (no bushing)

# --- XLR 3-pin female ------------------------------------------------------------
XLR_HOLE_D = 22.4             # body measured 22 mm
# M3 holes top-left and bottom-right: 28 mm apart centre to centre, each 10 mm
# horizontally from the (horizontally centred) middle pin -> 20 mm apart in x,
# sqrt(28^2 - 20^2) = 19.6 mm apart in y.
XLR_SCREW_DX = 10.0
XLR_SCREW_DY = (28.0 ** 2 - (2 * XLR_SCREW_DX) ** 2) ** 0.5 / 2
XLR_SCREWS = [(-XLR_SCREW_DX, -XLR_SCREW_DY), (XLR_SCREW_DX, XLR_SCREW_DY)]
# The bottom-right screw connects to the connector chassis, so the flange and
# the screws/nuts must not touch panel copper: no copper under the whole area.
XLR_KEEPOUT_HALF = max(XLR_SCREW_DX, XLR_SCREW_DY) + 4.0

# --- Connector to the soldering iron PCB (4-pin JST-PH, SMD, back side) ---------
# Side entry (S4B-PH-SM4-TB): the cable leaves parallel to the panel, here to the
# right as seen from the front - towards the Twist/XLR, away from the mains button.
BACK_CONN_POS = (WIO_X + 6.0, 44.0)  # behind the WIO, well left of the Twist for the cable
BACK_CONN_EXIT = (1, 0)               # cable exit direction (x, y), panel coordinates
# pin: (net, silkscreen label); GND between the heater PWM and the tip ADC signal.
# Order matches the routing: the tracks arrive from the header, left to right,
# as 5V, HEAT, TEMP, so pin 1 (top) is TEMP and pin 4 (bottom) is 5V.
BACK_CONN_PINS = {1: ("TIP_ADC", "TEMP"), 2: ("GND", "GND"), 3: ("HEATER_PWM", "HEATER PWM"), 4: ("+5V", "5V")}
# Signal direction arrows next to the labels: +1 = out through the cable (to the
# soldering iron PCB), -1 = in from the cable (to the WIO)
BACK_CONN_ARROWS = {"HEATER_PWM": +1, "TIP_ADC": -1}
BACK_CONN_TITLE = "J3: IRON PCB"

# --- Debug connector (same 4-pin JST-PH, back side) -------------------------------
# Between the WIO and the Twist, near the top edge, so the cable (to the right)
# passes above the Twist board. Its tracks come over the top of the header,
# above RST, from unused top-row pins at the header's right end.
RST_OVER_Y = -1.63      # RST's run over pin 40, relative to the top header row
DEBUG_PITCH = 1.2       # between the nested debug tracks, above RST
_debug_top_track_y = CY + WIO_HEADER_DY - 2.54 / 2 + RST_OVER_Y - 4 * DEBUG_PITCH
# x: labels clear of the WIO outline; y: pin 1 (3 mm above the centre) in line
# with the highest debug track
DEBUG_CONN_POS = (WIO_X + WIO_BODY[0] / 2 + 10.0, _debug_top_track_y + 3.0)
DEBUG_CONN_EXIT = (1, 0)
# pin: (net, silkscreen label, WIO header pin). Order matches the routing (no
# crossings): the further left the header pin, the higher the connector pin.
DEBUG_CONN_PINS = {1: ("DBG0", "DBG0", 26), 2: ("DBG1", "DBG1", 32),
                   3: ("GND", "GND", 34), 4: ("DBG2", "DBG2", 36)}
DEBUG_CONN_TITLE = "J4: DEBUG"

# --- Silkscreen: front texts, back outlines of the front parts, credits ---------
# Outline sizes marked ~ are estimates; the parts' real flanges weren't measured
SWITCH_FLANGE = (27.0, 14.0)  # ~ around the two M3 screws
KNOB_D = 16.0                 # ~ no knob chosen yet
XLR_FLANGE = (26.0, 26.0)     # ~ square flange around the diagonal M3 holes
CREDITS = ["github.com/tzijnge/SolderingStation", "Timon Zijnge 2026"]
REPO_URL = "https://github.com/tzijnge/SolderingStation"
QR_SIZE, QR_MARGIN = 16.0, 1.5  # QR code on the back, plus its white quiet zone

# WIO 40-pin header: BCM number or power rail per physical pin, from Seeed's
# WIO Terminal schematic (v1.2, header J6) - same as a Raspberry Pi. Note: the
# pin table comment in the WIO's variant.h wrongly lists pin 17 as GND; the
# schematic has it on 3.3V.
WIO_HEADER = {
    1: "3V3", 2: "5V", 3: "BCM2", 4: "5V", 5: "BCM3", 6: "GND", 7: "BCM4", 8: "BCM14",
    9: "GND", 10: "BCM15", 11: "BCM17", 12: "BCM18", 13: "BCM27", 14: "GND", 15: "BCM22",
    16: "BCM23", 17: "3V3", 18: "BCM24", 19: "BCM10", 20: "GND", 21: "BCM9", 22: "BCM25",
    23: "BCM11", 24: "BCM8", 25: "GND", 26: "BCM7", 27: "BCM0", 28: "BCM1", 29: "BCM5",
    30: "GND", 31: "BCM6", 32: "BCM12", 33: "BCM13", 34: "GND", 35: "BCM19", 36: "BCM16",
    37: "BCM26", 38: "BCM20", 39: "GND", 40: "BCM21",
}
# Soldering station use of the header pins (see src/main.cpp):
# net name, silkscreen label
WIO_FUNCTIONS = {
    3: ("I2C_SDA", "SDA"),          # Wire, Qwiic Twist
    5: ("I2C_SCL", "SCL"),          # Wire, Qwiic Twist
    16: ("HEATER_PWM", "HEAT"),     # CounterPwmOutput pwmOutput(BCM23)
    18: ("TIP_ADC", "TEMP"),        # AdcInput adcInput(A3), A3 = BCM24
    26: ("DBG0", "DBG0"),           # debug connector J4
    32: ("DBG1", "DBG1"),           # debug connector J4
    36: ("DBG2", "DBG2"),           # debug connector J4
    38: ("TWIST_RST", "~{RST}"),    # Twist RST, active low - not used by the firmware (yet)
    40: ("TWIST_INT", "~{INT}"),    # EncoderTask, Twist INT, active low
}
_RAIL_NETS = {"3V3": "+3V3", "5V": "+5V", "GND": "GND"}
# Nets on the used pins: the power rails, plus the functional pins. Pin 17
# (3.3V) stays unconnected on the panel: pin 1 already supplies the Twist, and
# the WIO joins the two internally.
WIO_PINS = {pin: _RAIL_NETS[name] for pin, name in WIO_HEADER.items() if name in _RAIL_NETS and pin != 17}
WIO_PINS.update({pin: net_name for pin, (net_name, _) in WIO_FUNCTIONS.items()})


def mm(v):
    return p.FromMM(v)


def pt(x, y):
    return p.VECTOR2I(mm(SHEET_X + x), mm(SHEET_Y + y))


board = p.NewBoard(PCB_PATH)
nets = {}


def net(name):
    if name not in nets:
        nets[name] = p.NETINFO_ITEM(board, name)
        board.Add(nets[name])
    return nets[name]


def rect(x0, y0, x1, y1, layer, width=0.1):
    s = p.PCB_SHAPE(board)
    s.SetShape(p.SHAPE_T_RECTANGLE)
    s.SetStart(pt(x0, y0))
    s.SetEnd(pt(x1, y1))
    s.SetLayer(layer)
    s.SetWidth(mm(width))
    board.Add(s)


def centred_rect(cx, cy, w, h, layer, width=0.1):
    rect(cx - w / 2, cy - h / 2, cx + w / 2, cy + h / 2, layer, width)


def circle(cx, cy, d, layer, width=0.1):
    s = p.PCB_SHAPE(board)
    s.SetShape(p.SHAPE_T_CIRCLE)
    s.SetCenter(pt(cx, cy))
    s.SetEnd(pt(cx + d / 2, cy))
    s.SetLayer(layer)
    s.SetWidth(mm(width))
    board.Add(s)


def line(x0, y0, x1, y1, layer, width=0.15):
    s = p.PCB_SHAPE(board)
    s.SetShape(p.SHAPE_T_SEGMENT)
    s.SetStart(pt(x0, y0))
    s.SetEnd(pt(x1, y1))
    s.SetLayer(layer)
    s.SetWidth(mm(width))
    board.Add(s)


def arrow_head(x, y, ux, uy, layer, head=0.8, width=0.15):
    """Arrow head at (x, y), pointing in unit direction (ux, uy)."""
    for side in (-1, 1):  # two strokes at +/-30 degrees
        hx = -ux * 0.866 - side * uy * 0.5
        hy = -uy * 0.866 + side * ux * 0.5
        line(x, y, x + head * hx, y + head * hy, layer, width)


def arrow(x0, y0, x1, y1, layer, head=0.8):
    """Arrow from (x0, y0) to (x1, y1), head at the end."""
    line(x0, y0, x1, y1, layer)
    length = ((x1 - x0) ** 2 + (y1 - y0) ** 2) ** 0.5
    arrow_head(x1, y1, (x1 - x0) / length, (y1 - y0) / length, layer, head)


def text(s, x, y, layer=p.Cmts_User, size=1.2, angle=0.0, justify=None):
    t = p.PCB_TEXT(board)
    t.SetText(s)
    t.SetPosition(pt(x, y))
    t.SetLayer(layer)
    t.SetTextSize(p.VECTOR2I(mm(size), mm(size)))
    t.SetTextThickness(mm(size * 0.15))
    t.SetTextAngleDegrees(angle)
    if justify is not None:
        t.SetHorizJustify(justify)
    # Text on the back must be mirrored to read correctly from behind
    t.SetMirrored(p.IsBackLayer(layer))
    board.Add(t)


def pin_label(label, x, y, direction, layer, size=0.8):
    """Pin label starting at (x, y) and running away from the pin in direction
    (x, y) - vertical text for up/down, horizontal text for left/right."""
    vertical = direction[1] != 0
    # Text reads left-to-right (horizontal) or bottom-to-top (vertical, 90 deg)
    forwards = direction[0] > 0 if not vertical else direction[1] < 0
    # Mirrored (back) text runs the other way, so its alignment flips too
    starts_at_anchor = forwards != p.IsBackLayer(layer)
    justify = p.GR_TEXT_H_ALIGN_LEFT if starts_at_anchor else p.GR_TEXT_H_ALIGN_RIGHT
    text(label, x, y, layer, size=size, angle=90 if vertical else 0, justify=justify)


def footprint(lib, name, ref, x, y, angle=0.0, back=False):
    fp = p.FootprintLoad(os.path.join(FP_DIR, lib + ".pretty"), name)
    fp.SetReference(ref)
    board.Add(fp)
    fp.SetOrientationDegrees(angle)
    if back:
        fp.Flip(fp.GetPosition(), p.FLIP_DIRECTION_LEFT_RIGHT)
    # Centre on the pads rather than the footprint origin (THT headers have
    # their origin on pin 1)
    pads = fp.Pads()
    cx = sum(pd.GetPosition().x for pd in pads) / len(pads)
    cy = sum(pd.GetPosition().y for pd in pads) / len(pads)
    target = pt(x, y)
    fp.Move(p.VECTOR2I(int(target.x - cx), int(target.y - cy)))
    # Nothing but the artwork may show on the front of the panel
    if not back:
        fp.Reference().SetVisible(False)
    fp.Value().SetVisible(False)
    return fp


# --- Board setup ---------------------------------------------------------------
ds = board.GetDesignSettings()
ds.SetBoardThickness(mm(PANEL_THICKNESS))
ds.m_CopperEdgeClearance = mm(COPPER_EDGE_CLEARANCE)
# The back GND pour is cut into pieces by the tracks; some pieces reach a THT GND
# header pin with only one thermal spoke. Fine here: those pins also connect to
# the unbroken front GND pour.
ds.m_MinResolvedSpokes = 1
tb = board.GetTitleBlock()
tb.SetTitle("Soldering station front panel")
tb.SetRevision("0.1")

# --- Outline and reference drawings ----------------------------------------------
rect(0, 0, PANEL_W, PANEL_H, p.Edge_Cuts)
rect(SLOT_DEPTH, SLOT_DEPTH, PANEL_W - SLOT_DEPTH, PANEL_H - SLOT_DEPTH, p.Dwgs_User)
text("hidden in enclosure slots (2 mm) - no artwork", PANEL_W / 2, 1.0, size=0.8)

def rounded_rect_points(cx, cy, half_w, half_h, radius, steps=8):
    """Corner points of a rectangle with rounded corners, arcs as short segments."""
    points = []
    corners = ((half_w, half_h, 0), (-half_w, half_h, 90), (-half_w, -half_h, 180), (half_w, -half_h, 270))
    for sx, sy, start in corners:
        # centre of this corner's arc, then sweep 90 degrees around it
        ax = cx + sx - math.copysign(radius, sx)
        ay = cy + sy - math.copysign(radius, sy)
        for i in range(steps + 1):
            a = math.radians(start + 90 * i / steps)
            points.append((ax + radius * math.cos(a), ay + radius * math.sin(a)))
    return points


def circle_points(cx, cy, d, steps=32):
    return [(cx + d / 2 * math.cos(2 * math.pi * i / steps), cy + d / 2 * math.sin(2 * math.pi * i / steps))
            for i in range(steps)]


def copper_keepout(name, cx, cy, half_w, half_h, radius=KEEPOUT_CORNER_RADIUS, points=None, pour_only=False):
    """No tracks, vias or pours on either copper layer in this rounded rectangle
    (or in the given outline points); with pour_only, tracks and vias are allowed."""
    keepout = p.ZONE(board)
    keepout.SetIsRuleArea(True)
    keepout.SetZoneName(name)
    layers = p.LSET()
    layers.AddLayer(p.F_Cu)
    layers.AddLayer(p.B_Cu)
    keepout.SetLayerSet(layers)
    keepout.SetDoNotAllowTracks(not pour_only)
    keepout.SetDoNotAllowVias(not pour_only)
    keepout.SetDoNotAllowZoneFills(True)
    # The parts' own (NPTH) mounting holes are meant to be in here
    keepout.SetDoNotAllowPads(False)
    keepout.SetDoNotAllowFootprints(False)
    ko = keepout.Outline()
    ko.NewOutline()
    for x, y in points or rounded_rect_points(cx, cy, half_w, half_h, radius):
        v = pt(x, y)
        ko.Append(v.x, v.y)
    board.Add(keepout)


def screw_hole(ref, size, x, y):
    """NPTH mounting hole with a copper-free circle around it on both layers."""
    footprint("MountingHole", "MountingHole_%s_%s" % ({"M2": "2.2mm", "M3": "3.2mm"}[size], size), ref, x, y)
    d = SCREW_KEEPOUT_D[size]
    copper_keepout(ref + "_screw_keepout", x, y, d / 2, d / 2, points=circle_points(x, y, d))


# --- Mains push button ------------------------------------------------------------------
circle(SWITCH_X, CY, SWITCH_HOLE_D, p.Edge_Cuts)
text("MAINS", SWITCH_X, CY - SWITCH_HOLE_D / 2 - 2)
for i, dx in enumerate((-SWITCH_SCREW_DX, SWITCH_SCREW_DX)):
    footprint("MountingHole", "MountingHole_3.2mm_M3", "H%d" % (i + 5), SWITCH_X + dx, CY)
copper_keepout("mains_switch_keepout", SWITCH_X, CY,
               SWITCH_SCREW_DX + 3.0 + SWITCH_KEEPOUT_MARGIN, SWITCH_HOLE_D / 2 + SWITCH_KEEPOUT_MARGIN)

# --- WIO Terminal --------------------------------------------------------------------
centred_rect(WIO_X, CY, *WIO_BODY, p.Dwgs_User)
text("WIO TERMINAL (mounted on front)", WIO_X, CY + WIO_BODY[1] / 2 - 4)
for i, dx in enumerate((-WIO_SCREW_DX, WIO_SCREW_DX)):
    screw_hole("H%d" % (i + 1), "M2", WIO_X + dx, CY)

hy = CY + WIO_HEADER_DY
j1 = footprint("Connector_PinHeader_2.54mm", "PinHeader_2x20_P2.54mm_Vertical", "J1", WIO_X, hy, angle=90)
# Renumber the pads to the WIO's pin numbers by their physical position, so pad
# numbers match the WIO pinout no matter how the footprint ended up oriented.
left_x = min(pd.GetPosition().x for pd in j1.Pads())
for pd in j1.Pads():
    pos = pd.GetPosition()
    col = round((pos.x - left_x) / mm(2.54))
    if not WIO_PIN1_LEFT:
        col = 19 - col
    top_row = pos.y < pt(0, hy).y
    pin = 2 * col + (1 if top_row == WIO_ODD_ROW_TOP else 2)
    pd.SetNumber(str(pin))
    pd.SetShape(p.PAD_SHAPE_RECT if pin == 1 else p.PAD_SHAPE_CIRCLE)
    if pin in WIO_PINS:
        pd.SetNet(net(WIO_PINS[pin]))
    # Pin labels, vertical, pointing away from the header: the BCM number or
    # power rail next to the pin, the soldering station function further out.
    # On both sides: the front ones are hidden behind the WIO once mounted, the
    # back ones are where the pins can be probed with the WIO attached.
    top_row = pos.y < pt(0, hy).y
    side = -1 if top_row else 1              # away from the header
    x = p.ToMM(pos.x) - SHEET_X
    labels = [(WIO_HEADER[pin], 2.3)]
    if pin in WIO_FUNCTIONS:
        labels.append((WIO_FUNCTIONS[pin][1], 7.5))
    for label, offset in labels:
        for layer in (p.F_SilkS, p.B_SilkS):
            pin_label(label, x, hy + side * (1.27 + offset), (0, side), layer)
# No pour between the two header rows: it only leaves ~1 mm2 slivers in the gaps
# between four pads. The GND pins connect to the pour above and below the header.
header_xs = [p.ToMM(pd.GetPosition().x) - SHEET_X for pd in j1.Pads()]
copper_keepout("header_no_pour", (min(header_xs) + max(header_xs)) / 2, hy,
               (max(header_xs) - min(header_xs)) / 2 + HALF_PITCH, HALF_PITCH, radius=0.0,
               points=[(min(header_xs) - HALF_PITCH, hy - HALF_PITCH), (max(header_xs) + HALF_PITCH, hy - HALF_PITCH),
                       (max(header_xs) + HALF_PITCH, hy + HALF_PITCH), (min(header_xs) - HALF_PITCH, hy + HALF_PITCH)],
               pour_only=True)
# Reference left of the header, on both sides; on the front it's hidden behind
# the WIO anyway, so it may show there
j1_label_x = p.ToMM(min(pd.GetPosition().x for pd in j1.Pads())) - SHEET_X - 2.8
j1.Reference().SetVisible(True)
j1.Reference().SetPosition(pt(j1_label_x, hy))
text("J1", j1_label_x, hy, p.B_SilkS, size=1.0)

# --- Qwiic Twist ---------------------------------------------------------------------
circle(KNOB_X, CY, ENCODER_HOLE_D, p.Edge_Cuts)
centred_rect(KNOB_X, CY, *TWIST_BOARD, p.Dwgs_User)
text("QWIIC TWIST (behind panel)", KNOB_X, CY - TWIST_BOARD[1] / 2 - 1.5, size=0.8)
text("encoder hole: placeholder", KNOB_X, CY + 6, size=0.7)
# M3 holes for the Twist's four mounting screws
for i, (sx, sy) in enumerate(((-1, -1), (1, -1), (-1, 1), (1, 1))):
    screw_hole("H%d" % (i + 7), "M3", KNOB_X + sx * TWIST_HOLE_DX, CY + sy * TWIST_HOLE_DY)

j2 = footprint("Connector_PinSocket_2.54mm", "PinSocket_1x06_P2.54mm_Vertical_SMD_Pin1Left", "J2",
               KNOB_X, CY + TWIST_PIN_ROW_DY, angle=90, back=True)
for i, pd in enumerate(sorted(j2.Pads(), key=lambda pd: pd.GetPosition().x)):
    pd.SetNumber(str(i + 1))
    if TWIST_PINS[i]:
        pd.SetNet(net(TWIST_PINS[i]))
# Pin labels as on the Twist itself; below the socket, outside the Twist's
# outline so they stay readable with the Twist plugged in
for i, label in enumerate(TWIST_PIN_LABELS):
    text(label, KNOB_X + (i - 2.5) * 2.54, CY + TWIST_PIN_ROW_DY + 5.5, p.B_SilkS, size=1.0, angle=90)

# --- XLR -------------------------------------------------------------------------------
circle(XLR_X, CY, XLR_HOLE_D, p.Edge_Cuts)
text("XLR 3-pin", XLR_X, CY + XLR_KEEPOUT_HALF + 1.5, size=0.8)
for i, (dx, dy) in enumerate(XLR_SCREWS):
    footprint("MountingHole", "MountingHole_3.2mm_M3", "H%d" % (i + 3), XLR_X + dx, CY + dy)
copper_keepout("xlr_chassis_keepout", XLR_X, CY, XLR_KEEPOUT_HALF, XLR_KEEPOUT_HALF)

# --- JST connectors: to the soldering iron PCB, debug ----------------------------------
def cable_direction(fp):
    """Unit direction the cable leaves in: from the signal pads towards the
    mounting pads (the opening is on the far side of the housing)."""
    def centre(pads):
        return (sum(pd.GetPosition().x for pd in pads) / len(pads),
                sum(pd.GetPosition().y for pd in pads) / len(pads))
    sig = centre([pd for pd in fp.Pads() if pd.GetNumber() != "MP"])
    mp = centre([pd for pd in fp.Pads() if pd.GetNumber() == "MP"])
    dx, dy = mp[0] - sig[0], mp[1] - sig[1]
    return (int(dx > 0) - int(dx < 0), 0) if abs(dx) > abs(dy) else (0, int(dy > 0) - int(dy < 0))


JST_LIB, JST_NAME = "Connector_JST", "JST_PH_S4B-PH-SM4-TB_1x04-1MP_P2.00mm_Horizontal"
ARROW_ROOM = 3.5  # between the housing and the labels, when a connector has arrows


def place_jst(ref, pos, exit_dir, pins, title, arrows=None, title_below=False):
    """Side-entry JST-PH on the back, cable leaving in exit_dir. pins maps pin
    number to (net, label, ...); arrows maps net to +1 (out through the cable)
    or -1 (in from the cable). Title above (or below) the housing, pin labels
    on the signal-pad side, opposite the cable."""
    arrows = arrows or {}
    # Find the rotation that points the cable the right way on a loose copy -
    # adding and removing footprints on the board upsets the zone filler
    for angle in (0, 90, 180, 270):
        probe = p.FootprintLoad(os.path.join(FP_DIR, JST_LIB + ".pretty"), JST_NAME)
        probe.SetOrientationDegrees(angle)
        # Flipping to the back mirrors left-right (Flip() needs a board, so do it by hand)
        dx, dy = cable_direction(probe)
        if (-dx, dy) == exit_dir:
            break
    fp = footprint(JST_LIB, JST_NAME, ref, *pos, angle=angle, back=True)
    away = (-exit_dir[0], -exit_dir[1])
    body = fp.GetBoundingBox(False)
    fp.Reference().SetVisible(False)  # part of the title instead
    title_y = p.ToMM(body.GetBottom()) - SHEET_Y + 1.2 if title_below else p.ToMM(body.GetY()) - SHEET_Y - 1.2
    text(title, p.ToMM(body.GetCenter().x) - SHEET_X, title_y, p.B_SilkS, size=1.0)
    edge = {(-1, 0): body.GetX(), (1, 0): body.GetRight(), (0, -1): body.GetY(), (0, 1): body.GetBottom()}[away]
    edge = p.ToMM(edge) - (SHEET_X if away[0] else SHEET_Y)
    room = ARROW_ROOM if arrows else 0.0
    for pd in fp.Pads():
        if pd.GetNumber() == "MP":
            continue
        net_name, label = pins[int(pd.GetNumber())][:2]
        pd.SetNet(net(net_name))
        x, y = p.ToMM(pd.GetPosition().x) - SHEET_X, p.ToMM(pd.GetPosition().y) - SHEET_Y
        # Positions along the label direction: near/far end of the arrow, label
        near, far = edge + 0.5 * sum(away), edge + (room - 0.5) * sum(away)
        label_at = edge + (room + 0.5) * sum(away)
        pin_label(label, label_at if away[0] else x, y if away[0] else label_at, away, p.B_SilkS)
        if net_name in arrows:
            # Out through the cable: arrow points at the connector; in: away from it
            start, end = (far, near) if arrows[net_name] > 0 else (near, far)
            if away[0]:
                arrow(start, y, end, y, p.B_SilkS)
            else:
                arrow(x, start, x, end, p.B_SilkS)
    return fp


j3 = place_jst("J3", BACK_CONN_POS, BACK_CONN_EXIT, BACK_CONN_PINS, BACK_CONN_TITLE, BACK_CONN_ARROWS)
j4 = place_jst("J4", DEBUG_CONN_POS, DEBUG_CONN_EXIT, DEBUG_CONN_PINS, DEBUG_CONN_TITLE,
               title_below=True)  # no room above it, near the top edge

# --- Tracks: back layer only ----------------------------------------------------------------
# Traces on the front would show through the black soldermask, so all routing
# is on B.Cu, and a rule area forbids tracks and vias on F.Cu. The THT header
# pads are the only connections that go through the board.
SIGNAL_W, POWER_W = 0.3, 0.6
HALF = 2.54 / 2   # half the header pitch: the gaps between the header's pads
BUS_Y = {"I2C_SCL": 52.5, "I2C_SDA": 54.0, "+3V3": 55.5}  # below the JST, to the Twist
CHAMFER = 1.0     # right-angle corners become 45 degree chamfers of this length
# INT and RST both leave the header at its right end, so neither runs between
# the header pads. INT goes down to just above the Twist bus and into the socket
# from below; RST goes over pin 40, down on its right and into the socket from
# above, clear of the encoder hole and of the Twist's mounting screws.
INT_X_OFFSET = 1.37     # INT's vertical, right of pin 40
RST_X_OFFSET = 2.57     # RST's vertical, right of INT's
INT_BUS_Y = 51.0
RST_Y = 34.5


def pad_at(fp, number):
    pos = [pd for pd in fp.Pads() if pd.GetNumber() == str(number)][0].GetPosition()
    return p.ToMM(pos.x) - SHEET_X, p.ToMM(pos.y) - SHEET_Y


def chamfered(points, widths, sharp=()):
    """Replaces every right-angle corner with a 45 degree chamfer, except at the
    point indices in sharp (corners squeezed between pads)."""
    out_points, out_widths = [points[0]], []
    for i in range(1, len(points) - 1):
        (x0, y0), (x1, y1), (x2, y2) = points[i - 1], points[i], points[i + 1]
        ax, ay, bx, by = x1 - x0, y1 - y0, x2 - x1, y2 - y1
        la, lb = math.hypot(ax, ay), math.hypot(bx, by)
        if i not in sharp and la and lb and abs(ax * bx + ay * by) < 1e-9:
            c = min(CHAMFER, la / 2, lb / 2)
            out_points += [(x1 - ax / la * c, y1 - ay / la * c), (x1 + bx / lb * c, y1 + by / lb * c)]
            out_widths += [widths[i - 1], min(widths[i - 1], widths[i])]
        else:
            out_points.append((x1, y1))
            out_widths.append(widths[i - 1])
    out_points.append(points[-1])
    out_widths.append(widths[-1])
    return out_points, out_widths


def route(net_name, points, widths, sharp=()):
    """Track through points on B.Cu, 45 degree corners (except at the point
    indices in sharp); widths is one width or one per segment."""
    if not isinstance(widths, (list, tuple)):
        widths = [widths] * (len(points) - 1)
    # Drop zero-length segments (a jog that happens to be 0 mm)
    kept = [i for i in range(len(points) - 1) if math.dist(points[i], points[i + 1]) > 1e-6]
    sharp = tuple(kept.index(i) for i in sharp if i in kept)
    points, widths = [points[i] for i in kept] + [points[-1]], [widths[i] for i in kept]
    points, widths = chamfered(points, widths, sharp)
    for (x0, y0), (x1, y1), w in zip(points, points[1:], widths):
        if math.dist((x0, y0), (x1, y1)) < 1e-6:
            continue  # two chamfers that used up a short segment between them
        t = p.PCB_TRACK(board)
        t.SetStart(pt(x0, y0))
        t.SetEnd(pt(x1, y1))
        t.SetWidth(mm(w))
        t.SetLayer(p.B_Cu)
        t.SetNet(net(net_name))
        board.Add(t)


# To the JST: the tracks leave the header between its pads and enter the JST
# pads from the left. 5V, HEAT and TEMP end up 5 mm apart, with GND pour between.
x, y = pad_at(j1, 4)
jx, jy = pad_at(j3, 4)
route("+5V", [pad_at(j1, 2), (x, y), (x + HALF, y + HALF), (x + 3 * HALF, y + HALF),
              (x + 3 * HALF, y + 4.3), (x + 3 * HALF, jy), (jx, jy)],
      [SIGNAL_W] * 4 + [POWER_W] * 2,  # narrow between the header pads only
      sharp=(3,))  # the turn down between pins 5 and 7: a chamfer would cut into pin 5
x, y = pad_at(j1, 16)
jx, jy = pad_at(j3, 3)
route("HEATER_PWM", [(x, y), (x - HALF, y + HALF), (x - HALF, jy), (jx, jy)], SIGNAL_W)
x, y = pad_at(j1, 18)
jx, jy = pad_at(j3, 1)
route("TIP_ADC", [(x, y), (x + HALF, y + HALF), (x + HALF, jy), (jx, jy)], SIGNAL_W)

# To the Twist: 3.3V, SDA and SCL straight down from the header, along the
# bottom under the JST, and up into the socket between its staggered pads
for header_pin, socket_pin, net_name in ((1, 5, "+3V3"), (3, 4, "I2C_SDA"), (5, 3, "I2C_SCL")):
    (hx, hy_), (sx, sy) = pad_at(j1, header_pin), pad_at(j2, socket_pin)
    route(net_name, [(hx, hy_), (hx, BUS_Y[net_name]), (sx, BUS_Y[net_name]), (sx, sy)], SIGNAL_W)
# INT from pin 40 (BCM21): right, down to just above the bus, into the socket from below
(hx, hy_), (sx, sy) = pad_at(j1, 40), pad_at(j2, 2)
int_x = hx + INT_X_OFFSET
route("TWIST_INT", [(hx, hy_), (int_x, hy_), (int_x, INT_BUS_Y), (sx, INT_BUS_Y), (sx, sy)], SIGNAL_W)
# RST from pin 38 (BCM20): up, over pin 40, down right of INT, into the socket from above
(rx, ry), (sx, sy) = pad_at(j1, 38), pad_at(j2, 1)
rst_x, over_y = hx + RST_X_OFFSET, hy_ + RST_OVER_Y
route("TWIST_RST", [(rx, ry), (rx, over_y), (rst_x, over_y), (rst_x, RST_Y), (sx, RST_Y), (sx, sy)], SIGNAL_W)
# Debug connector: up from the header, over the top (above RST), down right of
# RST and into J4 from the left. Nested: the rightmost header pin turns lowest
# and innermost, and ends on J4's bottom pin.
for k, (conn_pin, (net_name, _, header_pin)) in enumerate(sorted(DEBUG_CONN_PINS.items(), reverse=True), 1):
    (px, py), (cx, cy_) = pad_at(j1, header_pin), pad_at(j4, conn_pin)
    over, down_x = over_y - k * DEBUG_PITCH, rst_x + 0.6 + k * DEBUG_PITCH
    route(net_name, [(px, py), (px, over), (down_x, over), (down_x, cy_), (cx, cy_)], SIGNAL_W)

front_no_tracks = p.ZONE(board)
front_no_tracks.SetIsRuleArea(True)
front_no_tracks.SetZoneName("front_no_tracks")
front_no_tracks.SetLayer(p.F_Cu)
front_no_tracks.SetDoNotAllowTracks(True)
front_no_tracks.SetDoNotAllowVias(True)
front_no_tracks.SetDoNotAllowZoneFills(False)
front_no_tracks.SetDoNotAllowPads(False)
front_no_tracks.SetDoNotAllowFootprints(False)
fo = front_no_tracks.Outline()
fo.NewOutline()
for x, y in ((0, 0), (PANEL_W, 0), (PANEL_W, PANEL_H), (0, PANEL_H)):
    v = pt(x, y)
    fo.Append(v.x, v.y)
board.Add(front_no_tracks)

# --- Silkscreen ----------------------------------------------------------------------------
# Front: what the controls do (knob: clockwise = positive in EncoderTask, a
# press toggles the heater via TemperatureSetpoint::toggle())
text("POWER", SWITCH_X, CY - SWITCH_FLANGE[1] / 2 - 3.0, p.F_SilkS, size=2.0)
# Knob: a 100 degree arc around the shaft over the top of the knob, arrows at both ends,
# - at the left end and + at the right end
KNOB_ARC_R, KNOB_ARC_SPAN, KNOB_ARC_W = KNOB_D / 2 + 2.5, 100.0, 0.25
ends = []
for angle in (90 + KNOB_ARC_SPAN / 2, 90, 90 - KNOB_ARC_SPAN / 2):  # left end, top, right end
    a = math.radians(angle)
    ends.append((KNOB_X + KNOB_ARC_R * math.cos(a), CY - KNOB_ARC_R * math.sin(a)))
arc = p.PCB_SHAPE(board)
arc.SetShape(p.SHAPE_T_ARC)
arc.SetArcGeometry(pt(*ends[0]), pt(*ends[1]), pt(*ends[2]))
arc.SetLayer(p.F_SilkS)
arc.SetWidth(mm(KNOB_ARC_W))
board.Add(arc)
for (x, y), sign in ((ends[0], -1), (ends[2], 1)):
    # Tangent at the end, pointing away from the middle of the arc
    rx, ry = (x - KNOB_X) / KNOB_ARC_R, (y - CY) / KNOB_ARC_R
    ux, uy = (-ry, rx) if sign > 0 else (ry, -rx)
    arrow_head(x, y, ux, uy, p.F_SilkS, head=1.5, width=KNOB_ARC_W)
    text("+" if sign > 0 else "-", x + sign * 3.0, y + 1.5, p.F_SilkS, size=2.5)
text("SET TEMP", KNOB_X, CY - KNOB_ARC_R - 3.0, p.F_SilkS, size=2.0)
text("PUSH: ON/OFF", KNOB_X, CY + 13.0, p.F_SilkS, size=1.5)
# Credits, on the front hidden underneath the WIO
for i, credit in enumerate(CREDITS):
    text(credit, WIO_X, 44.0 + 3.0 * i, p.F_SilkS, size=1.2)

# Back: outlines of the parts on the front, so their positions are known from
# behind (clamped to the visible panel where they run into the slots)
top, bottom = SLOT_DEPTH + 0.3, PANEL_H - SLOT_DEPTH - 0.3
rect(WIO_X - WIO_BODY[0] / 2, max(CY - WIO_BODY[1] / 2, top),
     WIO_X + WIO_BODY[0] / 2, min(CY + WIO_BODY[1] / 2, bottom), p.B_SilkS, 0.15)
text("WIO TERMINAL", WIO_X - WIO_BODY[0] / 2 + 2.0, CY + 15.0, p.B_SilkS, size=1.2, angle=90)
centred_rect(SWITCH_X, CY, *SWITCH_FLANGE, p.B_SilkS, 0.15)
text("MAINS SWITCH", SWITCH_X, CY - SWITCH_FLANGE[1] / 2 - 1.5, p.B_SilkS, size=1.0)
circle(KNOB_X, CY, KNOB_D, p.B_SilkS, 0.15)
text("KNOB", KNOB_X, CY - KNOB_D / 2 - 1.5, p.B_SilkS, size=1.0)
# The Twist board itself (on the back); its bottom edge runs over the socket
# (pads and its own outline), so the board outline is interrupted there
tx0, tx1 = KNOB_X - TWIST_BOARD[0] / 2, KNOB_X + TWIST_BOARD[0] / 2
ty0, ty1 = CY - TWIST_BOARD[1] / 2, CY + TWIST_BOARD[1] / 2
socket = j2.GetBoundingBox(False)
socket_x0, socket_x1 = p.ToMM(socket.GetX()) - SHEET_X - 0.4, p.ToMM(socket.GetRight()) - SHEET_X + 0.4
socket_y0, socket_y1 = p.ToMM(socket.GetY()) - SHEET_Y - 0.4, p.ToMM(socket.GetBottom()) - SHEET_Y + 0.4
for edge_y in (ty0, ty1):
    if socket_y0 <= edge_y <= socket_y1:
        line(tx0, edge_y, socket_x0, edge_y, p.B_SilkS)
        line(socket_x1, edge_y, tx1, edge_y, p.B_SilkS)
    else:
        line(tx0, edge_y, tx1, edge_y, p.B_SilkS)
line(tx0, ty0, tx0, ty1, p.B_SilkS)
line(tx1, ty0, tx1, ty1, p.B_SilkS)
text("QWIIC TWIST", KNOB_X, ty0 - 1.2, p.B_SilkS, size=1.0)
centred_rect(XLR_X, CY, *XLR_FLANGE, p.B_SilkS, 0.15)
text("XLR (IRON)", XLR_X, CY - XLR_FLANGE[1] / 2 - 1.5, p.B_SilkS, size=1.0)
# Credits on the back, in the free strip below the Twist and the XLR
credits_x = (KNOB_X + TWIST_BOARD[0] / 2 + PANEL_W - SLOT_DEPTH) / 2
for i, credit in enumerate(CREDITS):
    text(credit, credits_x, 50.0 + 3.0 * i, p.B_SilkS, size=1.2)
# QR code of the repository, between the Twist board and the XLR. Knockout:
# white silkscreen with the modules left open, so they show as the black
# soldermask - normal (dark on light) polarity, which every scanner reads.
# KiCad draws a barcode on a back layer readable from behind (checked: finder
# patterns top-left, top-right and bottom-left when viewing the back).
qr = p.PCB_BARCODE(board)
qr.SetKind(p.BARCODE_T_QR_CODE)
qr.SetText(REPO_URL)
qr.SetErrorCorrection(p.BARCODE_ECC_T_M)
qr.SetLayer(p.B_SilkS)
qr.SetWidth(mm(QR_SIZE))
qr.SetHeight(mm(QR_SIZE))
qr.SetIsKnockout(True)
qr.SetMargin(p.VECTOR2I(mm(QR_MARGIN), mm(QR_MARGIN)))
qr.SetShowText(False)
qr.SetPosition(pt((KNOB_X + TWIST_BOARD[0] / 2 + XLR_X - XLR_FLANGE[0] / 2) / 2, CY))
qr.AssembleBarcode()
board.Add(qr)

# --- GND pours on both sides (strength, even look under the mask) -----------------------
for layer in (p.F_Cu, p.B_Cu):
    z = p.ZONE(board)
    z.SetLayer(layer)
    z.SetNet(net("GND"))
    z.SetZoneName("GND_" + ("front" if layer == p.F_Cu else "back"))
    ol = z.Outline()
    ol.NewOutline()
    for x, y in ((0, 0), (PANEL_W, 0), (PANEL_W, PANEL_H), (0, PANEL_H)):
        v = pt(x, y)
        ol.Append(v.x, v.y)
    board.Add(z)

p.SaveBoard(PCB_PATH, board)
# Fill the pours on a freshly loaded copy: only a board loaded from its project
# has the design rules (edge and hole clearance) the filler needs - filling the
# board built above leaves the pours empty or ignores those clearances.
board = p.LoadBoard(PCB_PATH)
board.BuildConnectivity()
p.ZONE_FILLER(board).Fill(board.Zones())
p.SaveBoard(PCB_PATH, board)
print("Wrote", PCB_PATH)
