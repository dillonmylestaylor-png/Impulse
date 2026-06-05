#pragma once

#include <cmath>
#include <chrono>
#include <sstream>
#include <unordered_map>
#include "Colors.h"
#include "IControls.h"

#define PLUG() static_cast<PLUG_CLASS_NAME*>(GetDelegate())
#define NAM_KNOB_HEIGHT 200.0f
#define NAM_SWTICH_HEIGHT 50.0f

using namespace iplug;
using namespace igraphics;

enum class NAMBrowserState
{
  Empty, // when no file loaded, show "Get" button
  Loaded // when file loaded, show "Clear" button
};

// Where the corner button on the plugin (settings, close settings) goes
// :param rect: Rect for the whole plugin's UI
IRECT CornerButtonArea(const IRECT& rect)
{
  const auto mainArea = rect.GetPadded(-20);
  return mainArea.GetFromTRHC(50, 50).GetCentredInside(20, 20);
};

class NAMBackgroundControl : public IControl
{
public:
  NAMBackgroundControl(const IRECT& bounds, const IColor& color = COLOR_BLACK)
  : IControl(bounds), mColor(color)
  {
  }

  void Draw(IGraphics& g) override
  {
    g.FillRect(mColor, mRECT);
  }

private:
  IColor mColor;
};

class NAMPanelControl : public IControl
{
public:
  NAMPanelControl(const IRECT& bounds, const IColor& color)
  : IControl(bounds), mColor(color)
  {
    mIgnoreMouse = true;
  }

  void Draw(IGraphics& g) override
  {
    g.FillRect(mColor, mRECT);
  }

private:
  IColor mColor;
};

class NAMLineControl : public IControl
{
public:
  NAMLineControl(const IRECT& bounds, const IColor& color, float thickness = 1.0f)
  : IControl(bounds), mColor(color), mThickness(thickness)
  {
    mIgnoreMouse = true;
  }

  void Draw(IGraphics& g) override
  {
    g.DrawLine(mColor, mRECT.L, mRECT.T, mRECT.R, mRECT.B, nullptr, mThickness);
  }

private:
  IColor mColor;
  float mThickness;
};

class NAMAmpImageControl : public IControl, public IBitmapBase
{
public:
  NAMAmpImageControl(const IRECT& bounds, const IBitmap& bitmap, bool fill = false)
  : IControl(bounds), IBitmapBase(bitmap), mFill(fill)
  {
    mIgnoreMouse = true;
  }

  void OnRescale() override { mBitmap = GetUI()->GetScaledBitmap(mBitmap); }

  void Draw(IGraphics& g) override
  {
    float scale = mFill
      ? std::max(mRECT.W() / static_cast<float>(mBitmap.W()),
                 mRECT.H() / static_cast<float>(mBitmap.H()))
      : std::min(mRECT.W() / static_cast<float>(mBitmap.W()),
                 mRECT.H() / static_cast<float>(mBitmap.H()));
    float w = mBitmap.W() * scale;
    float h = mBitmap.H() * scale;
    IRECT r = mRECT.GetCentredInside(w, h);
    g.DrawFittedBitmap(mBitmap, r);
  }

private:
  bool mFill;
};

class NAMSquareButtonControl : public ISVGButtonControl
{
public:
  NAMSquareButtonControl(const IRECT& bounds, IActionFunction af, const ISVG& svg)
  : ISVGButtonControl(bounds, af, svg, svg)
  {
  }

  void Draw(IGraphics& g) override
  {
    if (mMouseIsOver)
      g.FillRoundRect(PluginColors::MOUSEOVER, mRECT, 2.f);

    ISVGButtonControl::Draw(g);
  }
};

class NAMCircleButtonControl : public ISVGButtonControl
{
public:
  NAMCircleButtonControl(const IRECT& bounds, IActionFunction af, const ISVG& svg)
  : ISVGButtonControl(bounds, af, svg, svg)
  {
  }

  void Draw(IGraphics& g) override
  {
    if (mMouseIsOver)
      g.FillEllipse(PluginColors::MOUSEOVER, mRECT);

    ISVGButtonControl::Draw(g);
  }
};

class NAMPhaseFlipControl : public ISwitchControlBase
{
public:
  NAMPhaseFlipControl(const IRECT& bounds, int paramIdx)
  : ISwitchControlBase(bounds, paramIdx, nullptr, 2)
  {
  }

  void Draw(IGraphics& g) override
  {
    const bool active = GetSelectedIdx() > 0;
    const auto r = mRECT;
    const float cx = r.MW(), cy = r.MH();
    const float radius = std::min(r.W(), r.H()) * 0.5f;
    const auto circleRect = IRECT(cx - radius, cy - radius, cx + radius, cy + radius);
    IText symText(static_cast<int>(r.H() * 0.55f), active ? COLOR_WHITE : COLOR_WHITE, "Roboto-Regular",
                  EAlign::Center, EVAlign::Middle);

    if (mMouseIsOver)
      g.FillEllipse(PluginColors::MOUSEOVER, circleRect);

    if (active)
    {
      g.FillEllipse(PluginColors::NAM_2, circleRect);
    }
    else
    {
      g.DrawEllipse(COLOR_WHITE, circleRect, nullptr, 1.5f);
    }

    g.DrawText(symText, "\xC3\x98", r);
  }
};

