// Read / write 32-bit alpha .bmp strips (BITMAPINFOHEADER, BI_RGB, bottom-up,
// straight alpha) as used by the 7zFM toolbar, plus preview pictures.
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Runtime.InteropServices;

public static class AlphaBmp
{
  public static Bitmap Read(string path)
  {
    byte[] b = File.ReadAllBytes(path);
    int off = BitConverter.ToInt32(b, 10);
    int w = BitConverter.ToInt32(b, 18);
    int h = BitConverter.ToInt32(b, 22);
    bool topDown = h < 0;
    h = Math.Abs(h);
    var bmp = new Bitmap(w, h, PixelFormat.Format32bppArgb);
    var data = bmp.LockBits(new Rectangle(0, 0, w, h), ImageLockMode.WriteOnly, bmp.PixelFormat);
    for (int y = 0; y < h; y++)
    {
      int srcY = topDown ? y : h - 1 - y;
      Marshal.Copy(b, off + srcY * w * 4, new IntPtr(data.Scan0.ToInt64() + (long)y * data.Stride), w * 4);
    }
    bmp.UnlockBits(data);
    return bmp;
  }

  public static void Write(Bitmap bmp, string path)
  {
    int w = bmp.Width, h = bmp.Height;
    var data = bmp.LockBits(new Rectangle(0, 0, w, h), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
    int imageSize = w * h * 4;
    byte[] b = new byte[54 + imageSize];
    b[0] = (byte)'B'; b[1] = (byte)'M';
    BitConverter.GetBytes(b.Length).CopyTo(b, 2);
    BitConverter.GetBytes(54).CopyTo(b, 10);
    BitConverter.GetBytes(40).CopyTo(b, 14);
    BitConverter.GetBytes(w).CopyTo(b, 18);
    BitConverter.GetBytes(h).CopyTo(b, 22);
    BitConverter.GetBytes((short)1).CopyTo(b, 26);
    BitConverter.GetBytes((short)32).CopyTo(b, 28);
    BitConverter.GetBytes(imageSize).CopyTo(b, 34);
    BitConverter.GetBytes(2835).CopyTo(b, 38);
    BitConverter.GetBytes(2835).CopyTo(b, 42);
    for (int y = 0; y < h; y++)
      Marshal.Copy(new IntPtr(data.Scan0.ToInt64() + (long)y * data.Stride), b, 54 + (h - 1 - y) * w * 4, w * 4);
    bmp.UnlockBits(data);
    // fully transparent pixels: black (like other strips)
    for (int i = 54; i < b.Length; i += 4)
      if (b[i + 3] == 0) { b[i] = 0; b[i + 1] = 0; b[i + 2] = 0; }
    File.WriteAllBytes(path, b);
  }

  // strips one under another, on light (top half) and dark (bottom half) background
  public static void Preview(string outPng, string[] files, int zoom)
  {
    var bmps = new Bitmap[files.Length];
    int pad = 8, w = 0, rows = 0;
    for (int i = 0; i < files.Length; i++)
    {
      bmps[i] = Read(files[i]);
      w = Math.Max(w, bmps[i].Width * zoom);
      rows += bmps[i].Height * zoom + pad;
    }
    int W = w + 2 * pad, H = 2 * (rows + pad);
    using (var canvas = new Bitmap(W, H))
    using (var g = Graphics.FromImage(canvas))
    {
      g.InterpolationMode = System.Drawing.Drawing2D.InterpolationMode.NearestNeighbor;
      g.PixelOffsetMode = System.Drawing.Drawing2D.PixelOffsetMode.Half;
      g.Clear(Color.FromArgb(236, 233, 216));
      using (var dark = new SolidBrush(Color.FromArgb(40, 40, 40)))
        g.FillRectangle(dark, 0, H / 2, W, H / 2);
      for (int half = 0; half < 2; half++)
      {
        int y = pad + half * H / 2;
        foreach (var bm in bmps)
        {
          g.DrawImage(bm, pad, y, bm.Width * zoom, bm.Height * zoom);
          y += bm.Height * zoom + pad;
        }
      }
      canvas.Save(outPng, ImageFormat.Png);
    }
  }
}
