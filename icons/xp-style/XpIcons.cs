// "Modern 4" toolbar icons: Windows XP (Luna) like style.
// Original vector drawings: bright gradients, glossy balls, colored outlines, soft shadows.
// Every icon is drawn in a 96 x 96 unit box, rendered 4x bigger and scaled down (anti-aliasing).
// Cell order in strip: Add, Extract, Test, View, Delete, Find, Wizard (Easy Extract), Info, Comment, Copy, Move
using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.IO;
using System.Runtime.InteropServices;

public static class XpIcons
{
  public const int NumIcons = 11;
  const int kSuper = 4;

  // outline width in units: about 1.1 pixel at small sizes, 2.2 units at 96
  static float OW;
  static bool Small;   // 16 / 24: fewer details
  static bool Tiny;    // 16

  static Color C(int rgb) { return Color.FromArgb(255, (rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF); }
  static Color A(int a, Color c) { return Color.FromArgb(a, c); }

  // ---------- public ----------

  public static Bitmap RenderIcon(int index, int size)
  {
    int n = size * kSuper;
    using (var big = new Bitmap(n, n, PixelFormat.Format32bppArgb))
    {
      using (var g = Graphics.FromImage(big))
      {
        g.SmoothingMode = SmoothingMode.AntiAlias;
        g.PixelOffsetMode = PixelOffsetMode.HighQuality;
        g.Clear(Color.Transparent);
        g.ScaleTransform(n / 96f, n / 96f);
        OW = Math.Max(2.2f, 1.15f * 96f / size);
        Small = size <= 24;
        Tiny = size <= 16;
        DrawIcon(g, index);
      }
      return Downscale(big, kSuper);
    }
  }

  public static Bitmap RenderStrip(int size)
  {
    var strip = new Bitmap(size * NumIcons, size, PixelFormat.Format32bppArgb);
    using (var g = Graphics.FromImage(strip))
    {
      g.Clear(Color.Transparent);
      g.CompositingMode = CompositingMode.SourceCopy;
      for (int i = 0; i < NumIcons; i++)
        using (var icon = RenderIcon(i, size))
          g.DrawImageUnscaled(icon, i * size, 0);
    }
    return strip;
  }

  // .ico with PNG entries (like EasyExtract3.ico)
  public static void WriteIco(int index, int[] sizes, string path)
  {
    var pngs = new byte[sizes.Length][];
    for (int i = 0; i < sizes.Length; i++)
      using (var bmp = RenderIcon(index, sizes[i]))
      using (var ms = new MemoryStream())
      {
        bmp.Save(ms, ImageFormat.Png);
        pngs[i] = ms.ToArray();
      }
    using (var fs = new FileStream(path, FileMode.Create))
    using (var w = new BinaryWriter(fs))
    {
      w.Write((short)0); w.Write((short)1); w.Write((short)sizes.Length);
      int offset = 6 + 16 * sizes.Length;
      for (int i = 0; i < sizes.Length; i++)
      {
        int s = sizes[i];
        w.Write((byte)(s >= 256 ? 0 : s)); w.Write((byte)(s >= 256 ? 0 : s));
        w.Write((byte)0); w.Write((byte)0);
        w.Write((short)1); w.Write((short)32);
        w.Write(pngs[i].Length); w.Write(offset);
        offset += pngs[i].Length;
      }
      foreach (var p in pngs)
        w.Write(p);
    }
  }

  // ---------- icons ----------

  static void DrawIcon(Graphics g, int index)
  {
    switch (index)
    {
      case 0: // Add: archive box + green "+" ball
        GroundShadow(g, 44, 86, 70);
        Box(g, new RectangleF(6, 16, 72, 68));
        Orb(g, 70, 68, 22, GreenOrb);
        PlusGlyph(g, 70, 68, 22);
        break;
      case 1: // Extract: folder + blue arrow going out
        GroundShadow(g, 46, 86, 78);
        FolderBack(g, new RectangleF(4, 26, 80, 60));
        FolderFront(g, new RectangleF(4, 26, 80, 60), false);
        Arrow(g, 40, 62, -48, Tiny ? 58 : 56, Small ? 22 : 18, BlueArrow);
        break;
      case 2: // Test: document + green check
        GroundShadow(g, 46, 88, 60);
        Document(g, new RectangleF(14, 4, 58, 82));
        Check(g);
        break;
      case 3: // View: document + magnifier
        GroundShadow(g, 46, 88, 64);
        Document(g, new RectangleF(10, 4, 58, 82));
        Magnifier(g, 58, 54, Small ? 21 : 18, 1.25f);
        break;
      case 4: // Delete: red X
        GroundShadow(g, 48, 86, 66);
        RedX(g);
        break;
      case 5: // Find: big magnifier
        GroundShadow(g, 50, 88, 66);
        Magnifier(g, 40, 40, Small ? 30 : 28, 1.35f);
        break;
      case 6: // Wizard (Easy Extract): zipped folder + green up arrow ball
        GroundShadow(g, 44, 86, 78);
        FolderBack(g, new RectangleF(4, 18, 80, 64));
        FolderFront(g, new RectangleF(4, 18, 80, 64), true);
        Orb(g, 70, 66, 22, GreenOrb);
        UpGlyph(g, 70, 66, 22);
        break;
      case 7: // Info: blue ball with "i"
        GroundShadow(g, 48, 90, 66);
        Orb(g, 48, 46, 41, BlueOrb);
        InfoGlyph(g, 48, 46, 41);
        break;
      case 8: // Comment: yellow balloon
        GroundShadow(g, 48, 88, 72);
        Balloon(g);
        break;
      case 9: // Copy: two documents
        GroundShadow(g, 50, 90, 64);
        Document(g, new RectangleF(8, 4, 50, 68));
        Document(g, new RectangleF(36, 24, 50, 66));
        break;
      case 10: // Move: document + blue arrow
        GroundShadow(g, 48, 88, 70);
        Document(g, new RectangleF(8, 4, 56, 82));
        Arrow(g, 36, 62, 0, Tiny ? 58 : 56, Small ? 24 : 20, BlueArrow);
        break;
    }
  }

  // ---------- color sets ----------

  struct OrbColors { public Color Light, Mid, Dark, Edge; }
  static readonly OrbColors GreenOrb = new OrbColors { Light = C(0xC8F5A0), Mid = C(0x48B828), Dark = C(0x1E7A10), Edge = C(0x14560A) };
  static readonly OrbColors BlueOrb  = new OrbColors { Light = C(0xBFE3FF), Mid = C(0x2F86E8), Dark = C(0x1050B8), Edge = C(0x0A3482) };

  struct ArrowColors { public Color Top, Bottom, Edge; }
  static readonly ArrowColors BlueArrow = new ArrowColors { Top = C(0x8CC8FF), Bottom = C(0x1A5CD0), Edge = C(0x0E3A8C) };

  // ---------- helpers ----------

  static GraphicsPath RoundRect(RectangleF r, float rad)
  {
    var p = new GraphicsPath();
    float d = Math.Min(rad * 2, Math.Min(r.Width, r.Height));
    if (d <= 0.01f) { p.AddRectangle(r); return p; }
    p.AddArc(r.X, r.Y, d, d, 180, 90);
    p.AddArc(r.Right - d, r.Y, d, d, 270, 90);
    p.AddArc(r.Right - d, r.Bottom - d, d, d, 0, 90);
    p.AddArc(r.X, r.Bottom - d, d, d, 90, 90);
    p.CloseFigure();
    return p;
  }

  static GraphicsPath Poly(params float[] xy)
  {
    var pts = new PointF[xy.Length / 2];
    for (int i = 0; i < pts.Length; i++)
      pts[i] = new PointF(xy[2 * i], xy[2 * i + 1]);
    var p = new GraphicsPath();
    p.AddPolygon(pts);
    return p;
  }

  static LinearGradientBrush VGrad(RectangleF r, Color top, Color bottom)
  {
    var rr = new RectangleF(r.X, r.Y - 0.5f, Math.Max(r.Width, 1), Math.Max(r.Height, 1) + 1);
    return new LinearGradientBrush(rr, top, bottom, LinearGradientMode.Vertical);
  }

  static void FillOutline(Graphics g, GraphicsPath p, Brush fill, Color edge)
  {
    g.FillPath(fill, p);
    using (var pen = new Pen(edge, OW) { LineJoin = LineJoin.Round })
      g.DrawPath(pen, p);
  }

  // soft elliptic shadow under the object
  static void GroundShadow(Graphics g, float cx, float cy, float w)
  {
    if (Small)
      return;
    float h = w * 0.16f;
    using (var p = new GraphicsPath())
    {
      p.AddEllipse(cx - w / 2, cy - h / 2, w, h);
      using (var b = new PathGradientBrush(p))
      {
        b.CenterColor = Color.FromArgb(80, 0, 0, 0);
        b.SurroundColors = new[] { Color.FromArgb(0, 0, 0, 0) };
        g.FillPath(b, p);
      }
    }
  }

  // white top highlight (glossy look), clipped to path
  static void Gloss(Graphics g, GraphicsPath clip, RectangleF r, float part, int alpha)
  {
    var st = g.Save();
    g.SetClip(clip, CombineMode.Intersect);
    var hr = new RectangleF(r.X - 2, r.Y - 2, r.Width + 4, r.Height * part + 2);
    using (var b = VGrad(hr, Color.FromArgb(alpha, 255, 255, 255), Color.FromArgb(0, 255, 255, 255)))
      g.FillRectangle(b, hr);
    g.Restore(st);
  }

  // ---------- shapes ----------

  static void Document(Graphics g, RectangleF r)
  {
    float fold = r.Width * 0.30f;
    float l = r.Left, t = r.Top, rt = r.Right, b = r.Bottom;
    using (var p = Poly(l, t, rt - fold, t, rt, t + fold, rt, b, l, b))
    {
      using (var br = new LinearGradientBrush(new PointF(l, t), new PointF(rt, b), C(0xFFFFFF), C(0xD3DEEF)))
        FillOutline(g, p, br, Tiny ? C(0x4A5F80) : C(0x6A80A6));
    }
    // folded corner
    using (var p = Poly(rt - fold, t, rt - fold, t + fold, rt, t + fold))
    using (var br = new LinearGradientBrush(new PointF(rt - fold, t + fold), new PointF(rt, t), C(0xC3D2EA), C(0xFFFFFF)))
      FillOutline(g, p, br, Tiny ? C(0x4A5F80) : C(0x6A80A6));
    if (Tiny)
      return;
    // text lines
    float lw = Small ? 4.5f : 3.2f;
    using (var pen = new Pen(C(0x8FA7CC), lw))
    {
      float x0 = l + r.Width * 0.16f, x1 = rt - r.Width * 0.16f;
      int numLines = Small ? 3 : 6;
      float y = t + r.Height * 0.30f, step = (r.Height * 0.60f) / numLines;
      for (int i = 0; i < numLines; i++, y += step)
      {
        float xe = (i == numLines - 1) ? x0 + (x1 - x0) * 0.6f : x1;
        g.DrawLine(pen, x0, y, xe, y);
      }
    }
  }

  static void FolderBack(Graphics g, RectangleF r)
  {
    float l = r.Left, t = r.Top, rt = r.Right, b = r.Bottom;
    float th = r.Height * 0.16f, tw = r.Width * 0.40f;
    using (var p = Poly(l, t + th, l + 3, t, l + tw - 3, t, l + tw + th * 0.8f, t + th, rt, t + th, rt, b, l, b))
    using (var br = VGrad(r, C(0xF0C24E), C(0xC8890E)))
      FillOutline(g, p, br, C(0x8E5F06));
  }

  static void FolderFront(Graphics g, RectangleF r, bool zipper)
  {
    float l = r.Left, t = r.Top, rt = r.Right, b = r.Bottom;
    float ft = t + r.Height * 0.32f;
    float skew = r.Width * 0.08f;
    using (var p = Poly(l + skew, ft, rt, ft, rt - skew * 0.6f, b, l, b))
    {
      var fr = new RectangleF(l, ft, r.Width, b - ft);
      using (var br = VGrad(fr, C(0xFFEBA6), C(0xF2B930)))
        FillOutline(g, p, br, C(0x9A6808));
      Gloss(g, p, fr, 0.35f, 120);
      if (zipper)
      {
        float zx = l + r.Width * 0.30f;
        float zw = Small ? 9 : 7;
        var zr = new RectangleF(zx - zw / 2, ft, zw, b - ft);
        var st = g.Save();
        g.SetClip(p, CombineMode.Intersect);
        using (var br2 = new SolidBrush(C(0x7A5208)))
          g.FillRectangle(br2, zr);
        if (!Small)
        {
          using (var teeth = new SolidBrush(C(0xE8D9A8)))
            for (float y = ft + 3; y < b - 2; y += 6)
              g.FillRectangle(teeth, zx - zw / 2 + ((((int)(y / 6)) & 1) == 0 ? 0 : zw / 2), y, zw / 2, 3);
          // pull tab
          using (var tab = new SolidBrush(C(0xBFC7D0)))
          using (var pen = new Pen(C(0x4A5260), OW * 0.6f))
          {
            var tr = new RectangleF(zx - 5, ft + 2, 10, 14);
            using (var tp = RoundRect(tr, 3)) { g.FillPath(tab, tp); g.DrawPath(pen, tp); }
          }
        }
        g.Restore(st);
      }
    }
  }

  static void Box(Graphics g, RectangleF r)
  {
    float l = r.Left, t = r.Top, rt = r.Right, b = r.Bottom;
    float lidH = r.Height * 0.24f;
    var body = new RectangleF(l + 4, t + lidH - 2, r.Width - 8, r.Height - lidH + 2);
    using (var p = RoundRect(body, 3))
    using (var br = VGrad(body, C(0xE6BE7E), C(0xB98444)))
      FillOutline(g, p, br, C(0x7A5020));
    var lid = new RectangleF(l, t, r.Width, lidH);
    using (var p = RoundRect(lid, 3))
    {
      using (var br = VGrad(lid, C(0xF8DCA6), C(0xD9A862)))
        FillOutline(g, p, br, C(0x7A5020));
      Gloss(g, p, lid, 0.5f, 140);
    }
    if (Tiny)
      return;
    // handle slot
    var slot = new RectangleF(l + r.Width * 0.32f, t + lidH + r.Height * 0.12f, r.Width * 0.36f, Small ? 8 : 7);
    using (var p = RoundRect(slot, slot.Height / 2))
    using (var br = VGrad(slot, C(0x6A4210), C(0x9A6A30)))
      g.FillPath(br, p);
  }

  static void Orb(Graphics g, float cx, float cy, float r, OrbColors c)
  {
    using (var p = new GraphicsPath())
    {
      p.AddEllipse(cx - r, cy - r, 2 * r, 2 * r);
      using (var br = new PathGradientBrush(p))
      {
        br.CenterPoint = new PointF(cx - r * 0.25f, cy - r * 0.35f);
        br.InterpolationColors = new ColorBlend
        {
          Colors = new[] { c.Dark, c.Mid, c.Mid, c.Light },
          Positions = new[] { 0f, 0.35f, 0.65f, 1f }
        };
        FillOutline(g, p, br, c.Edge);
      }
      // glossy top
      var hr = new RectangleF(cx - r * 0.66f, cy - r * 0.90f, r * 1.32f, r * 0.80f);
      using (var hp = new GraphicsPath())
      {
        hp.AddEllipse(hr);
        using (var hb = VGrad(hr, Color.FromArgb(210, 255, 255, 255), Color.FromArgb(10, 255, 255, 255)))
          g.FillPath(hb, hp);
      }
    }
  }

  // white glyph with dark soft shadow
  static void Glyph(Graphics g, GraphicsPath p, Color shadow)
  {
    float d = Math.Max(OW * 0.6f, 1.5f);
    var m = new Matrix();
    m.Translate(d * 0.5f, d);
    using (var sp = (GraphicsPath)p.Clone())
    {
      sp.Transform(m);
      using (var sb = new SolidBrush(Color.FromArgb(110, shadow)))
        g.FillPath(sb, sp);
    }
    using (var wb = new SolidBrush(Color.White))
      g.FillPath(wb, p);
  }

  static void PlusGlyph(Graphics g, float cx, float cy, float r)
  {
    float a = r * 0.62f, w = r * (Small ? 0.46f : 0.36f);
    using (var p = new GraphicsPath())
    {
      p.AddRectangle(new RectangleF(cx - a, cy - w / 2, 2 * a, w));
      p.AddRectangle(new RectangleF(cx - w / 2, cy - a, w, 2 * a));
      p.FillMode = FillMode.Winding;
      Glyph(g, p, C(0x0A3A05));
    }
  }

  static void UpGlyph(Graphics g, float cx, float cy, float r)
  {
    float hw = r * 0.62f, sw = r * (Small ? 0.26f : 0.21f), top = cy - r * 0.66f, mid = cy - r * 0.02f, bot = cy + r * 0.62f;
    using (var p = Poly(cx, top, cx + hw, mid, cx + sw, mid, cx + sw, bot, cx - sw, bot, cx - sw, mid, cx - hw, mid))
      Glyph(g, p, C(0x0A3A05));
  }

  static void InfoGlyph(Graphics g, float cx, float cy, float r)
  {
    using (var p = new GraphicsPath())
    {
      float dr = r * (Small ? 0.17f : 0.14f);
      p.AddEllipse(cx - dr, cy - r * 0.56f - dr, 2 * dr, 2 * dr);
      float sw = r * (Small ? 0.30f : 0.24f);
      p.AddRectangle(new RectangleF(cx - sw / 2, cy - r * 0.22f, sw, r * 0.78f));
      if (!Small)
      {
        // serifs
        p.AddRectangle(new RectangleF(cx - sw * 0.95f, cy - r * 0.22f, sw * 0.5f, sw * 0.38f));
        p.AddRectangle(new RectangleF(cx - sw * 1.0f, cy + r * 0.56f - sw * 0.38f, sw * 2.0f, sw * 0.38f));
      }
      p.FillMode = FillMode.Winding;
      Glyph(g, p, C(0x062A6A));
    }
  }

  // arrow from (x0, y0), angle in degrees (0 = right, -90 = up)
  static void Arrow(Graphics g, float x0, float y0, float angle, float len, float thick, ArrowColors c)
  {
    float hl = thick * 1.25f, hw = thick * 1.05f, t = thick / 2;
    var st = g.Save();
    g.TranslateTransform(x0, y0);
    g.RotateTransform(angle);
    using (var p = Poly(0, -t, len - hl, -t, len - hl, -hw, len, 0, len - hl, hw, len - hl, t, 0, t))
    {
      var r = new RectangleF(0, -hw, len, 2 * hw);
      using (var br = VGrad(r, c.Top, c.Bottom))
        FillOutline(g, p, br, c.Edge);
      Gloss(g, p, new RectangleF(0, -t, len, 2 * t), 0.45f, 150);
    }
    g.Restore(st);
  }

  static void Check(Graphics g)
  {
    var pts = new[] { new PointF(36, 58), new PointF(54, 78), new PointF(86, 34) };
    float w = Small ? 17 : 14;
    using (var pen = new Pen(C(0x145A0A), w + 2 * OW) { LineJoin = LineJoin.Round, StartCap = LineCap.Round, EndCap = LineCap.Round })
      g.DrawLines(pen, pts);
    using (var br = new LinearGradientBrush(new PointF(0, 30), new PointF(0, 82), C(0x9AE85A), C(0x2A9A12)))
    using (var pen = new Pen(br, w) { LineJoin = LineJoin.Round, StartCap = LineCap.Round, EndCap = LineCap.Round })
      g.DrawLines(pen, pts);
    if (!Small)
      using (var pen = new Pen(Color.FromArgb(150, 255, 255, 255), 3) { StartCap = LineCap.Round, EndCap = LineCap.Round })
      {
        g.DrawLine(pen, 37, 55, 52, 72);
        g.DrawLine(pen, 57, 71, 82, 36);
      }
  }

  static void RedX(Graphics g)
  {
    float w = Small ? 22 : 19;
    var a1 = new PointF(18, 16); var a2 = new PointF(78, 76);
    var b1 = new PointF(78, 16); var b2 = new PointF(18, 76);
    using (var pen = new Pen(C(0x7A0A06), w + 2 * OW) { StartCap = LineCap.Round, EndCap = LineCap.Round })
    {
      g.DrawLine(pen, a1, a2);
      g.DrawLine(pen, b1, b2);
    }
    using (var br = new LinearGradientBrush(new PointF(0, 8), new PointF(0, 84), C(0xFF8A6A), C(0xC8180C)))
    using (var pen = new Pen(br, w) { StartCap = LineCap.Round, EndCap = LineCap.Round })
    {
      g.DrawLine(pen, a1, a2);
      g.DrawLine(pen, b1, b2);
    }
    if (!Small)
      using (var pen = new Pen(Color.FromArgb(140, 255, 255, 255), 3.5f) { StartCap = LineCap.Round, EndCap = LineCap.Round })
      {
        g.DrawLine(pen, 18, 12, 46, 40);
        g.DrawLine(pen, 76, 12, 54, 34);
      }
  }

  static void Magnifier(Graphics g, float cx, float cy, float r, float handleLen)
  {
    // handle (down-right)
    float k = 0.7071f;
    var h0 = new PointF(cx + r * 0.9f * k, cy + r * 0.9f * k);
    var h1 = new PointF(cx + r * (0.9f + handleLen) * k, cy + r * (0.9f + handleLen) * k);
    float hw = r * (Small ? 0.50f : 0.42f);
    using (var pen = new Pen(C(0x202020), hw + 2 * OW) { StartCap = LineCap.Round, EndCap = LineCap.Round })
      g.DrawLine(pen, h0, h1);
    using (var br = new LinearGradientBrush(new PointF(h0.X + hw, h0.Y - hw), new PointF(h0.X - hw, h0.Y + hw), C(0x9A6A3A), C(0x4A2A10)))
    using (var pen = new Pen(br, hw) { StartCap = LineCap.Round, EndCap = LineCap.Round })
      g.DrawLine(pen, h0, h1);
    // ring
    using (var p = new GraphicsPath())
    {
      p.AddEllipse(cx - r, cy - r, 2 * r, 2 * r);
      using (var br = new LinearGradientBrush(new PointF(cx - r, cy - r), new PointF(cx + r, cy + r), C(0xF4F6F8), C(0x7C8694)))
        FillOutline(g, p, br, C(0x3A4250));
    }
    // glass
    float gr = r * (Small ? 0.70f : 0.76f);
    using (var p = new GraphicsPath())
    {
      p.AddEllipse(cx - gr, cy - gr, 2 * gr, 2 * gr);
      using (var br = new PathGradientBrush(p))
      {
        br.CenterPoint = new PointF(cx - gr * 0.3f, cy - gr * 0.3f);
        br.CenterColor = C(0xEAF6FF);
        br.SurroundColors = new[] { C(0x5A9AD8) };
        g.FillPath(br, p);
      }
      using (var pen = new Pen(C(0x4A6A90), OW * 0.7f))
        g.DrawPath(pen, p);
      // shine
      if (!Tiny)
        using (var sp = new GraphicsPath())
        {
          sp.AddEllipse(cx - gr * 0.62f, cy - gr * 0.70f, gr * 0.75f, gr * 0.5f);
          using (var sb = new SolidBrush(Color.FromArgb(170, 255, 255, 255)))
            g.FillPath(sb, sp);
        }
    }
  }

  static void Balloon(Graphics g)
  {
    var r = new RectangleF(6, 10, 84, 58);
    using (var p = RoundRect(r, 14))
    using (var tail = Poly(22, 62, 14, 88, 44, 62))
    {
      var all = new RectangleF(6, 10, 84, 78);
      // outline first (both parts), then fill: one shape without inner line
      using (var pen = new Pen(C(0x9A7A10), OW * 2) { LineJoin = LineJoin.Round })
      {
        g.DrawPath(pen, p);
        g.DrawPath(pen, tail);
      }
      using (var br = VGrad(all, C(0xFFFFE8), C(0xF5D860)))
      {
        g.FillPath(br, p);
        g.FillPath(br, tail);
      }
      Gloss(g, p, r, 0.45f, 150);
      if (Tiny)
        return;
      using (var pen = new Pen(C(0xA88A30), Small ? 5f : 4f) { StartCap = LineCap.Round, EndCap = LineCap.Round })
      {
        g.DrawLine(pen, 22, 28, 74, 28);
        g.DrawLine(pen, 22, 40, 74, 40);
        if (!Small)
          g.DrawLine(pen, 22, 52, 56, 52);
      }
    }
  }

  // ---------- scaling ----------

  // box filter with alpha weighting
  static Bitmap Downscale(Bitmap src, int k)
  {
    int sw = src.Width, sh = src.Height, dw = sw / k, dh = sh / k;
    var sd = src.LockBits(new Rectangle(0, 0, sw, sh), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
    var s = new int[sw * sh];
    for (int y = 0; y < sh; y++)
      Marshal.Copy(new IntPtr(sd.Scan0.ToInt64() + (long)y * sd.Stride), s, y * sw, sw);
    src.UnlockBits(sd);
    var d = new int[dw * dh];
    for (int y = 0; y < dh; y++)
      for (int x = 0; x < dw; x++)
      {
        long sa = 0, sr = 0, sg = 0, sb = 0;
        for (int j = 0; j < k; j++)
          for (int i = 0; i < k; i++)
          {
            uint v = (uint)s[(y * k + j) * sw + x * k + i];
            long a = v >> 24;
            sa += a; sr += a * ((v >> 16) & 0xFF); sg += a * ((v >> 8) & 0xFF); sb += a * (v & 0xFF);
          }
        uint o = 0;
        if (sa > 0)
        {
          uint a = (uint)((sa + k * k / 2) / (k * k));
          o = (a << 24) | ((uint)((sr + sa / 2) / sa) << 16) | ((uint)((sg + sa / 2) / sa) << 8) | (uint)((sb + sa / 2) / sa);
        }
        d[y * dw + x] = (int)o;
      }
    var dst = new Bitmap(dw, dh, PixelFormat.Format32bppArgb);
    var dd = dst.LockBits(new Rectangle(0, 0, dw, dh), ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
    for (int y = 0; y < dh; y++)
      Marshal.Copy(d, y * dw, new IntPtr(dd.Scan0.ToInt64() + (long)y * dd.Stride), dw);
    dst.UnlockBits(dd);
    return dst;
  }
}