class NAMMuteControl : public ISwitchControlBase
{
public:
  NAMMuteControl(const IRECT& bounds, int paramIdx)
  : ISwitchControlBase(bounds, paramIdx, nullptr, 2)
  {
  }

  void Draw(IGraphics& g) override
  {
    const bool active = GetSelectedIdx() > 0;
    const auto r = mRECT;
    const float cx = r.MW(), cy = r.MH();
    const float radius = std::min(r.W(), r.H()) * 0.5f;
    const auto circleRect = IRECT(cx - radius, cy - radius, cx + radius, cy + radius);
    IText symText(static_cast<int>(r.H() * 0.55f), active ? COLOR_WHITE : COLOR_WHITE, "Roboto-Regular",
                  EAlign::Center, EVAlign::Middle);

    if (mMouseIsOver)
      g.FillEllipse(PluginColors::MOUSEOVER, circleRect);

    if (active)
    {
      g.FillEllipse(PluginColors::NAM_2, circleRect);
    }
    else
    {
      g.DrawEllipse(COLOR_WHITE, circleRect, nullptr, 1.5f);
    }

    g.DrawText(symText, "M", r);
  }
};

class NAMDelayControl : public IControl
{
public:
  NAMDelayControl(const IRECT& bounds, int paramIdx)
  : IControl(bounds, paramIdx)
  {
  }

  void HideLabel() { mHideLabel = true; }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    const auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - mLastClick).count() < 500)
      SetValueToDefault();
    mLastClick = now;
  }

  void Draw(IGraphics& g) override
  {
    const int delay = static_cast<int>(GetValue() * 99.0);
    const auto r = mRECT;
    const float cx = r.MW();
    const float radius = 13.0f;
    const float cy = r.MH();
    const auto circleRect = IRECT(cx - radius, cy - radius, cx + radius, cy + radius);

    IText valText(static_cast<int>(radius * 0.9f), COLOR_WHITE, "Roboto-Regular",
                  EAlign::Center, EVAlign::Middle);
    IText labelText(11, COLOR_WHITE, "Roboto-Regular",
                    EAlign::Center, EVAlign::Top);

    if (mMouseIsOver)
      g.FillEllipse(PluginColors::MOUSEOVER, circleRect);

    g.DrawEllipse(COLOR_WHITE, circleRect, nullptr, 1.5f);

    char buf[8];
    snprintf(buf, sizeof(buf), "%d", delay);
    g.DrawText(valText, buf, circleRect);
    if (!mHideLabel)
    {
      const auto labelRect = IRECT(r.L, circleRect.B + 2, r.R, r.B);
      g.DrawText(labelText, "Delay", labelRect);
    }
  }

  void OnMouseDrag(float x, float y, float dX, float dY, const IMouseMod& mod) override
  {
    const double range = 99.0;
    double val = GetValue() - dY * 0.005;
    val = std::max(0.0, std::min(1.0, val));
    SetValue(val);
    SetDirty(true);
  }

  void OnMouseWheel(float x, float y, const IMouseMod& mod, float d) override
  {
    double val = GetValue() + (d > 0 ? 0.01 : -0.01);
    val = std::max(0.0, std::min(1.0, val));
    SetValue(val);
    SetDirty(true);
  }

private:
  bool mHideLabel = false;
  std::chrono::steady_clock::time_point mLastClick;
};

class NAMKnobControl : public IVKnobControl, public IBitmapBase
{
public:
  NAMKnobControl(const IRECT& bounds, int paramIdx, const char* label, const IVStyle& style, IBitmap bitmap)
  : IVKnobControl(bounds, paramIdx, label, style, true)
  , IBitmapBase(bitmap)
  {
    mInnerPointerFrac = 0.55;
    mHideCursorOnDrag = false;
  }

  void HideLabel() { mStyle.showLabel = false; }
  void HideValue() { mStyle.showValue = false; }

  void OnRescale() override { mBitmap = GetUI()->GetScaledBitmap(mBitmap); }

  bool IsHit(float x, float y) const override
  {
    return mRECT.Contains(x, y) || mLabelBounds.Contains(x, y);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    using namespace std::chrono;
    static steady_clock::time_point sLastClick;
    static int sLastParam = -1;

    auto now = steady_clock::now();
    auto dt = duration<double>(now - sLastClick).count();

    if (GetParamIdx() == sLastParam && dt < 0.3)
    {
      sLastParam = -1;
      if (auto* p = GetParam())
      {
        SetValueFromUserInput(p->GetDefault(true), 0);
        GetDelegate()->SendParameterValueFromUI(GetParamIdx(), p->GetDefault(true));
        GetUI()->SetAllControlsDirty();
        GetUI()->ReleaseMouseCapture();
      }
      return;
    }

    sLastParam = GetParamIdx();
    sLastClick = now;
    IVKnobControl::OnMouseDown(x, y, mod);
  }

  void OnMouseDblClick(float x, float y, const IMouseMod& mod) override
  {
    if (auto* p = GetParam())
    {
      double def = p->GetDefault(true);
      SetValueFromUserInput(def, 0);
      GetDelegate()->SendParameterValueFromUI(GetParamIdx(), def);
    }
  }

