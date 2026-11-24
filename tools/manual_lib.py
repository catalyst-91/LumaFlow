"""Shared ReportLab layout for LUMAFLOW PDFs: cover, clickable TOC, bookmarks, page numbers."""
import os, re
from reportlab.lib.pagesizes import A4
from reportlab.lib.units import mm
from reportlab.lib import colors
from reportlab.lib.styles import ParagraphStyle
from reportlab.platypus import (BaseDocTemplate, PageTemplate, Frame, Paragraph, Spacer, Image, Table, TableStyle,
                                PageBreak, KeepTogether, NextPageTemplate, CondPageBreak)
from reportlab.platypus.tableofcontents import TableOfContents
from reportlab.graphics.shapes import Drawing, Rect, String, Line, Polygon
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BLUE = colors.HexColor("#2f6fe8"); VIOLET = colors.HexColor("#8a52f5"); DARK = colors.HexColor("#14151f"); GREY = colors.HexColor("#5a5f7a")
LIGHT = colors.HexColor("#eef1fb")

for name, f in [("DejaVu", "DejaVuSans.ttf"), ("DejaVu-Bold", "DejaVuSans-Bold.ttf"), ("DejaVuMono", "DejaVuSansMono.ttf"), ("DejaVu-Oblique", "DejaVuSans-Oblique.ttf")]:
    for d in ["/usr/share/fonts/truetype/dejavu/"]:
        if os.path.exists(d + f): pdfmetrics.registerFont(TTFont(name, d + f))
if "DejaVu-Oblique" not in pdfmetrics.getRegisteredFontNames():
    pdfmetrics.registerFont(TTFont("DejaVu-Oblique", "/usr/share/fonts/truetype/liberation/LiberationSans-Italic.ttf"))
from reportlab.pdfbase.pdfmetrics import registerFontFamily
registerFontFamily("DejaVu", normal="DejaVu", bold="DejaVu-Bold", italic="DejaVu-Oblique", boldItalic="DejaVu-Bold")

S = {
    "body": ParagraphStyle("body", fontName="DejaVu", fontSize=9.6, leading=14, spaceAfter=6, textColor=colors.HexColor("#1d1f2b")),
    "h1": ParagraphStyle("h1", fontName="DejaVu-Bold", fontSize=20, leading=25, spaceBefore=4, spaceAfter=12, textColor=BLUE),
    "h2": ParagraphStyle("h2", fontName="DejaVu-Bold", fontSize=13, leading=17, spaceBefore=10, spaceAfter=6, textColor=VIOLET),
    "h3": ParagraphStyle("h3", fontName="DejaVu-Bold", fontSize=10.5, leading=14, spaceBefore=6, spaceAfter=3, textColor=DARK),
    "bullet": ParagraphStyle("bullet", fontName="DejaVu", fontSize=9.6, leading=13.5, leftIndent=14, bulletIndent=4, spaceAfter=2),
    "code": ParagraphStyle("code", fontName="DejaVuMono", fontSize=8.2, leading=11, backColor=LIGHT, borderPadding=5, leftIndent=4, spaceBefore=3, spaceAfter=8),
    "caption": ParagraphStyle("caption", fontName="DejaVu-Oblique", fontSize=8.2, leading=11, textColor=GREY, alignment=1, spaceAfter=10),
    "cell": ParagraphStyle("cell", fontName="DejaVu", fontSize=8, leading=10.2),
    "cellb": ParagraphStyle("cellb", fontName="DejaVu-Bold", fontSize=8, leading=10.2, textColor=colors.white),
    "note": ParagraphStyle("note", fontName="DejaVu", fontSize=9, leading=13, backColor=colors.HexColor("#fff6e0"), borderColor=colors.HexColor("#f0b44c"),
                           borderWidth=0.6, borderPadding=6, spaceBefore=4, spaceAfter=10),
    "toc1": ParagraphStyle("toc1", fontName="DejaVu", fontSize=10, leading=14, leftIndent=8),
    "toc2": ParagraphStyle("toc2", fontName="DejaVu", fontSize=8.5, leading=11, leftIndent=24, textColor=GREY),
}

