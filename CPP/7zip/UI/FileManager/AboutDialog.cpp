// AboutDialog.cpp

#include "StdAfx.h"

#include <math.h>

#include "../../../../C/CpuArch.h"

#include "../../MyVersion.h"

#include "../Common/LoadCodecs.h"

#include "AboutDialog.h"
#include "DarkMode.h"
#include "resourceGui.h"
#include "PropertyNameRes.h"

#include "HelpUtils.h"
#include "LangUtils.h"

#ifdef Z7_LANG
static const UInt32 kLangIDs[] =
{
  IDT_ABOUT_INFO
};
#endif

extern HINSTANCE g_hInstance;

#define kHomePageURL TEXT("https://www.7-zip.org/")
#define kGitHubURL TEXT("https://github.com/saturn76-source/7zip-modern-ui")
#define kHelpTopic "start.htm"

#define LLL_(quote) L##quote
#define LLL(quote) LLL_(quote)

extern CCodecs *g_CodecsObj;

// ---------- animated logo ----------

static const DWORD kIntroTime = 1100;
static const DWORD kSpinTime = 1000;
static const DWORD kShineTime = 900;
static const double kPi = 3.14159265358979323846;

static double EaseOutBack(double p)
{
  const double c1 = 1.70158;
  const double c3 = c1 + 1;
  const double q = p - 1;
  return 1 + c3 * q * q * q + c1 * q * q;
}

static double EaseInOut(double p)
{
  return p < 0.5 ? 4 * p * p * p : 1 - (-2 * p + 2) * (-2 * p + 2) * (-2 * p + 2) / 2;
}

static HBITMAP CreateDib32(HDC dc, int w, int h, UInt32 **bits)
{
  BITMAPINFO bi;
  memset(&bi, 0, sizeof(bi));
  bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi.bmiHeader.biWidth = w;
  bi.bmiHeader.biHeight = -h; // top-down
  bi.bmiHeader.biPlanes = 1;
  bi.bmiHeader.biBitCount = 32;
  bi.bmiHeader.biCompression = BI_RGB;
  void *p = NULL;
  HBITMAP bmp = ::CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &p, NULL, 0);
  *bits = (UInt32 *)p;
  return bmp;
}

bool CAboutDialog::InitLogo()
{
  const HWND logo = GetItem(IDC_ABOUT_LOGO);
  if (!logo)
    return false;
  RECT r;
  ::GetClientRect(logo, &r);
  int s = r.bottom * 78 / 100;
  if (s > r.right)
    s = r.right;
  if (s < 8)
    return false;
  _iconSize = s;
  _icon = (HICON)::LoadImageW(g_hInstance, MAKEINTRESOURCEW(IDI_ICON), IMAGE_ICON, s, s, LR_DEFAULTCOLOR);
  if (!_icon)
    return false;
  const HDC screen = ::GetDC(NULL);
  _iconDC = ::CreateCompatibleDC(screen);
  _iconBmp = CreateDib32(screen, s, s, &_iconBits);
  ::ReleaseDC(NULL, screen);
  if (!_iconDC || !_iconBmp || !_iconBits)
  {
    FreeLogo();
    return false;
  }
  _iconOldBmp = ::SelectObject(_iconDC, _iconBmp);
  const size_t num = (size_t)s * (size_t)s;
  memset(_iconBits, 0, num * 4);
  ::DrawIconEx(_iconDC, 0, 0, _icon, s, s, 0, NULL, DI_NORMAL);
  ::GdiFlush();
  // some systems don't write alpha: make opaque pixels from colored ones
  bool hasAlpha = false;
  for (size_t i = 0; i < num; i++)
    if (_iconBits[i] & 0xFF000000)
    {
      hasAlpha = true;
      break;
    }
  if (!hasAlpha)
    for (size_t i = 0; i < num; i++)
      if (_iconBits[i] != 0)
        _iconBits[i] |= 0xFF000000;
  return true;
}

void CAboutDialog::FreeLogo()
{
  if (_iconDC)
  {
    if (_iconOldBmp)
      ::SelectObject(_iconDC, _iconOldBmp);
    ::DeleteDC(_iconDC);
    _iconDC = NULL;
  }
  _iconOldBmp = NULL;
  if (_iconBmp)
  {
    ::DeleteObject(_iconBmp);
    _iconBmp = NULL;
  }
  _iconBits = NULL;
  if (_icon)
  {
    ::DestroyIcon(_icon);
    _icon = NULL;
  }
}