  void DrawWidget(IGraphics& g) override
  {
    float widgetRadius = GetRadius() * 0.73;
    float knobSize = std::min(mWidgetBounds.W(), mWidgetBounds.H());
    auto knobRect = mWidgetBounds.GetCentredInside(knobSize, knobSize);
    const float cx = knobRect.MW(), cy = knobRect.MH();
    const float angle = mAngle1 + (static_cast<float>(GetValue()) * (mAngle2 - mAngle1));
    g.DrawFittedBitmap(mBitmap, knobRect);
    float data[2][2];
    RadialPoints(angle, cx, cy, mInnerPointerFrac * widgetRadius, mInnerPointerFrac * widgetRadius, 2, data);
    g.PathCircle(data[1][0], data[1][1], 3);
    g.PathFill(COLOR_BLACK, &mBlend);
    g.DrawCircle(COLOR_BLACK.WithOpacity(0.5f), data[1][0], data[1][1], 3, &mBlend);
  }
};

class NAMGateKnobControl : public NAMKnobControl
{
public:
  NAMGateKnobControl(const IRECT& bounds, int paramIdx, const char* label, const IVStyle& style, IBitmap bitmap)
  : NAMKnobControl(bounds, paramIdx, label, style, bitmap)
  {
  }

  bool IsHit(float x, float y) const override
  {
    return mLabelBounds.Contains(x, y) || mWidgetBounds.Contains(x, y) || mValueBounds.Contains(x, y);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    if (mLabelBounds.Contains(x, y))
    {
      using namespace std::chrono;
      static steady_clock::time_point sLastClick;
      static int sLastParam = -1;

      auto now = steady_clock::now();
      auto dt = duration<double>(now - sLastClick).count();

      if (GetParamIdx() == sLastParam && dt < 0.3)
      {
        sLastParam = -1;
        if (auto* p = GetParam())
        {
          SetValueFromUserInput(p->GetDefault(true), 0);
          GetDelegate()->SendParameterValueFromUI(GetParamIdx(), p->GetDefault(true));
          GetUI()->SetAllControlsDirty();
          GetUI()->ReleaseMouseCapture();
        }
        return;
      }

      sLastParam = GetParamIdx();
      sLastClick = now;
      _ToggleGate();
      GetUI()->ReleaseMouseCapture();
    }
    else
    {
      if (mValueBounds.Contains(x, y))
        NAMKnobControl::OnMouseDown(x, y, mod);
      else
      {
        _EnsureGateOn();
        NAMKnobControl::OnMouseDown(x, y, mod);
      }
    }
  }

  void OnMouseDblClick(float x, float y, const IMouseMod& mod) override
  {
    NAMKnobControl::OnMouseDblClick(x, y, mod);
  }

  void _ToggleGate()
  {
    auto* pPlug = static_cast<PLUG_CLASS_NAME*>(GetDelegate());
    const int idx = kNoiseGateActive;
    pPlug->SendParameterValueFromUI(idx, pPlug->GetParam(idx)->Bool() ? 0.0 : 1.0);
  }

  void _EnsureGateOn()
  {
    auto* pPlug = static_cast<PLUG_CLASS_NAME*>(GetDelegate());
    const int idx = kNoiseGateActive;
    if (!pPlug->GetParam(idx)->Bool())
      pPlug->SendParameterValueFromUI(idx, 1.0);
  }
};

class NAMSwitchControl : public IVSlideSwitchControl, public IBitmapBase
{
public:
  NAMSwitchControl(const IRECT& bounds, int paramIdx, const char* label, const IVStyle& style, IBitmap bitmap)
  : IVSlideSwitchControl(bounds, paramIdx, label,
                         style.WithRoundness(0.666f)
                           .WithShowValue(false)
                           .WithEmboss(true)
                           .WithShadowOffset(1.5f)
                           .WithDrawShadows(false)
                           .WithColor(kFR, COLOR_BLACK)
                           .WithFrameThickness(0.5f)
                           .WithWidgetFrac(0.5f)
                           .WithLabelOrientation(EOrientation::South))
  , IBitmapBase(bitmap)
  {
  }

  void Draw(IGraphics& g) override
  {
    DrawBackground(g, mRECT);
    DrawWidget(g);
  }

  void DrawWidget(IGraphics& g) override
  {
    DrawTrack(g, mWidgetBounds);
    DrawHandle(g, mHandleBounds);
  }

  void DrawTrack(IGraphics& g, const IRECT& bounds) override
  {
    IRECT handleBounds = GetAdjustedHandleBounds(bounds);
    handleBounds = IRECT(handleBounds.L, handleBounds.T, handleBounds.R, handleBounds.T + mBitmap.H());
    IRECT centreBounds = handleBounds.GetPadded(-mStyle.shadowOffset);
    IRECT shadowBounds = handleBounds.GetTranslated(mStyle.shadowOffset, mStyle.shadowOffset);
    //    const float contrast = mDisabled ? -GRAYED_ALPHA : 0.f;
    float cR = 7.f;
    const float tlr = cR;
    const float trr = cR;
    const float blr = cR;
    const float brr = cR;

    // outer shadow
    if (mStyle.drawShadows)
      g.FillRoundRect(GetColor(kSH), shadowBounds, tlr, trr, blr, brr, &mBlend);

    // Embossed style unpressed
    if (mStyle.emboss)
    {
      // Positive light
      g.FillRoundRect(GetColor(kPR), handleBounds, tlr, trr, blr, brr /*, &blend*/);

      // Negative light
      g.FillRoundRect(GetColor(kSH), shadowBounds, tlr, trr, blr, brr /*, &blend*/);

      // Fill in foreground
      g.FillRoundRect(GetValue() > 0.5 ? GetColor(kX1) : COLOR_BLACK, centreBounds, tlr, trr, blr, brr, &mBlend);

      // Shade when hovered
      if (mMouseIsOver)
        g.FillRoundRect(GetColor(kHL), centreBounds, tlr, trr, blr, brr, &mBlend);
    }
    else
    {
      g.FillRoundRect(GetValue() > 0.5 ? GetColor(kX1) : COLOR_BLACK, handleBounds, tlr, trr, blr, brr /*, &blend*/);

      // Shade when hovered
      if (mMouseIsOver)
        g.FillRoundRect(GetColor(kHL), handleBounds, tlr, trr, blr, brr, &mBlend);
    }

    if (mStyle.drawFrame)
      g.DrawRoundRect(GetColor(kFR), handleBounds, tlr, trr, blr, brr, &mBlend, mStyle.frameThickness);
  }