def esc(t): return t.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")

def md_inline(t):
    t = esc(t)
    t = re.sub(r"\*\*(.+?)\*\*", r"<b>\1</b>", t)
    t = re.sub(r"`(.+?)`", r"<font name='DejaVuMono' size='8.4' color='#3b3f8f'>\1</font>", t)
    return t

class Doc(BaseDocTemplate):
    def __init__(self, path, title, subtitle, **kw):
        super().__init__(path, pagesize=A4, leftMargin=20*mm, rightMargin=20*mm, topMargin=20*mm, bottomMargin=18*mm, title=title, author="LUMAFLOW", subject=subtitle, **kw)
        self.doc_title = title
        frame = Frame(self.leftMargin, self.bottomMargin, self.width, self.height, id="f")
        self.addPageTemplates([PageTemplate("cover", [frame], onPage=self._cover), PageTemplate("body", [frame], onPage=self._page)])
        self._hcount = 0
    def _cover(self, c, d):
        c.saveState(); c.setFillColor(DARK); c.rect(0, 0, A4[0], A4[1], fill=1, stroke=0)
        # flowing light lines (original artwork)
        import math
        for k in range(7):
            c.setStrokeColor(colors.Color(0.18 + k * 0.07, 0.44 - k * 0.03, 1.0, alpha=0.85 - k * 0.1)); c.setLineWidth(3.5 - k * 0.4)
            p = c.beginPath()
            for i in range(121):
                x = i / 120 * A4[0]; y = A4[1] * 0.42 + math.sin(i / 120 * math.pi * 2 + k * 0.4) * (90 - k * 9) * math.sin(i / 120 * math.pi)
                (p.moveTo if i == 0 else p.lineTo)(x, y)
            c.drawPath(p, stroke=1, fill=0)
        icon = os.path.join(ROOT, "resources/icons/lumaflow.png")
        c.drawImage(icon, 20*mm, A4[1] - 62*mm, 34*mm, 34*mm, mask="auto")
        c.setFillColor(colors.white); c.setFont("DejaVu-Bold", 40); c.drawString(20*mm, A4[1] - 82*mm, "LUMAFLOW")
        c.setFont("DejaVu", 16); c.setFillColor(colors.HexColor("#9fb8ff")); c.drawString(20*mm, A4[1] - 93*mm, self.doc_title)
        c.setFont("DejaVu", 10); c.setFillColor(colors.HexColor("#b8bdd8"))
        c.drawString(20*mm, 30*mm, "Version 3.0.0  ·  November 2026"); c.drawString(20*mm, 24*mm, "Real-time visual effects · Media server · Pixel mapping · Spatial effects · Show control")
        c.restoreState()
    def _page(self, c, d):
        c.saveState(); c.setStrokeColor(BLUE); c.setLineWidth(0.6)
        c.line(20*mm, A4[1] - 14*mm, A4[0] - 20*mm, A4[1] - 14*mm)
        c.setFont("DejaVu", 7.8); c.setFillColor(GREY)
        c.drawString(20*mm, A4[1] - 12*mm, "LUMAFLOW 3.0  ·  " + self.doc_title)
        c.drawRightString(A4[0] - 20*mm, 10*mm, "Page %d" % d.page)
        c.restoreState()
    def beforeDocument(self):
        self._hcount = 0
    def afterFlowable(self, f):
        if isinstance(f, Paragraph) and f.style.name in ("h1", "h2"):
            text = f.getPlainText(); lvl = 0 if f.style.name == "h1" else 1
            key = "h%d" % self._hcount; self._hcount += 1
            self.canv.bookmarkPage(key); self.canv.addOutlineEntry(text, key, level=lvl, closed=lvl == 0)
            self.notify("TOCEntry", (lvl, text, self.page, key))

def toc():
    t = TableOfContents(); t.levelStyles = [S["toc1"], S["toc2"]]; t.dotsMinLevel = 0
    return t

