using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.Drawing.Text;
using System.IO;
using System.Runtime.InteropServices;

// Side picture for the NSIS Welcome / Finish pages (164x314 at 100%; drawn at 150%: 246x471).
public static class SetupSide
{
  static GraphicsPath Round(RectangleF r, float rad)
  {
    var p = new GraphicsPath(); float d = rad * 2;
    p.AddArc(r.X, r.Y, d, d, 180, 90); p.AddArc(r.Right - d, r.Y, d, d, 270, 90);
    p.AddArc(r.Right - d, r.Bottom - d, d, d, 0, 90); p.AddArc(r.X, r.Bottom - d, d, d, 90, 90);
    p.CloseFigure(); return p;
  }

  static Bitmap LargestIcoFrame(string path)
  {
    byte[] b = File.ReadAllBytes(path);
    int count = BitConverter.ToUInt16(b, 4), best = -1, bestW = 0;
    for (int i = 0; i < count; i++)
    {
      int w = b[6 + i * 16]; if (w == 0) w = 256;
      if (w > bestW) { bestW = w; best = i; }
    }
    int size = BitConverter.ToInt32(b, 6 + best * 16 + 8), off = BitConverter.ToInt32(b, 6 + best * 16 + 12);
    if (b[off] == 0x89) // PNG
      return new Bitmap(new MemoryStream(b, off, size));
    using (var ico = new Icon(path, bestW, bestW)) return ico.ToBitmap();
  }

  static void Center(Graphics g, string s, Font f, Brush b, float w, float y)
  {
    var sz = g.MeasureString(s, f);
    g.DrawString(s, f, b, (w - sz.Width) / 2, y);
  }

  public static void Make(string fmIco, string strip96, string outBmp, string outPng, string version)
  {
    const int W = 246, H = 471;
    using (var bmp = new Bitmap(W, H, PixelFormat.Format24bppRgb))
    using (var g = Graphics.FromImage(bmp))
    {
      g.SmoothingMode = SmoothingMode.AntiAlias;
      g.InterpolationMode = InterpolationMode.HighQualityBicubic;
      g.TextRenderingHint = TextRenderingHint.AntiAliasGridFit;

      // background: dark blue gradient (Windows 11 style)
      using (var bg = new LinearGradientBrush(new Rectangle(0, 0, W, H),
          Color.FromArgb(31, 74, 128), Color.FromArgb(10, 20, 38), 90f))
        g.FillRectangle(bg, 0, 0, W, H);
      // soft light circles
      using (var c1 = new SolidBrush(Color.FromArgb(28, 120, 180, 255)))
      {
        g.FillEllipse(c1, -90, -70, 260, 260);
        g.FillEllipse(c1, 120, 300, 230, 230);
      }

      // app icon: largest frame of .ico (256 px frame is stored as PNG)
      using (var icoBmp = LargestIcoFrame(fmIco))
        g.DrawImage(icoBmp, new Rectangle((W - 92) / 2, 30, 92, 92));

      // title
      using (var f1 = new Font("Segoe UI Semibold", 26f, GraphicsUnit.Pixel))
      using (var f2 = new Font("Segoe UI Light", 30f, GraphicsUnit.Pixel))
      using (var f3 = new Font("Segoe UI", 15f, GraphicsUnit.Pixel))
      using (var white = new SolidBrush(Color.White))
      using (var blue = new SolidBrush(Color.FromArgb(150, 205, 255)))
      using (var gray = new SolidBrush(Color.FromArgb(165, 185, 210)))
      {
        Center(g, "7-Zip", f1, white, W, 136);
        Center(g, "Modern UI", f2, blue, W, 166);
        Center(g, "edition " + version, f3, gray, W, 206);

        // toolbar icons (menu style "Modern 1") on tiles: 3 x 3
        using (var strip = AlphaBmp.Read(strip96))
        {
          int n = strip.Width / 96;
          int[] order = { 0, 1, 2, 3, 4, 5, 6, 7, 8 };
          const int cell = 58, icon = 40, gap = 8;
          int gridW = 3 * cell + 2 * gap;
          int x0 = (W - gridW) / 2, y0 = 242;
          using (var tile = new SolidBrush(Color.FromArgb(40, 255, 255, 255)))
          using (var edge = new Pen(Color.FromArgb(55, 255, 255, 255), 1f))
            for (int i = 0; i < 9 && i < n; i++)
            {
              int cx = x0 + (i % 3) * (cell + gap), cy = y0 + (i / 3) * (cell + gap);
              using (var p = Round(new RectangleF(cx, cy, cell, cell), 10))
              { g.FillPath(tile, p); g.DrawPath(edge, p); }
              g.DrawImage(strip, new Rectangle(cx + (cell - icon) / 2, cy + (cell - icon) / 2, icon, icon),
                  new Rectangle(order[i] * 96, 0, 96, 96), GraphicsUnit.Pixel);
            }
        }

        using (var f4 = new Font("Segoe UI", 12f, GraphicsUnit.Pixel))
          Center(g, "based on 7-Zip 26.05", f4, gray, W, 448);
      }
      bmp.Save(outBmp, ImageFormat.Bmp);
      if (outPng != null) bmp.Save(outPng, ImageFormat.Png);
    }
  }
}