static void Blend(HDC dest, int x, int y, int w, int h, HDC src, int sw, int sh, BYTE alpha)
{
  if (w <= 0 || h <= 0 || alpha == 0)
    return;
  BLENDFUNCTION bf;
  bf.BlendOp = AC_SRC_OVER;
  bf.BlendFlags = 0;
  bf.SourceConstantAlpha = alpha;
  bf.AlphaFormat = AC_SRC_ALPHA;
  ::GdiAlphaBlend(dest, x, y, w, h, src, 0, 0, sw, sh, bf);
}

void CAboutDialog::DrawLogo(HDC dc, const RECT &rect)
{
  const int W = rect.right - rect.left;
  const int H = rect.bottom - rect.top;
  if (W <= 0 || H <= 0)
    return;
  const COLORREF bkColor = NDarkMode::Color_Bk();

  UInt32 *bits = NULL;
  const HDC mem = ::CreateCompatibleDC(dc);
  const HBITMAP bmp = CreateDib32(dc, W, H, &bits);
  if (!mem || !bmp || !bits)
  {
    if (bmp) ::DeleteObject(bmp);
    if (mem) ::DeleteDC(mem);
    return;
  }
  const HGDIOBJ oldBmp = ::SelectObject(mem, bmp);
  {
    const HBRUSH br = ::CreateSolidBrush(bkColor);
    RECT r2 = { 0, 0, W, H };
    ::FillRect(mem, &r2, br);
    ::DeleteObject(br);
  }

  if (_iconBits)
  {
    const int S = _iconSize;
    const DWORD now = ::GetTickCount();
    const DWORD t = now - _startTime;

    // intro: zoom in with bounce
    double scale = 1;
    if (t < kIntroTime)
    {
      scale = EaseOutBack((double)t / kIntroTime);
      if (scale < 0)
        scale = 0;
    }

    // coin spin (at start and on click)
    double sx = 1;
    const DWORD ts = now - _spinTime;
    if (ts < kSpinTime)
    {
      const double a = 2 * kPi * EaseInOut((double)ts / kSpinTime);
      sx = cos(a);
      if (sx < 0)
        sx = -sx;
    }

    const double bob = 0; // no floating: logo stays still after animation
    const int floatAmp = (H - S) / 4;
    const int cx = W / 2;
    const int cy = S / 2 + (H - S) / 3 - (int)(bob * floatAmp);

    // shadow on the "floor"
    {
      const int shW = (int)(S * 0.75 * scale * (1.0 - bob * 0.15));
      const int shH = (H - S) / 3 + 2;
      const int shY = H - shH - 1;
      UInt32 *sb = NULL;
      const HDC sdc = ::CreateCompatibleDC(dc);
      const HBITMAP sbmp = (shW > 0 && shH > 0) ? CreateDib32(dc, shW, shH, &sb) : NULL;
      if (sdc && sbmp && sb)
      {
        const HGDIOBJ so = ::SelectObject(sdc, sbmp);
        const double strength = NDarkMode::IsDark() ? 0.75 : 0.35;
        for (int y = 0; y < shH; y++)
          for (int x = 0; x < shW; x++)
          {
            const double dx = (x + 0.5) / shW * 2 - 1;
            const double dy = (y + 0.5) / shH * 2 - 1;
            double v = 1 - (dx * dx + dy * dy);
            if (v < 0) v = 0;
            const unsigned a = (unsigned)(v * v * strength * (0.8 - bob * 0.2) * 255);
            sb[(size_t)y * (size_t)shW + (size_t)x] = (UInt32)a << 24; // premultiplied black
          }
        ::GdiFlush();
        Blend(mem, cx - shW / 2, shY, shW, shH, sdc, shW, shH, 255);
        ::SelectObject(sdc, so);
      }
      if (sbmp) ::DeleteObject(sbmp);
      if (sdc) ::DeleteDC(sdc);
    }

    // logo
    const int h = (int)(S * scale);
    const int w = (int)(S * scale * sx);
    const int x = cx - w / 2;
    const int y = cy - h / 2;
    Blend(mem, x, y, w, h, _iconDC, S, S, 255);

    // shine: light stripe crossing the logo
    {
      DWORD phase = (DWORD)-1;
      if (ts < kSpinTime + kShineTime && ts >= kSpinTime)
        phase = ts - kSpinTime;
      if (phase < kShineTime && w > 0 && h > 0)
      {
        const double pos = -0.3 + 1.6 * (double)phase / kShineTime;
        UInt32 *lb = NULL;
        const HDC ldc = ::CreateCompatibleDC(dc);
        const HBITMAP lbmp = CreateDib32(dc, S, S, &lb);
        if (ldc && lbmp && lb)
        {
          const HGDIOBJ lo = ::SelectObject(ldc, lbmp);
          for (int yy = 0; yy < S; yy++)
            for (int xx = 0; xx < S; xx++)
            {
              const size_t i = (size_t)yy * (size_t)S + (size_t)xx;
              const unsigned ia = _iconBits[i] >> 24;
              UInt32 v = 0;
              if (ia != 0)
              {
                const double d = ((double)xx + (double)yy * 0.5) / (S * 1.5) - pos;
                const double k = exp(-(d * d) / (0.06 * 0.06)) * 0.7;
                const unsigned a = (unsigned)(k * ia);
                v = (a << 24) | (a << 16) | (a << 8) | a; // premultiplied white
              }
              lb[i] = v;
            }
          ::GdiFlush();
          Blend(mem, x, y, w, h, ldc, S, S, 255);
          ::SelectObject(ldc, lo);
        }
        if (lbmp) ::DeleteObject(lbmp);
        if (ldc) ::DeleteDC(ldc);
      }
    }
  }

  ::BitBlt(dc, rect.left, rect.top, W, H, mem, 0, 0, SRCCOPY);
  ::SelectObject(mem, oldBmp);
  ::DeleteObject(bmp);
  ::DeleteDC(mem);
}