def table(rows, widths=None, header=True, zebra=True):
    data = []
    for i, r in enumerate(rows):
        data.append([Paragraph(md_inline(str(c)), S["cellb"] if header and i == 0 else S["cell"]) for c in r])
    t = Table(data, colWidths=widths, repeatRows=1 if header else 0)
    st = [("GRID", (0, 0), (-1, -1), 0.3, colors.HexColor("#c9cde3")), ("VALIGN", (0, 0), (-1, -1), "TOP"),
          ("TOPPADDING", (0, 0), (-1, -1), 2.5), ("BOTTOMPADDING", (0, 0), (-1, -1), 2.5)]
    if header: st.append(("BACKGROUND", (0, 0), (-1, 0), BLUE))
    if zebra:
        for i in range(1, len(data)):
            if i % 2 == 0: st.append(("BACKGROUND", (0, i), (-1, i), colors.HexColor("#f4f6fd")))
    t.setStyle(TableStyle(st)); return t

def image(path, width_mm=170, caption=None):
    from reportlab.lib.utils import ImageReader
    ir = ImageReader(path); w, h = ir.getSize(); W = width_mm * mm; H = W * h / w
    if H > 215 * mm: H = 215 * mm; W = H * w / h
    out = [Image(path, W, H)]
    if caption: out.append(Paragraph(esc(caption), S["caption"]))
    return KeepTogether(out)

def boxes_diagram(boxes, arrows, w=170, h=80, title=None):
    """boxes: list of (x,y,w,h,label,color) in mm; arrows: list of ((x1,y1),(x2,y2))"""
    d = Drawing(w * mm, h * mm)
    for (x1, y1), (x2, y2) in arrows:
        d.add(Line(x1 * mm, y1 * mm, x2 * mm, y2 * mm, strokeColor=GREY, strokeWidth=1.1))
        import math
        a = math.atan2(y2 - y1, x2 - x1); L = 2.4 * mm
        d.add(Polygon([x2 * mm, y2 * mm, x2 * mm - L * math.cos(a - 0.4), y2 * mm - L * math.sin(a - 0.4), x2 * mm - L * math.cos(a + 0.4), y2 * mm - L * math.sin(a + 0.4)], fillColor=GREY, strokeColor=GREY))
    for x, y, bw, bh, label, col in boxes:
        d.add(Rect(x * mm, y * mm, bw * mm, bh * mm, rx=2 * mm, ry=2 * mm, fillColor=col, strokeColor=colors.white, strokeWidth=0.8))
        lines = label.split("\n")
        for i, ln in enumerate(lines):
            d.add(String((x + bw / 2) * mm, (y + bh / 2) * mm + (len(lines) / 2 - i - 0.75) * 9.5, ln, fontName="DejaVu-Bold" if i == 0 else "DejaVu", fontSize=7.8 if i == 0 else 6.6, fillColor=colors.white, textAnchor="middle"))
    return d

def blocks_to_flowables(blocks):
    """Mini markup: '# ' h1, '## ' h2, '### ' h3, '- ' bullet, '```' code, '!note ' callout, otherwise paragraph."""
    out = []; code = None
    for line in blocks.strip("\n").split("\n"):
        if line.startswith("```"):
            if code is None: code = []
            else: out.append(Paragraph("<br/>".join(esc(l).replace(" ", "&nbsp;") for l in code), S["code"])); code = None
            continue
        if code is not None: code.append(line); continue
        s = line.strip()
        if not s: continue
        if s.startswith("### "): out.append(Paragraph(md_inline(s[4:]), S["h3"]))
        elif s.startswith("## "): out.append(CondPageBreak(40 * mm)); out.append(Paragraph(md_inline(s[3:]), S["h2"]))
        elif s.startswith("- "): out.append(Paragraph(md_inline(s[2:]), S["bullet"], bulletText="•"))
        elif s.startswith("!note "): out.append(Paragraph(md_inline(s[6:]), S["note"]))
        else: out.append(Paragraph(md_inline(s), S["body"]))
    return out

def md_table(md):
    rows = [ [c.strip() for c in l.strip().strip("|").split("|")] for l in md.strip().split("\n") if l.strip().startswith("|") and not re.match(r"^\|[\s:\-|]+\|$", l.strip())]
    return rows