  void DrawHandle(IGraphics& g, const IRECT& filledArea) override
  {
    IRECT r;
    if (GetSelectedIdx() == 0)
    {
      r = filledArea.GetFromLeft(mBitmap.W());
    }
    else
    {
      r = filledArea.GetFromRight(mBitmap.W());
    }

    g.DrawBitmap(mBitmap, r, 0, 0, nullptr);
  }
};

class NAMFileNameControl : public IVButtonControl
{
public:
  NAMFileNameControl(const IRECT& bounds, const char* label, const IVStyle& style)
  : IVButtonControl(bounds, DefaultClickActionFunc, label, style)
  {
  }

  void SetLabelAndTooltip(const char* str)
  {
    SetLabelStr(str);
    SetTooltip(str);
  }

  void SetLabelAndTooltipEllipsizing(const WDL_String& fileName)
  {
    auto EllipsizeFilePath = [](const char* filePath, size_t prefixLength, size_t suffixLength, size_t maxLength) {
      const std::string ellipses = "...";
      assert(maxLength <= (prefixLength + suffixLength + ellipses.size()));
      std::string str{filePath};

      if (str.length() <= maxLength)
      {
        return str;
      }
      else
      {
        return str.substr(0, prefixLength) + ellipses + str.substr(str.length() - suffixLength);
      }
    };

    auto ellipsizedFileName = EllipsizeFilePath(fileName.get_filepart(), 22, 22, 45);
    SetLabelStr(ellipsizedFileName.c_str());
    SetTooltip(fileName.get_filepart());
  }
};

// URL control for the "Get" models/irs links
class NAMGetButtonControl : public NAMSquareButtonControl
{
public:
  NAMGetButtonControl(const IRECT& bounds, const char* label, const char* url, const ISVG& globeSVG)
  : NAMSquareButtonControl(
      bounds,
      [url](IControl* pCaller) {
        WDL_String fullURL(url);
        pCaller->GetUI()->OpenURL(fullURL.Get());
      },
      globeSVG)
  {
    SetTooltip(label);
  }
};

class NAMFileBrowserControl : public IDirBrowseControlBase
{
public:
  NAMFileBrowserControl(const IRECT& bounds, int clearMsgTag, const char* labelStr, const char* fileExtension,
                        IFileDialogCompletionHandlerFunc ch, const IVStyle& style, const ISVG& loadSVG,
                        const ISVG& clearSVG, const ISVG& leftSVG, const ISVG& rightSVG, const IBitmap& bitmap,
                        const ISVG& globeSVG, const char* getButtonLabel, const char* getButtonURL)
  : IDirBrowseControlBase(bounds, fileExtension, false, false)
  , mClearMsgTag(clearMsgTag)
  , mDefaultLabelStr(labelStr)
  , mCompletionHandlerFunc(ch)
  , mStyle(style.WithColor(kFG, COLOR_TRANSPARENT).WithDrawFrame(false))
  , mBitmap(bitmap)
  , mLoadSVG(loadSVG)
  , mClearSVG(clearSVG)
  , mLeftSVG(leftSVG)
  , mRightSVG(rightSVG)
  , mGlobeSVG(globeSVG)
  , mGetButtonLabel(getButtonLabel)
  , mGetButtonURL(getButtonURL)
  , mBrowserState(NAMBrowserState::Empty)
  {
    mIgnoreMouse = true;
  }

  void Draw(IGraphics& g) override { g.DrawFittedBitmap(mBitmap, mRECT); }

  void OnPopupMenuSelection(IPopupMenu* pSelectedMenu, int valIdx) override
  {
    if (pSelectedMenu)
    {
      IPopupMenu::Item* pItem = pSelectedMenu->GetChosenItem();

      if (pItem)
      {
        mSelectedItemIndex = mItems.Find(pItem);
        LoadFileAtCurrentIndex();
      }
    }
  }