bool CAboutDialog::OnMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
  if (message == WM_DRAWITEM && wParam == IDC_ABOUT_LOGO)
  {
    const DRAWITEMSTRUCT *dis = (const DRAWITEMSTRUCT *)lParam;
    DrawLogo(dis->hDC, dis->rcItem);
    return true;
  }
  if (message == WM_SETCURSOR && (HWND)wParam == GetItem(IDC_ABOUT_LOGO))
  {
    ::SetCursor(::LoadCursor(NULL, IDC_HAND));
    ::SetWindowLongPtrW(*this, DWLP_MSGRESULT, TRUE);
    return true;
  }
  return CModalDialog::OnMessage(message, wParam, lParam);
}

bool CAboutDialog::OnTimer(WPARAM /* timerID */, LPARAM /* callback */)
{
  const HWND logo = GetItem(IDC_ABOUT_LOGO);
  if (logo)
    ::InvalidateRect(logo, NULL, FALSE);
  // animation is finished: stop timer, the logo stays frozen
  const DWORD now = ::GetTickCount();
  if (now - _startTime > kIntroTime + 50 && now - _spinTime > kSpinTime + kShineTime + 50)
    KillTimer(1);
  return true;
}

bool CAboutDialog::OnDestroy()
{
  KillTimer(1);
  FreeLogo();
  return CModalDialog::OnDestroy();
}

bool CAboutDialog::OnInit()
{
  #ifdef Z7_EXTERNAL_CODECS
  if (g_CodecsObj)
  {
    UString s;
    g_CodecsObj->GetCodecsErrorMessage(s);
    if (!s.IsEmpty())
      MessageBoxW(GetParent(), s, L"7-Zip", MB_ICONERROR);
  }
  #endif

  #ifdef Z7_LANG
  LangSetWindowText(*this, IDD_ABOUT);
  LangSetDlgItems(*this, kLangIDs, Z7_ARRAY_SIZE(kLangIDs));
  #endif
  SetItemText(IDT_ABOUT_VERSION, UString("7-Zip " MY_VERSION_CPU));
  SetItemText(IDT_ABOUT_DATE, LLL(MY_DATE));
  SetItemText(IDT_ABOUT_EDITION, UString("7-Zip Modern UI edition v1.01, based on 7-Zip " MY_VERSION " source code"));

  if (InitLogo())
  {
    _startTime = ::GetTickCount();
    _spinTime = _startTime;
    SetTimer(1, 15);
  }

  NormalizePosition();
  return CModalDialog::OnInit();
}

void CAboutDialog::OnHelp()
{
  ShowHelpWindow(kHelpTopic);
}

bool CAboutDialog::OnButtonClicked(unsigned buttonID, HWND buttonHWND)
{
  LPCTSTR url;
  switch (buttonID)
  {
    case IDB_ABOUT_HOMEPAGE: url = kHomePageURL; break;
    case IDB_ABOUT_GITHUB: url = kGitHubURL; break;
    case IDC_ABOUT_LOGO:
      // click on logo: spin it again
      if (_iconBits && ::GetTickCount() - _spinTime > kSpinTime)
      {
        _spinTime = ::GetTickCount();
        SetTimer(1, 15);
      }
      return true;
    default:
      return CModalDialog::OnButtonClicked(buttonID, buttonHWND);
  }

  #ifdef UNDER_CE
  SHELLEXECUTEINFO s;
  memset(&s, 0, sizeof(s));
  s.cbSize = sizeof(s);
  s.lpFile = url;
  ::ShellExecuteEx(&s);
  #else
  ::ShellExecute(NULL, NULL, url, NULL, NULL, SW_SHOWNORMAL);
  #endif

  return true;
}