  void OnAttached() override
  {
    auto prevFileFunc = [&](IControl* pCaller) {
      const auto nItems = NItems();
      if (nItems == 0)
        return;
      mSelectedItemIndex--;

      if (mSelectedItemIndex < 0)
        mSelectedItemIndex = nItems - 1;

      LoadFileAtCurrentIndex();
    };

    auto nextFileFunc = [&](IControl* pCaller) {
      const auto nItems = NItems();
      if (nItems == 0)
        return;
      mSelectedItemIndex++;

      if (mSelectedItemIndex >= nItems)
        mSelectedItemIndex = 0;

      LoadFileAtCurrentIndex();
    };

    auto loadFileFunc = [&](IControl* pCaller) {
      WDL_String fileName;
      WDL_String path;
      GetSelectedFileDirectory(path);
#ifdef NAM_PICK_DIRECTORY
      pCaller->GetUI()->PromptForDirectory(path, [&](const WDL_String& fileName, const WDL_String& path) {
        if (path.GetLength())
        {
          ClearPathList();
          AddPath(path.Get(), "");
          SetupMenu();
          SelectFirstFile();
          LoadFileAtCurrentIndex();
        }
      });
#else
      pCaller->GetUI()->PromptForFile(
        fileName, path, EFileAction::Open, mExtension.Get(), [&](const WDL_String& fileName, const WDL_String& path) {
          if (fileName.GetLength())
          {
            ClearPathList();
            AddPath(path.Get(), "");
            SetupMenu();
            SetSelectedFile(fileName.Get());
            LoadFileAtCurrentIndex();
          }
        });
#endif
    };

    auto clearFileFunc = [&](IControl* pCaller) {
      pCaller->GetDelegate()->SendArbitraryMsgFromUI(mClearMsgTag);
      mFileNameControl->SetLabelAndTooltip(mDefaultLabelStr.Get());
      SetBrowserState(NAMBrowserState::Empty);
      // FIXME disabling output mode...
      //      pCaller->GetUI()->GetControlWithTag(kCtrlTagOutputMode)->SetDisabled(false);
    };

    auto chooseFileFunc = [&, loadFileFunc](IControl* pCaller) {
      if (std::string_view(pCaller->As<IVButtonControl>()->GetLabelStr()) == mDefaultLabelStr.Get())
      {
        loadFileFunc(pCaller);
      }
      else
      {
        CheckSelectedItem();

        if (!mMainMenu.HasSubMenus())
        {
          mMainMenu.SetChosenItemIdx(mSelectedItemIndex);
        }
        pCaller->GetUI()->CreatePopupMenu(*this, mMainMenu, pCaller->GetRECT());
      }
    };

    IRECT padded = mRECT.GetPadded(-6.f).GetHPadded(-2.f);
    const auto buttonWidth = padded.H();
    const auto loadFileButtonBounds = padded.ReduceFromLeft(buttonWidth);
    const auto clearAndGetButtonBounds = padded.ReduceFromRight(buttonWidth);
    const auto leftButtonBounds = padded.ReduceFromLeft(buttonWidth);
    const auto rightButtonBounds = padded.ReduceFromLeft(buttonWidth);
    const auto fileNameButtonBounds = padded;

    AddChildControl(new NAMSquareButtonControl(loadFileButtonBounds, DefaultClickActionFunc, mLoadSVG))
      ->SetAnimationEndActionFunction(loadFileFunc);
    AddChildControl(new NAMSquareButtonControl(leftButtonBounds, DefaultClickActionFunc, mLeftSVG))
      ->SetAnimationEndActionFunction(prevFileFunc);
    AddChildControl(new NAMSquareButtonControl(rightButtonBounds, DefaultClickActionFunc, mRightSVG))
      ->SetAnimationEndActionFunction(nextFileFunc);
    AddChildControl(mFileNameControl = new NAMFileNameControl(fileNameButtonBounds, mDefaultLabelStr.Get(), mStyle))
      ->SetAnimationEndActionFunction(chooseFileFunc);

    // creates both right-side controls but only show one based on state
    mClearButton = new NAMSquareButtonControl(clearAndGetButtonBounds, DefaultClickActionFunc, mClearSVG);
    mClearButton->SetAnimationEndActionFunction(clearFileFunc);
    AddChildControl(mClearButton);

    mGetButton = new NAMGetButtonControl(clearAndGetButtonBounds, mGetButtonLabel, mGetButtonURL, mGlobeSVG);
    AddChildControl(mGetButton);

    // initialize control visibility
    SetBrowserState(NAMBrowserState::Empty);
  }

  void LoadFileAtCurrentIndex()
  {
    if (mSelectedItemIndex > -1 && mSelectedItemIndex < NItems())
    {
      WDL_String fileName, path;
      GetSelectedFile(fileName);
      mFileNameControl->SetLabelAndTooltipEllipsizing(fileName);
      mCompletionHandlerFunc(fileName, path);
    }
  }

  void OnMsgFromDelegate(int msgTag, int dataSize, const void* pData) override
  {
    switch (msgTag)
    {
      case kMsgTagLoadFailed:
      case kMsgTagLoadFailedIR2:
      case kMsgTagLoadFailedIR3:
      case kMsgTagLoadFailedIR4:
        {
          std::string label(std::string("(FAILED) ") + std::string(mFileNameControl->GetLabelStr()));
          mFileNameControl->SetLabelAndTooltip(label.c_str());
          SetBrowserState(NAMBrowserState::Empty);
        }
        break;
      case kMsgTagLoadedIR:
      case kMsgTagLoadedIR2:
      case kMsgTagLoadedIR3:
      case kMsgTagLoadedIR4:
      {
        WDL_String fileName, directory;
        fileName.Set(reinterpret_cast<const char*>(pData));
        directory.Set(reinterpret_cast<const char*>(pData));
        directory.remove_filepart(true);

        ClearPathList();
        AddPath(directory.Get(), "");
        SetupMenu();
        SetSelectedFile(fileName.Get());
        mFileNameControl->SetLabelAndTooltipEllipsizing(fileName);
        SetBrowserState(NAMBrowserState::Loaded);
      }
      break;
      default: break;
    }
  }

private:
  void SelectFirstFile() { mSelectedItemIndex = mFiles.GetSize() ? 0 : -1; }

  void GetSelectedFileDirectory(WDL_String& path)
  {
    GetSelectedFile(path);
    path.remove_filepart();
    return;
  }

  // set the state of the browser and the visibility of the "Get" vs. "Clear" buttons
  void SetBrowserState(NAMBrowserState newState)
  {
    mBrowserState = newState;

    switch (mBrowserState)
    {
      case NAMBrowserState::Empty:
        mClearButton->Hide(true);
        mGetButton->Hide(false);
        break;
      case NAMBrowserState::Loaded:
        mClearButton->Hide(false);
        mGetButton->Hide(true);
        break;
    }
  }

  WDL_String mDefaultLabelStr;
  IFileDialogCompletionHandlerFunc mCompletionHandlerFunc;
  NAMFileNameControl* mFileNameControl = nullptr;
  IVStyle mStyle;
  IBitmap mBitmap;
  ISVG mLoadSVG, mClearSVG, mLeftSVG, mRightSVG, mGlobeSVG;
  int mClearMsgTag;

  // new members for the "Get" button
  const char* mGetButtonLabel;
  const char* mGetButtonURL;
  NAMBrowserState mBrowserState;
  NAMSquareButtonControl* mClearButton = nullptr;
  NAMGetButtonControl* mGetButton = nullptr;
};

class NAMMeterControl : public IVPeakAvgMeterControl<>, public IBitmapBase
{
  static constexpr float KMeterMin = -70.0f;
  static constexpr float KMeterMax = -0.01f;

public:
  NAMMeterControl(const IRECT& bounds, const IBitmap& bitmap, const IVStyle& style)
  : IVPeakAvgMeterControl<>(bounds, "", style.WithShowValue(false).WithDrawFrame(false).WithWidgetFrac(0.8),
                            EDirection::Vertical, {}, 0, KMeterMin, KMeterMax, {})
  , IBitmapBase(bitmap)
  {
    SetPeakSize(1.0f);
  }

  void OnRescale() override { mBitmap = GetUI()->GetScaledBitmap(mBitmap); }

  virtual void OnResize() override
  {
    SetTargetRECT(MakeRects(mRECT));
    mWidgetBounds = mWidgetBounds.GetMidHPadded(5).GetVPadded(10);
    MakeTrackRects(mWidgetBounds);
    MakeStepRects(mWidgetBounds, mNSteps);
    SetDirty(false);
  }

  void DrawBackground(IGraphics& g, const IRECT& r) override { g.DrawFittedBitmap(mBitmap, r); }

  void DrawTrackHandle(IGraphics& g, const IRECT& r, int chIdx, bool aboveBaseValue) override
  {
    if (r.H() > 2)
      g.FillRect(GetColor(kX1), r, &mBlend);
  }

  void DrawPeak(IGraphics& g, const IRECT& r, int chIdx, bool aboveBaseValue) override
  {
    g.FillRect(GetColor(kX3), r, &mBlend);
  }
};

// Container where we can refer to children by names instead of indices
class IContainerBaseWithNamedChildren : public IContainerBase
{
public:
  IContainerBaseWithNamedChildren(const IRECT& bounds)
  : IContainerBase(bounds) {};
  ~IContainerBaseWithNamedChildren() = default;

protected:
  IControl* AddNamedChildControl(IControl* control, std::string name, int ctrlTag = kNoTag, const char* group = "")
  {
    // Make sure we haven't already used this name
    assert(mChildNameIndexMap.find(name) == mChildNameIndexMap.end());
    mChildNameIndexMap[name] = NChildren();
    return AddChildControl(control, ctrlTag, group);
  };

  IControl* GetNamedChild(std::string name)
  {
    const int index = mChildNameIndexMap[name];
    return GetChild(index);
  };


private:
  std::unordered_map<std::string, int> mChildNameIndexMap;
}; // class IContainerBaseWithNamedChildren


struct PossiblyKnownParameter
{
  bool known = false;
  double value = 0.0;
};

struct ModelInfo
{
  PossiblyKnownParameter sampleRate;
  PossiblyKnownParameter inputCalibrationLevel;
  PossiblyKnownParameter outputCalibrationLevel;
};

class ModelInfoControl : public IContainerBaseWithNamedChildren
{
public:
  ModelInfoControl(const IRECT& bounds, const IVStyle& style)
  : IContainerBaseWithNamedChildren(bounds)
  , mStyle(style) {};

  void ClearModelInfo()
  {
    static_cast<IVLabelControl*>(GetNamedChild(mControlNames.sampleRate))->SetStr("");
    mHasInfo = false;
  };

  void Hide(bool hide) override
  {
    // Don't show me unless I have info to show!
    IContainerBase::Hide(hide || (!mHasInfo));
  };

  void OnAttached() override
  {
    AddChildControl(new IVLabelControl(GetRECT().SubRectVertical(4, 0), "Model information:", mStyle));
    AddNamedChildControl(new IVLabelControl(GetRECT().SubRectVertical(4, 1), "", mStyle), mControlNames.sampleRate);
    // AddNamedChildControl(
    //   new IVLabelControl(GetRECT().SubRectVertical(4, 2), "", mStyle), mControlNames.inputCalibrationLevel);
    // AddNamedChildControl(
    //   new IVLabelControl(GetRECT().SubRectVertical(4, 3), "", mStyle), mControlNames.outputCalibrationLevel);
  };

  void SetModelInfo(const ModelInfo& modelInfo)
  {
    auto SetControlStr = [&](const std::string& name, const PossiblyKnownParameter& p, const std::string& units,
                             const std::string& childName) {
      std::stringstream ss;
      ss << name << ": ";
      if (p.known)
      {
        ss << p.value << " " << units;
      }
      else
      {
        ss << "(Unknown)";
      }
      static_cast<IVLabelControl*>(GetNamedChild(childName))->SetStr(ss.str().c_str());
    };

    SetControlStr("Sample rate", modelInfo.sampleRate, "Hz", mControlNames.sampleRate);
    // SetControlStr(
    //   "Input calibration level", modelInfo.inputCalibrationLevel, "dBu", mControlNames.inputCalibrationLevel);
    // SetControlStr(
    //   "Output calibration level", modelInfo.outputCalibrationLevel, "dBu", mControlNames.outputCalibrationLevel);

    mHasInfo = true;
  };

private:
  const IVStyle mStyle;
  struct
  {
    const std::string sampleRate = "sampleRate";
    // const std::string inputCalibrationLevel = "inputCalibrationLevel";
    // const std::string outputCalibrationLevel = "outputCalibrationLevel";
  } mControlNames;
  // Do I have info?
  bool mHasInfo = false;
};

class OutputModeControl : public IVRadioButtonControl
{
public:
  OutputModeControl(const IRECT& bounds, int paramIdx, const IVStyle& style, float buttonSize)
  : IVRadioButtonControl(
      bounds, paramIdx, {"Raw", "Normalized"}, "Output Mode", style, EVShape::Ellipse, EDirection::Vertical, buttonSize) {};

  void SetNormalizedDisable(const bool disable)
  {
    // HACK non-DRY string and hard-coded indices
    std::stringstream ss;
    ss << "Normalized";
    if (disable)
    {
      ss << " [Not supported by model]";
    }
    mTabLabels.Get(1)->Set(ss.str().c_str());
  };
};

class OversamplingControl : public IVRadioButtonControl
{
public:
  OversamplingControl(const IRECT& bounds, int paramIdx, const IVStyle& style, float buttonSize)
  : IVRadioButtonControl(
      bounds, paramIdx, {}, "Oversampling", style, EVShape::Rectangle, EDirection::Horizontal, buttonSize) {};
};

class IRModeControl : public IVRadioButtonControl
{
public:
  IRModeControl(const IRECT& bounds, int paramIdx, const IVStyle& style, float buttonSize)
  : IVRadioButtonControl(
      bounds, paramIdx, {}, "IR Mode", style, EVShape::Rectangle, EDirection::Horizontal, buttonSize) {};
};

class BlendNearestControl : public IVRadioButtonControl
{
public:
  BlendNearestControl(const IRECT& bounds, int paramIdx, const IVStyle& style, float buttonSize)
  : IVRadioButtonControl(
      bounds, paramIdx, {}, "CPU Usage", style, EVShape::Rectangle, EDirection::Horizontal, buttonSize) {};
};

class NAMSettingsPageControl : public IContainerBaseWithNamedChildren
{
public:
  NAMSettingsPageControl(const IRECT& bounds, const IBitmap& inputLevelBackgroundBitmap,
                         const IBitmap& switchBitmap, ISVG closeSVG, const IVStyle& style,
                         const IVStyle& radioButtonStyle)
  : IContainerBaseWithNamedChildren(bounds)
  , mAnimationTime(0)
  , mInputLevelBackgroundBitmap(inputLevelBackgroundBitmap)
  , mSwitchBitmap(switchBitmap)
  , mStyle(style)
  , mRadioButtonStyle(radioButtonStyle)
  , mCloseSVG(closeSVG)
  {
    mIgnoreMouse = false;
  }

  void Draw(IGraphics& g) override
  {
    g.FillRect(PluginColors::NAM_BG_BLUE, mRECT);
    IContainerBase::Draw(g);
  }

  bool OnKeyDown(float x, float y, const IKeyPress& key) override
  {
    if (key.VK == kVK_ESCAPE)
    {
      HideAnimated(true);
      return true;
    }
    return false;
  }

  void HideAnimated(bool hide)
  {
    mWillHide = hide;

    if (hide == false)
    {
      mHide = false;
    }
    else
    {
      ForAllChildrenFunc([hide](int childIdx, IControl* pChild) { pChild->Hide(hide); });
    }

    SetAnimation(
      [&](IControl* pCaller) {
        auto progress = static_cast<float>(pCaller->GetAnimationProgress());

        if (mWillHide)
          SetBlend(IBlend(EBlend::Default, 1.0f - progress));
        else
          SetBlend(IBlend(EBlend::Default, progress));

        if (progress > 1.0f)
        {
          pCaller->OnEndAnimation();
          IContainerBase::Hide(mWillHide);
          GetUI()->SetAllControlsDirty();
          return;
        }
      },
      mAnimationTime);

    SetDirty(true);
  }

  void OnAttached() override
  {
    const float pad = 20.0f;
    const IVStyle titleStyle = DEFAULT_STYLE.WithValueText(IText(30, COLOR_WHITE, "Michroma-Regular"))
                                 .WithDrawFrame(false)
                                 .WithShadowOffset(2.f);
    const auto text = IText(DEFAULT_TEXT_SIZE, EAlign::Center, PluginColors::HELP_TEXT);
    const auto style = mStyle.WithDrawFrame(false).WithValueText(text);

    const auto titleArea = GetRECT().GetPadded(-(pad + 10.0f)).GetFromTop(50.0f);
    AddNamedChildControl(new IVLabelControl(titleArea, "SETTINGS", titleStyle), mControlNames.title);

    {
      const float height = NAM_KNOB_HEIGHT + NAM_SWTICH_HEIGHT + 10.0f + 85.0f;
      const float width = titleArea.W();
      const auto centerArea = titleArea.GetFromBottom(height).GetTranslated(0.0f, height);
      const auto leftArea = centerArea.GetFromLeft(0.5f * width);
      const auto rightArea = centerArea.GetFromRight(0.5f * width);

      const float switchH = 25.0f;
      const float labelH = 18.0f;
      const float hPad = 50.0f;
      const float irModeRowH = labelH + switchH;

      // IR Mode (left column)
      const auto irModeLabelArea = leftArea.GetFromTop(labelH);
      AddNamedChildControl(new IVLabelControl(irModeLabelArea, "IR Mode", style), mControlNames.irModeLabel);
      const auto irModeSwitchArea = leftArea.GetFromTop(irModeRowH).GetFromBottom(switchH).GetPadded(hPad, 0.0f, hPad, 0.0f);
      auto* irModeSwitch = AddNamedChildControl(
        new NAMSwitchControl(irModeSwitchArea, kIRMode, "", mStyle, mSwitchBitmap), mControlNames.irMode);
      irModeSwitch->SetTooltip(
        "IR convolution mode. Zero Latency uses direct convolution with no added latency. Normal uses FFT convolution.");

      // About section (right column)
      const auto aboutArea = rightArea.GetPadded(-20.0f, 0.0f, -20.0f, 0.0f);
      AddNamedChildControl(new AboutControl(aboutArea, style, text), mControlNames.about);
    }

    auto closeAction = [&](IControl* pCaller) {
      static_cast<NAMSettingsPageControl*>(pCaller->GetParent())->HideAnimated(true);
    };
    AddNamedChildControl(
      new NAMSquareButtonControl(CornerButtonArea(GetRECT()), closeAction, mCloseSVG), mControlNames.close);

    OnResize();
  }

private:
  IBitmap mInputLevelBackgroundBitmap;
  IBitmap mSwitchBitmap;
  IVStyle mStyle;
  IVStyle mRadioButtonStyle;
  ISVG mCloseSVG;
  int mAnimationTime = 200;
  bool mWillHide = false;

  struct ControlNames
  {
    const std::string close = "Close";
    const std::string irMode = "IRMode";
    const std::string irModeLabel = "IRModeLabel";
    const std::string title = "Title";
    const std::string about = "About";
  } mControlNames;

  class AboutControl : public IContainerBase
  {
  public:
    AboutControl(const IRECT& bounds, const IVStyle& style, const IText& text)
    : IContainerBase(bounds)
    , mStyle(style)
    , mText(text) {};

    void OnAttached() override
    {
      WDL_String verStr, buildInfoStr;
      if (auto* pPlug = dynamic_cast<iplug::IPluginBase*>(GetDelegate()))
      {
        pPlug->GetPluginVersionStr(verStr);
        buildInfoStr.SetFormatted(100, "Version %s %s %s", verStr.Get(), pPlug->GetArchStr(), pPlug->GetAPIStr());
      }
      else
      {
        verStr.Set("Unknown");
        buildInfoStr.SetFormatted(100, "Version %s %s %s", verStr.Get(), "unknown", "unknown");
      }

      AddChildControl(new IVLabelControl(GetRECT().SubRectVertical(5, 0), "IMPULSE", mStyle));
      AddChildControl(new IVLabelControl(GetRECT().SubRectVertical(5, 1), "By Tonkraf", mStyle));
      AddChildControl(new IVLabelControl(GetRECT().SubRectVertical(5, 2), buildInfoStr.Get(), mStyle));
      AddChildControl(new IURLControl(GetRECT().SubRectVertical(5, 3),
                                      "Plug-in development: Steve Atkinson, Oli Larkin, ... ",
                                      "https://github.com/sdatkinson/NeuralAmpModelerPlugin/graphs/contributors", mText,
                                      COLOR_TRANSPARENT, PluginColors::HELP_TEXT_MO, PluginColors::HELP_TEXT_CLICKED));
      AddChildControl(new IURLControl(GetRECT().SubRectVertical(5, 4), "www.tonkraf.com",
                                      "https://www.tonkraf.com", mText, COLOR_TRANSPARENT,
                                      PluginColors::HELP_TEXT_MO, PluginColors::HELP_TEXT_CLICKED));
    };

  private:
    IVStyle mStyle;
    IText mText;
  };
};
