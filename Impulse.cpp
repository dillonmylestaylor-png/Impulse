#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <utility>

#include <dlfcn.h>

#include "Colors.h"
#include "IPlugPaths.h"
// clang-format off
#include "Impulse.h"
#include "IPlug_include_in_plug_src.h"
// clang-format on
#include "architecture.hpp"

#ifndef NO_IGRAPHICS
#include "ImpulseControls.h"
#endif

using namespace iplug;
using namespace igraphics;

const double kDCBlockerFrequency = 5.0;

// Styles
const IVColorSpec colorSpec{
  DEFAULT_BGCOLOR, // Background
  PluginColors::NAM_THEMECOLOR, // Foreground
  PluginColors::NAM_THEMECOLOR.WithOpacity(0.3f), // Pressed
  PluginColors::NAM_THEMECOLOR.WithOpacity(0.4f), // Frame
  PluginColors::MOUSEOVER, // Highlight
  DEFAULT_SHCOLOR, // Shadow
  PluginColors::NAM_THEMECOLOR, // Extra 1
  COLOR_RED, // Extra 2 --> color for clipping in meters
  PluginColors::NAM_THEMECOLOR.WithContrast(0.1f), // Extra 3
};

const IVStyle style =
  IVStyle{true, // Show label
          true, // Show value
          colorSpec,
          {DEFAULT_TEXT_SIZE + 3.f, EVAlign::Middle, PluginColors::NAM_THEMEFONTCOLOR}, // Knob label text
          {DEFAULT_TEXT_SIZE + 3.f, EVAlign::Bottom, PluginColors::NAM_THEMEFONTCOLOR}, // Knob value text
          DEFAULT_HIDE_CURSOR,
          DEFAULT_DRAW_FRAME,
          false,
          DEFAULT_EMBOSS,
          0.2f,
          2.f,
          DEFAULT_SHADOW_OFFSET,
          DEFAULT_WIDGET_FRAC,
          DEFAULT_WIDGET_ANGLE};
const IVStyle knobStyle =
  style.WithValueText(IText(DEFAULT_TEXT_SIZE + 3.f, COLOR_BLACK, nullptr, EAlign::Center, EVAlign::Bottom))
       .WithLabelText(IText(DEFAULT_TEXT_SIZE + 3.f, COLOR_BLACK, nullptr, EAlign::Center, EVAlign::Middle));
const IVStyle titleStyle =
  DEFAULT_STYLE.WithValueText(IText(30, COLOR_WHITE, "Michroma-Regular")).WithDrawFrame(false).WithShadowOffset(2.f);
const IVStyle filterRadioStyle =
  style
    .WithLabelText(IText(9, PluginColors::NAM_THEMEFONTCOLOR, "Roboto-Regular"))
    .WithValueText(IText(9, PluginColors::NAM_THEMEFONTCOLOR, "Roboto-Regular"))
    .WithColor(EVColor::kON, PluginColors::NAM_THEMECOLOR)
    .WithColor(EVColor::kOFF, PluginColors::NAM_THEMECOLOR.WithOpacity(0.1f))
    .WithColor(EVColor::kX1, PluginColors::NAM_THEMECOLOR.WithOpacity(0.6f));
const IVStyle radioButtonStyle =
  style
    .WithColor(EVColor::kON, PluginColors::NAM_THEMECOLOR)
    .WithColor(EVColor::kOFF, PluginColors::NAM_THEMECOLOR.WithOpacity(0.1f))
    .WithColor(EVColor::kX1, PluginColors::NAM_THEMECOLOR.WithOpacity(0.6f));

#ifndef NO_IGRAPHICS
EMsgBoxResult _ShowMessageBox(iplug::igraphics::IGraphics* pGraphics, const char* str, const char* caption,
                              EMsgBoxType type)
{
#ifdef OS_MAC
  return pGraphics->ShowMessageBox(caption, str, type);
#else
  return pGraphics->ShowMessageBox(str, caption, type);
#endif
}
#endif

static int _ImpulseBinaryMarker = 0;

Impulse::Impulse(const InstanceInfo& info)
: Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
  // Input / output
  GetParam(kInputLevel)->InitGain("Input", 0.0, -40.0, 40.0, 0.1);
  GetParam(kOutputLevel)->InitGain("Output", 0.0, -40.0, 40.0, 0.1);
  GetParam(kInputTrim)->InitDouble("Trim", 0.0, -40.0, 40.0, 0.1, "dB");
  GetParam(kIRMode)->InitEnum("Mode", 0, {"Zero Latency", "Normal"});

  // IR 0
  GetParam(kIRToggle)->InitBool("IR1", true);
  GetParam(kIRPolarity)->InitBool("IR1 Phase", false);
  GetParam(kIRLevel)->InitDouble("IR1 Level", 1.0, 0.0, 1.0, 0.01, "");
  GetParam(kIRMute)->InitBool("IR1 Mute", false);
  GetParam(kIRDelay)->InitInt("IR1 Delay", 0, 0, 99, "%i samples");
  GetParam(kIRPan)->InitDouble("IR1 Pan", 0.0, -1.0, 1.0, 0.01, "");

  // IR 1
  GetParam(kIRToggle2)->InitBool("IR2", true);
  GetParam(kIRPolarity2)->InitBool("IR2 Phase", false);
  GetParam(kIRLevel2)->InitDouble("IR2 Level", 1.0, 0.0, 1.0, 0.01, "");
  GetParam(kIRMute2)->InitBool("IR2 Mute", false);
  GetParam(kIRDelay2)->InitInt("IR2 Delay", 0, 0, 99, "%i samples");
  GetParam(kIRPan2)->InitDouble("IR2 Pan", 0.0, -1.0, 1.0, 0.01, "");

  // IR 2
  GetParam(kIRToggle3)->InitBool("IR3", true);
  GetParam(kIRPolarity3)->InitBool("IR3 Phase", false);
  GetParam(kIRLevel3)->InitDouble("IR3 Level", 1.0, 0.0, 1.0, 0.01, "");
  GetParam(kIRMute3)->InitBool("IR3 Mute", false);
  GetParam(kIRDelay3)->InitInt("IR3 Delay", 0, 0, 99, "%i samples");
  GetParam(kIRPan3)->InitDouble("IR3 Pan", 0.0, -1.0, 1.0, 0.01, "");

  // IR 3
  GetParam(kIRToggle4)->InitBool("IR4", true);
  GetParam(kIRPolarity4)->InitBool("IR4 Phase", false);
  GetParam(kIRLevel4)->InitDouble("IR4 Level", 1.0, 0.0, 1.0, 0.01, "");
  GetParam(kIRMute4)->InitBool("IR4 Mute", false);
  GetParam(kIRDelay4)->InitInt("IR4 Delay", 0, 0, 99, "%i samples");
  GetParam(kIRPan4)->InitDouble("IR4 Pan", 0.0, -1.0, 1.0, 0.01, "");

  // IR 0 HPF/LPF
  GetParam(kIRHPFreq)->InitDouble("IR1 HPF Freq", 5.0, 5.0, 1000.0, 1.0, "Hz");
  GetParam(kIRHPFSlope)->InitEnum("IR1 HPF Slope", 0, {"6", "12", "18"});
  GetParam(kIRHPFBypass)->InitBool("IR1 HPF Bypass", true);
  GetParam(kIRLPFreq)->InitDouble("IR1 LPF Freq", 20000.0, 1000.0, 20000.0, 1.0, "Hz");
  GetParam(kIRLPFSlope)->InitEnum("IR1 LPF Slope", 0, {"6", "12", "18"});
  GetParam(kIRLPFBypass)->InitBool("IR1 LPF Bypass", true);
  // IR 1 HPF/LPF
  GetParam(kIRHPFreq2)->InitDouble("IR2 HPF Freq", 5.0, 5.0, 1000.0, 1.0, "Hz");
  GetParam(kIRHPFSlope2)->InitEnum("IR2 HPF Slope", 0, {"6", "12", "18"});
  GetParam(kIRHPFBypass2)->InitBool("IR2 HPF Bypass", true);
  GetParam(kIRLPFreq2)->InitDouble("IR2 LPF Freq", 20000.0, 1000.0, 20000.0, 1.0, "Hz");
  GetParam(kIRLPFSlope2)->InitEnum("IR2 LPF Slope", 0, {"6", "12", "18"});
  GetParam(kIRLPFBypass2)->InitBool("IR2 LPF Bypass", true);
  // IR 2 HPF/LPF
  GetParam(kIRHPFreq3)->InitDouble("IR3 HPF Freq", 5.0, 5.0, 1000.0, 1.0, "Hz");
  GetParam(kIRHPFSlope3)->InitEnum("IR3 HPF Slope", 0, {"6", "12", "18"});
  GetParam(kIRHPFBypass3)->InitBool("IR3 HPF Bypass", true);
  GetParam(kIRLPFreq3)->InitDouble("IR3 LPF Freq", 20000.0, 1000.0, 20000.0, 1.0, "Hz");
  GetParam(kIRLPFSlope3)->InitEnum("IR3 LPF Slope", 0, {"6", "12", "18"});
  GetParam(kIRLPFBypass3)->InitBool("IR3 LPF Bypass", true);
  // IR 3 HPF/LPF
  GetParam(kIRHPFreq4)->InitDouble("IR4 HPF Freq", 5.0, 5.0, 1000.0, 1.0, "Hz");
  GetParam(kIRHPFSlope4)->InitEnum("IR4 HPF Slope", 0, {"6", "12", "18"});
  GetParam(kIRHPFBypass4)->InitBool("IR4 HPF Bypass", true);
  GetParam(kIRLPFreq4)->InitDouble("IR4 LPF Freq", 20000.0, 1000.0, 20000.0, 1.0, "Hz");
  GetParam(kIRLPFSlope4)->InitEnum("IR4 LPF Slope", 0, {"6", "12", "18"});
  GetParam(kIRLPFBypass4)->InitBool("IR4 LPF Bypass", true);

#ifndef NO_IGRAPHICS
  mMakeGraphicsFunc = [&]() {
#ifdef OS_IOS
    auto scaleFactor = GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT) * 0.85f;
#else
    auto scaleFactor = 1.0f;
#endif
    return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS, scaleFactor);
  };

  mLayoutFunc = [&](IGraphics* pGraphics) {
    pGraphics->AttachCornerResizer(EUIResizerMode::Scale, false);
    pGraphics->AttachTextEntryControl();
    pGraphics->EnableMouseOver(true);
    pGraphics->EnableTooltips(true);
    pGraphics->EnableMultiTouch(true);

    pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);
    pGraphics->LoadFont("Michroma-Regular", MICHROMA_FN);

    const auto gearSVG = pGraphics->LoadSVG(GEAR_FN);
    const auto fileSVG = pGraphics->LoadSVG(FILE_FN);
    const auto globeSVG = pGraphics->LoadSVG(GLOBE_ICON_FN);
    const auto crossSVG = pGraphics->LoadSVG(CLOSE_BUTTON_FN);
    const auto rightArrowSVG = pGraphics->LoadSVG(RIGHT_ARROW_FN);
    const auto leftArrowSVG = pGraphics->LoadSVG(LEFT_ARROW_FN);
    const auto irIconOnSVG = pGraphics->LoadSVG(IR_ICON_ON_FN);
    const auto irIconOffSVG = pGraphics->LoadSVG(IR_ICON_OFF_FN);

    const auto controlsBackgroundBitmap = pGraphics->LoadBitmap(CONTROLSBACKGROUND_FN);
    const auto fileBackgroundBitmap = pGraphics->LoadBitmap(FILEBACKGROUND_FN);
    const auto inputLevelBackgroundBitmap = pGraphics->LoadBitmap(INPUTLEVELBACKGROUND_FN);
    const auto knobBackgroundBitmap = pGraphics->LoadBitmap(KNOBBACKGROUND_FN);
    const auto switchHandleBitmap = pGraphics->LoadBitmap(SLIDESWITCHHANDLE_FN);
    const auto meterBackgroundBitmap = pGraphics->LoadBitmap(METERBACKGROUND_FN);
    const auto logoBitmap = pGraphics->LoadBitmap(LOGO_FN);

    const auto b = pGraphics->GetBounds();
    const auto mainArea = b.GetPadded(-20);
    const auto contentArea = mainArea.GetPadded(-10);
    const auto topBarHeight = 96.0f;
    const float topBarKnobH = 64.0f;

    // ===== TOP BAR: IN + meter | Trim | Logo | IR Mode | OUT =====
    const auto topBar = contentArea.GetFromTop(topBarHeight);
    const float meterW = 14.0f;
    const float meterH = 32.0f;
    const float topBarPad = 25.0f;

    // IN section: meter | label | trim knob (mirrors OUT)
    const float inSectionW = 130.0f;
    const float inLabelW = 32.0f;
    const auto inSection = IRECT(topBar.L + topBarPad, topBar.T,
                                 topBar.L + topBarPad + inSectionW, topBar.B);
    const auto inMeterArea = inSection.GetFromLeft(meterW).GetCentredInside(meterW, meterH);
    const auto inLabelArea = IRECT(inMeterArea.R + 4, topBar.T,
                                   inMeterArea.R + 4 + inLabelW, topBar.B)
                               .GetCentredInside(inLabelW, 22);

    // Input trim knob
    const float inTrimW = 64.0f;
    const float labelH = 12.0f;
    const auto inTrimArea = IRECT(inLabelArea.R + 8, topBar.T + 2,
                                  inSection.R, topBar.B - 2);
    const auto inTrimLabelArea = inTrimArea.GetFromTop(labelH).GetCentredInside(inTrimW, labelH);
    const auto inTrimKnobArea = IRECT(inTrimArea.L, inTrimLabelArea.B + 2,
                                      inTrimArea.R, inTrimLabelArea.B + 2 + topBarKnobH);
    const auto inTrimValueArea = IRECT(inTrimArea.L, inTrimKnobArea.B + 2,
                                       inTrimArea.R, inTrimArea.B);

    // OUT section
    const float outSectionW = 168.0f;
    const float outLabelW = 40.0f;
    const auto outSection = IRECT(topBar.R - outSectionW - topBarPad, topBar.T,
                                  topBar.R - topBarPad, topBar.B);
    const auto outMeterArea = outSection.GetFromRight(meterW).GetCentredInside(meterW, meterH);
    const auto outLabelArea = IRECT(outMeterArea.L - 4 - outLabelW, topBar.T,
                                    outMeterArea.L - 4, topBar.B)
                                .GetCentredInside(outLabelW, 22);
    const auto outKnobContainer = IRECT(outSection.L, topBar.T + 2,
                                        outLabelArea.L - 8, topBar.B - 2);
    const auto outKnobLabelArea = outKnobContainer.GetFromTop(labelH).GetCentredInside(outKnobContainer.W(), labelH);
    const auto outKnobSubArea = IRECT(outKnobContainer.L, outKnobLabelArea.B + 2,
                                      outKnobContainer.R, outKnobLabelArea.B + 2 + topBarKnobH);
    const auto outKnobValueArea = IRECT(outKnobContainer.L, outKnobSubArea.B + 2,
                                        outKnobContainer.R, outKnobContainer.B);

    // Center logo area
    const auto logoArea = topBar.GetCentredInside(140, 55);

    // IR Mode buttons (between logo and OUT)
    const auto irModeArea = IRECT(logoArea.R + 8, topBar.T + 4, outSection.L - 8, topBar.B - 4);

    // ===== MAIN CONTENT AREA: IR slots =====
    const auto mainContentArea = contentArea.GetReducedFromTop(topBarHeight);
    const auto irSection = mainContentArea;

    // IR section layout: 4 rows stacked vertically
    const auto irRowHeight = irSection.H() / 4.0f;
    const auto btnSz = 30.0f;
    const auto irLabelH = 11.0f;
    const auto irLabelGap = 6.0f;
    const auto fileHeight = 30.0f;

    const char* const defaultIRString = "Select IR...";
    const char* const getUrl = "https://www.tonkraf.com";

    // Draw background
    pGraphics->AttachControl(new NAMBackgroundControl(b, PluginColors::NAM_BG_BLUE));
    const auto impulseLabelArea = logoArea.GetFromTop(18.0f).GetVShifted(-logoArea.H() * 0.5f);
    pGraphics->AttachControl(new ITextControl(impulseLabelArea, "IMPULSE",
      IText(16, PluginColors::NAM_THEMEFONTCOLOR, "Michroma-Regular")));
    pGraphics->AttachControl(new NAMAmpImageControl(logoArea, logoBitmap));

    // Top bar elements
    pGraphics->AttachControl(new ITextControl(inLabelArea, "IN", IText(16, PluginColors::NAM_THEMEFONTCOLOR, "Michroma-Regular")));
    pGraphics->AttachControl(new NAMMeterControl(inMeterArea, meterBackgroundBitmap, style), kCtrlTagInputMeter);
    pGraphics->AttachControl(new ITextControl(inTrimLabelArea, "Input", IText(11, PluginColors::NAM_THEMEFONTCOLOR, "Michroma-Regular")));
    {
      auto* trimKnob = new NAMKnobControl(inTrimKnobArea, kInputTrim, "", style, knobBackgroundBitmap);
      trimKnob->HideLabel();
      trimKnob->HideValue();
      pGraphics->AttachControl(trimKnob);
    }
    pGraphics->AttachControl(new ICaptionControl(inTrimValueArea, kInputTrim,
      IText(11, PluginColors::NAM_THEMEFONTCOLOR, "Michroma-Regular"), COLOR_TRANSPARENT, true));
    {
      auto* irModeCtrl = new IVRadioButtonControl(irModeArea, kIRMode, {}, "Mode", radioButtonStyle, EVShape::Rectangle, EDirection::Vertical, 20.f);
      irModeCtrl->SetTooltip("IR convolution mode: Zero Latency vs Normal (FFT)");
      pGraphics->AttachControl(irModeCtrl);
    }
    pGraphics->AttachControl(new ITextControl(outKnobLabelArea, "Output", IText(11, PluginColors::NAM_THEMEFONTCOLOR, "Michroma-Regular")));
    {
      auto* outKnob = new NAMKnobControl(outKnobSubArea, kOutputLevel, "", style, knobBackgroundBitmap);
      outKnob->HideLabel();
      outKnob->HideValue();
      pGraphics->AttachControl(outKnob);
    }
    pGraphics->AttachControl(new ICaptionControl(outKnobValueArea, kOutputLevel,
      IText(11, PluginColors::NAM_THEMEFONTCOLOR, "Michroma-Regular"), COLOR_TRANSPARENT, true));
    pGraphics->AttachControl(new ITextControl(outLabelArea, "OUT", IText(16, PluginColors::NAM_THEMEFONTCOLOR, "Michroma-Regular")));
    pGraphics->AttachControl(new NAMMeterControl(outMeterArea, meterBackgroundBitmap, style), kCtrlTagOutputMeter);

    // Build 4 IR rows
    for (int i = 0; i < kNumIRs; i++)
    {
      const auto row = IRECT(irSection.L, irSection.T + i * irRowHeight, irSection.R, irSection.T + (i + 1) * irRowHeight).GetPadded(-6, -4, -6, -4);
      const auto fileRow = IRECT(row.L, row.B - fileHeight, row.R, row.B).GetMidHPadded(row.W() * 0.35f);
      const auto switchArea = fileRow.GetFromLeft(22.0f).GetScaledAboutCentre(0.55f);
      const auto fileArea = fileRow.GetPadded(-14.0f, 0.0f, -4.0f, 0.0f);

      const auto controlsRow = IRECT(row.L, row.T, row.R, fileRow.T - 4);
      const auto levelArea = controlsRow.GetFromLeft(100.0f).GetCentredInside(100.0f, controlsRow.H());
      const auto ctrlGap = 2.0f;
      const auto ctrlPackW = fileRow.W();
      const auto ctrlPackL = fileRow.L;
      const auto ctrlW = ctrlPackW / 4.0f;
      const auto phaseArea = IRECT(ctrlPackL, controlsRow.T, ctrlPackL + ctrlW, controlsRow.B).GetCentredInside(btnSz, btnSz);
      const auto muteArea = IRECT(ctrlPackL + ctrlW, controlsRow.T, ctrlPackL + ctrlW * 2.0f, controlsRow.B).GetCentredInside(btnSz, btnSz);
      const auto delayAreaL = ctrlPackL + ctrlW * 2.0f;
      const auto delayCircleArea = IRECT(delayAreaL, controlsRow.T, delayAreaL + ctrlW, controlsRow.B).GetCentredInside(btnSz, btnSz);
      const auto delayLabelArea = IRECT(delayAreaL, delayCircleArea.B + irLabelGap, delayAreaL + ctrlW, delayCircleArea.B + irLabelGap + irLabelH);
      const auto panAreaL = ctrlPackL + ctrlW * 3.0f;
      const auto panArea = IRECT(panAreaL, controlsRow.T, panAreaL + ctrlW, controlsRow.B).GetCentredInside(btnSz, btnSz);
      const auto panLabelArea = IRECT(panAreaL, panArea.B + irLabelGap, panAreaL + ctrlW, panArea.B + irLabelGap + irLabelH);

      const int toggleParam = kIRToggle + i * 6;
      const int phaseParam = kIRPolarity + i * 6;
      const int levelParam = kIRLevel + i * 6;
      const int muteParam = kIRMute + i * 6;
      const int delayParam = kIRDelay + i * 6;
      const int panParam = kIRPan + i * 6;
      const int browserTag = kCtrlTagIRFileBrowser + i;
      const int clearMsg = kMsgTagClearIR + i;

      // Toggle
      pGraphics->AttachControl(new ISVGSwitchControl(switchArea, {irIconOffSVG, irIconOnSVG}, toggleParam));

      // File browser
      auto loadHandler = [&, i](const WDL_String& fileName, const WDL_String& path) {
        if (fileName.GetLength())
        {
          mIRSlots[i].path = fileName;
          const dsp::wav::LoadReturnCode retCode = _StageIR(fileName, i);
          if (retCode != dsp::wav::LoadReturnCode::SUCCESS)
          {
            std::stringstream message;
            message << "Failed to load IR file " << fileName.Get() << ":\n";
            message << dsp::wav::GetMsgForLoadReturnCode(retCode);
            _ShowMessageBox(GetUI(), message.str().c_str(), "Failed to load IR!", kMB_OK);
          }
        }
      };
      pGraphics->AttachControl(
        new NAMFileBrowserControl(fileArea, clearMsg, defaultIRString, "wav", loadHandler, style,
                                  fileSVG, crossSVG, leftArrowSVG, rightArrowSVG, fileBackgroundBitmap, globeSVG,
                                  "More Info", getUrl),
        browserTag);

      // Level knob
      char lvlLabel[8];
      snprintf(lvlLabel, sizeof(lvlLabel), "IR %d", i + 1);
      pGraphics->AttachControl(new NAMKnobControl(levelArea, levelParam, lvlLabel, style, knobBackgroundBitmap));

      // Phase
      pGraphics->AttachControl(new NAMPhaseFlipControl(phaseArea, phaseParam));

      // Mute
      pGraphics->AttachControl(new NAMMuteControl(muteArea, muteParam));

      // Delay
      {
        auto* pDelay = new NAMDelayControl(delayCircleArea, delayParam);
        pDelay->HideLabel();
        pGraphics->AttachControl(pDelay);
      }
      pGraphics->AttachControl(new NAMLabelControl(delayLabelArea, "Delay", IText(11, PluginColors::NAM_THEMEFONTCOLOR, "Roboto-Regular"), delayParam));

      // Pan
      {
        auto* panCtrl = new NAMPanCircleControl(panArea, panParam);
        panCtrl->HideLabel();
        pGraphics->AttachControl(panCtrl);
      }
      pGraphics->AttachControl(new NAMLabelControl(panLabelArea, "Pan", IText(11, PluginColors::NAM_THEMEFONTCOLOR, "Roboto-Regular"), panParam), kCtrlTagPanLabel + i);

      // Per-IR HPF/LPF filter controls (right side panel)
      const float filterCircW = 28.0f;
      const float filterGap = 4.0f;
      const auto filterPanel = IRECT(fileRow.R + 8, row.T, row.R, row.B);
      const float filterHalfH = filterPanel.H() / 2.0f;
      const auto hpfPanel = IRECT(filterPanel.L, filterPanel.T, filterPanel.R, filterPanel.T + filterHalfH);
      const auto lpfPanel = IRECT(filterPanel.L, hpfPanel.B, filterPanel.R, filterPanel.B);

      const int hpfFreqP = kIRHPFreq + i * 6;
      const int hpfSlopeP = kIRHPFSlope + i * 6;
      const int hpfBypP = kIRHPFBypass + i * 6;
      const int lpfFreqP = kIRLPFreq + i * 6;
      const int lpfSlopeP = kIRLPFSlope + i * 6;
      const int lpfBypP = kIRLPFBypass + i * 6;

      // HPF: [freq circle] [slope radio with clickable "HPF" title]
      {
        const auto hpfFreqArea = hpfPanel.GetFromLeft(filterCircW).GetCentredInside(filterCircW, filterCircW);
        pGraphics->AttachControl(new NAMFreqCircleControl(hpfFreqArea, hpfFreqP));
        const auto hpfSlopeArea = IRECT(hpfFreqArea.R + filterGap, hpfPanel.T, hpfPanel.R, hpfPanel.B);
        pGraphics->AttachControl(new NAMFilterRadioButton(hpfSlopeArea, hpfSlopeP, {}, "HPF", filterRadioStyle, EVShape::Rectangle, EDirection::Horizontal, 10.f, hpfBypP));
      }

      // LPF: [freq circle] [slope radio with clickable "LPF" title]
      {
        const auto lpfFreqArea = lpfPanel.GetFromLeft(filterCircW).GetCentredInside(filterCircW, filterCircW);
        pGraphics->AttachControl(new NAMFreqCircleControl(lpfFreqArea, lpfFreqP));
        const auto lpfSlopeArea = IRECT(lpfFreqArea.R + filterGap, lpfPanel.T, lpfPanel.R, lpfPanel.B);
        pGraphics->AttachControl(new NAMFilterRadioButton(lpfSlopeArea, lpfSlopeP, {}, "LPF", filterRadioStyle, EVShape::Rectangle, EDirection::Horizontal, 10.f, lpfBypP));
      }
    }

    // Settings/help/about box
    const auto settingsButtonArea = CornerButtonArea(b);
    pGraphics->AttachControl(new NAMCircleButtonControl(
      settingsButtonArea,
      [pGraphics](IControl* pCaller) {
        pGraphics->GetControlWithTag(kCtrlTagSettingsBox)->As<NAMSettingsPageControl>()->HideAnimated(false);
      },
      gearSVG));

    pGraphics
      ->AttachControl(new NAMSettingsPageControl(b, inputLevelBackgroundBitmap, switchHandleBitmap, crossSVG, style,
                                                  style),
                      kCtrlTagSettingsBox)
      ->Hide(true);

    pGraphics->ForAllControlsFunc([](IControl* pControl) {
      pControl->SetMouseEventsWhenDisabled(true);
      pControl->SetMouseOverWhenDisabled(true);
    });
    // Prevent pan knobs from moving when disabled (mono output)
    pGraphics->ForAllControlsFunc([](IControl* pControl) {
      int idx = pControl->GetParamIdx();
      if (idx == kIRPan || idx == kIRPan2 || idx == kIRPan3 || idx == kIRPan4)
        pControl->SetMouseEventsWhenDisabled(false);
    });

    // Force all controls to sync displayed value from param defaults
    pGraphics->ForAllControlsFunc([](IControl* c) {
      if (int idx = c->GetParamIdx(); idx >= 0 && idx < kNumParams)
        if (auto* p = c->GetParam())
          c->SetValue(p->GetDefault(true));
    });
    pGraphics->SetAllControlsDirty();
  };
#endif
}

Impulse::~Impulse()
{
  _DeallocateIOPointers();
}

#if !defined(WEB_API) && !defined(WASM_UI_API)
void Impulse::ProcessBlock(iplug::sample** inputs, iplug::sample** outputs, int nFrames)
{
  const size_t numChannelsExternalIn = (size_t)NInChansConnected();
  const size_t numChannelsExternalOut = (size_t)NOutChansConnected();
  mStereoOutput.store(numChannelsExternalOut > 1);
  const size_t numChannelsInternal = kNumChannelsInternal;
  const size_t numFrames = (size_t)nFrames;
  const double sampleRate = GetSampleRate();

  // Disable floating point denormals
  std::fenv_t fe_state;
  std::feholdexcept(&fe_state);
  disable_denormals();

  _PrepareBuffers(numChannelsInternal, numFrames);
  _ProcessInput(inputs, numFrames, numChannelsExternalIn, numChannelsInternal);
  _ApplyDSPStaging();

  // Set IR convolution mode
  for (auto& slot : mIRSlots)
    if (slot.ir) slot.ir->SetMode(mIRMode);

  // Process all 4 IRs and mix them
  constexpr double kIRMakeupGain = 1.012;
  const bool useStereo = numChannelsExternalOut > 1;

  // Count active IRs for blend scaling
  int activeCount = 0;
  int toggledOnCount = 0;
  for (int i = 0; i < kNumIRs; i++)
  {
    const int toggleParam = kIRToggle + i * 6;
    const int muteParam = kIRMute + i * 6;
    if (mIRSlots[i].ir && GetParam(toggleParam)->Value())
    {
      toggledOnCount++;
      if (!GetParam(muteParam)->Bool())
        activeCount++;
    }
  }
  const double blendScale = activeCount > 1 ? 1.0 / static_cast<double>(activeCount) : 1.0;

  if (useStereo)
  {
    const size_t stereoChans = 2;
    mIRStereoBuffer.resize(stereoChans * numFrames);
    std::fill(mIRStereoBuffer.begin(), mIRStereoBuffer.end(), 0.0);

    for (int i = 0; i < kNumIRs; i++)
    {
      const int toggleParam = kIRToggle + i * 6;
      const int phaseParam = kIRPolarity + i * 6;
      const int levelParam = kIRLevel + i * 6;
      const int muteParam = kIRMute + i * 6;
      const int delayParam = kIRDelay + i * 6;
      const int panParam = kIRPan + i * 6;

      if (!mIRSlots[i].ir || !GetParam(toggleParam)->Value()) continue;

      sample** irOut = mIRSlots[i].ir->Process(mInputPointers, numChannelsInternal, numFrames);
      const int delay = GetParam(delayParam)->Int();
      const double pol = GetParam(phaseParam)->Bool() ? -1.0 : 1.0;
      const double level = GetParam(levelParam)->Value();
      const double mute = GetParam(muteParam)->Bool() ? 0.0 : 1.0;
      const double pan = GetParam(panParam)->Value();
      const double leftGain = pan <= 0.0 ? 1.0 : 1.0 - pan;
      const double rightGain = pan >= 0.0 ? 1.0 : 1.0 + pan;

      _ApplyIRDelay(irOut, delay, mIRSlots[i].delayBuffer, mIRSlots[i].delayWritePos, numChannelsInternal, numFrames);

      // Per-IR HPF
      {
        const int hpfBypP = kIRHPFBypass + i * 6;
        if (!GetParam(hpfBypP)->Bool())
        {
          const double hpfFreq = GetParam(kIRHPFreq + i * 6)->Value();
          const int nStages = GetParam(kIRHPFSlope + i * 6)->Int() + 1;
          auto& stages = mIRSlots[i].hpfStages;
          if ((int)stages.size() != nStages) stages.resize(nStages);
          for (auto& f : stages)
          {
            f.SetParams(recursive_linear_filter::HighPassParams(sampleRate, hpfFreq));
            irOut = f.Process(irOut, numChannelsInternal, numFrames);
          }
        }
      }
      // Per-IR LPF
      {
        const int lpfBypP = kIRLPFBypass + i * 6;
        if (!GetParam(lpfBypP)->Bool())
        {
          const double lpfFreq = GetParam(kIRLPFreq + i * 6)->Value();
          const int nStages = GetParam(kIRLPFSlope + i * 6)->Int() + 1;
          auto& stages = mIRSlots[i].lpfStages;
          if ((int)stages.size() != nStages) stages.resize(nStages);
          for (auto& f : stages)
          {
            f.SetParams(recursive_linear_filter::LowPassParams(sampleRate, lpfFreq));
            irOut = f.Process(irOut, numChannelsInternal, numFrames);
          }
        }
      }

      for (size_t s = 0; s < numFrames; s++)
      {
        const double s_val = irOut[0][s] * pol * level * mute * blendScale * kIRMakeupGain;
        mIRStereoBuffer[s] += s_val * leftGain;
        mIRStereoBuffer[numFrames + s] += s_val * rightGain;
      }
    }

    // If no IRs toggled on at all, pass through
    if (toggledOnCount == 0)
    {
      for (size_t s = 0; s < numFrames; s++)
      {
        mIRStereoBuffer[s] = mInputPointers[0][s];
        mIRStereoBuffer[numFrames + s] = mInputPointers[0][s];
      }
    }

    mIRBlendBuffer.resize(stereoChans * numFrames);
    for (size_t i = 0; i < stereoChans * numFrames; i++)
      mIRBlendBuffer[i] = mIRStereoBuffer[i];

    sample** stereoPtrs = new sample*[stereoChans];
    stereoPtrs[0] = mIRBlendBuffer.data();
    stereoPtrs[1] = mIRBlendBuffer.data() + numFrames;

    // DC blocker
    const double highPassCutoffFreq = kDCBlockerFrequency;
    const recursive_linear_filter::HighPassParams highPassParams(sampleRate, highPassCutoffFreq);
    mDCBlocker.SetParams(highPassParams);
    sample** hpfPointers = mDCBlocker.Process(stereoPtrs, stereoChans, numFrames);

    std::feupdateenv(&fe_state);
    _ProcessOutput(hpfPointers, outputs, numFrames, stereoChans, numChannelsExternalOut);
    delete[] stereoPtrs;
  }
  else
  {
    // Mono output
    mIRBlendBuffer.resize(numChannelsInternal * numFrames);
    std::fill(mIRBlendBuffer.begin(), mIRBlendBuffer.end(), 0.0);

    for (int i = 0; i < kNumIRs; i++)
    {
      const int toggleParam = kIRToggle + i * 6;
      const int phaseParam = kIRPolarity + i * 6;
      const int levelParam = kIRLevel + i * 6;
      const int muteParam = kIRMute + i * 6;
      const int delayParam = kIRDelay + i * 6;

      if (!mIRSlots[i].ir || !GetParam(toggleParam)->Value()) continue;

      sample** irOut = mIRSlots[i].ir->Process(mInputPointers, numChannelsInternal, numFrames);
      const int delay = GetParam(delayParam)->Int();
      const double pol = GetParam(phaseParam)->Bool() ? -1.0 : 1.0;
      const double level = GetParam(levelParam)->Value();
      const double mute = GetParam(muteParam)->Bool() ? 0.0 : 1.0;

      _ApplyIRDelay(irOut, delay, mIRSlots[i].delayBuffer, mIRSlots[i].delayWritePos, numChannelsInternal, numFrames);

      // Per-IR HPF
      {
        const int hpfBypP = kIRHPFBypass + i * 6;
        if (!GetParam(hpfBypP)->Bool())
        {
          const double hpfFreq = GetParam(kIRHPFreq + i * 6)->Value();
          const int nStages = GetParam(kIRHPFSlope + i * 6)->Int() + 1;
          auto& stages = mIRSlots[i].hpfStages;
          if ((int)stages.size() != nStages) stages.resize(nStages);
          for (auto& f : stages)
          {
            f.SetParams(recursive_linear_filter::HighPassParams(sampleRate, hpfFreq));
            irOut = f.Process(irOut, numChannelsInternal, numFrames);
          }
        }
      }
      // Per-IR LPF
      {
        const int lpfBypP = kIRLPFBypass + i * 6;
        if (!GetParam(lpfBypP)->Bool())
        {
          const double lpfFreq = GetParam(kIRLPFreq + i * 6)->Value();
          const int nStages = GetParam(kIRLPFSlope + i * 6)->Int() + 1;
          auto& stages = mIRSlots[i].lpfStages;
          if ((int)stages.size() != nStages) stages.resize(nStages);
          for (auto& f : stages)
          {
            f.SetParams(recursive_linear_filter::LowPassParams(sampleRate, lpfFreq));
            irOut = f.Process(irOut, numChannelsInternal, numFrames);
          }
        }
      }

      for (size_t c = 0; c < numChannelsInternal; c++)
        for (size_t s = 0; s < numFrames; s++)
          mIRBlendBuffer[c * numFrames + s] += irOut[c][s] * pol * level * mute * blendScale * kIRMakeupGain;
    }

    // If no IRs toggled on at all, pass through
    if (toggledOnCount == 0)
    {
      for (size_t c = 0; c < numChannelsInternal; c++)
        for (size_t s = 0; s < numFrames; s++)
          mIRBlendBuffer[c * numFrames + s] = mInputPointers[c][s];
    }

    sample** monoPtrs = new sample*[numChannelsInternal];
    for (size_t c = 0; c < numChannelsInternal; c++)
      monoPtrs[c] = &mIRBlendBuffer[c * numFrames];

    // DC blocker
    const double highPassCutoffFreq = kDCBlockerFrequency;
    const recursive_linear_filter::HighPassParams highPassParams(sampleRate, highPassCutoffFreq);
    mDCBlocker.SetParams(highPassParams);
    sample** hpfPointers = mDCBlocker.Process(monoPtrs, numChannelsInternal, numFrames);

    std::feupdateenv(&fe_state);
    _ProcessOutput(hpfPointers, outputs, numFrames, numChannelsInternal, numChannelsExternalOut);
    delete[] monoPtrs;
  }

  // Click-free fade for IR toggles
  if (mFadeCounter > 0)
  {
    for (size_t c = 0; c < numChannelsExternalOut; c++)
      for (size_t i = 0; i < numFrames && mFadeCounter > 0; i++, mFadeCounter--)
      {
        double gain = 1.0;
        if (mFadeCounter > 128)
          gain = (256 - mFadeCounter) / 128.0;
        else
          gain = mFadeCounter / 128.0;
        outputs[c][i] *= gain;
      }
  }

  _UpdateMeters(mPreGainPointers, outputs, numFrames, numChannelsInternal, numChannelsExternalOut);
}

void Impulse::OnReset()
{
  const auto sampleRate = GetSampleRate();
  const int maxBlockSize = GetBlockSize();

  const int tailCycles = 10;
  SetTailSize(tailCycles * (int)(sampleRate / kDCBlockerFrequency));
  mInputSender.Reset(sampleRate);
  mOutputSender.Reset(sampleRate);
  _ResetIRs(sampleRate, maxBlockSize);

#ifndef NO_IGRAPHICS
  // Refresh pan disabled state on reset (handles channel config changes)
  if (auto pGraphics = GetUI())
  {
    const bool stereo = mStereoOutput.load();
    for (int i = 0; i < kNumIRs; i++)
    {
      pGraphics->ForControlWithParam(kIRPan + i * 6, [stereo](IControl* c) {
        c->SetDisabled(!stereo);
        c->Hide(!stereo);
      });
      if (auto* panLabel = pGraphics->GetControlWithTag(kCtrlTagPanLabel + i))
      {
        panLabel->SetDisabled(!stereo);
        panLabel->Hide(!stereo);
      }
      if (!stereo)
        GetParam(kIRPan + i * 6)->Set(0.0);
    }
  }
#endif
}
#endif

void Impulse::OnIdle()
{
  mInputSender.TransmitData(*this);
  mOutputSender.TransmitData(*this);

  // Destroy retired IRs off the audio thread
  mIRRetirement.clear();
}

bool Impulse::SerializeState(IByteChunk& chunk) const
{
  WDL_String header("###Impulse###");
  chunk.PutStr(header.Get());
  WDL_String version(PLUG_VERSION_STR);
  chunk.PutStr(version.Get());
  for (int i = 0; i < kNumIRs; i++)
    chunk.PutStr(mIRSlots[i].path.Get());
  return SerializeParams(chunk);
}

int Impulse::UnserializeState(const IByteChunk& chunk, int startPos)
{
  WDL_String header;
  int pos = startPos;
  pos = chunk.GetStr(header, pos);

  const char* kExpectedHeader = "###Impulse###";
  if (strcmp(header.Get(), kExpectedHeader) == 0)
  {
    return _UnserializeStateWithKnownVersion(chunk, pos);
  }
  else
  {
    return _UnserializeStateWithUnknownVersion(chunk, startPos);
  }
}

void Impulse::OnUIOpen()
{
  Plugin::OnUIOpen();

#ifndef NO_IGRAPHICS
  if (auto pGraphics = GetUI())
  {
    const bool stereo = NOutChansConnected() > 1;
    for (int i = 0; i < kNumIRs; i++)
    {
      pGraphics->ForControlWithParam(kIRPan + i * 6, [stereo](IControl* c) {
        c->SetDisabled(!stereo);
        c->Hide(!stereo);
      });
      if (auto* panLabel = pGraphics->GetControlWithTag(kCtrlTagPanLabel + i))
      {
        panLabel->SetDisabled(!stereo);
        panLabel->Hide(!stereo);
      }
      if (!stereo)
        GetParam(kIRPan + i * 6)->Set(0.0);
    }
  }
#endif

  for (int i = 0; i < kNumIRs; i++)
  {
    if (mIRSlots[i].path.GetLength())
    {
      int msg = kMsgTagLoadedIR + i;
      int tag = kCtrlTagIRFileBrowser + i;
      SendControlMsgFromDelegate(tag, msg, mIRSlots[i].path.GetLength(), mIRSlots[i].path.Get());
      if (mIRSlots[i].ir == nullptr && mIRSlots[i].stagedIR == nullptr)
        SendControlMsgFromDelegate(tag, kMsgTagLoadFailed + i, 0, nullptr);
    }
  }
}

void Impulse::OnParentWindowResize(int width, int height)
{
  if (auto* pGraphics = GetUI())
  {
    const double platformScale = pGraphics->GetPlatformWindowScale();
    const int logicalW = static_cast<int>(width / platformScale);
    const int logicalH = static_cast<int>(height / platformScale);
    const int designW = pGraphics->Width();
    const int designH = pGraphics->Height();
    const float scaleX = static_cast<float>(logicalW) / static_cast<float>(designW);
    const float scaleY = static_cast<float>(logicalH) / static_cast<float>(designH);
    const float newScale = std::min(scaleX, scaleY);
    pGraphics->Resize(designW, designH, newScale, false);
    pGraphics->ForAllControlsFunc([](IControl* pControl) { pControl->OnRescale(); });
    pGraphics->SetAllControlsDirty();
  }
}

bool Impulse::ConstrainEditorResize(int& w, int& h) const
{
  const float aspect = static_cast<float>(PLUG_WIDTH) / static_cast<float>(PLUG_HEIGHT);
  const float newAspect = static_cast<float>(w) / static_cast<float>(h);

  if (newAspect > aspect)
    w = static_cast<int>(h * aspect);
  else
    h = static_cast<int>(w / aspect);

  w = Clip(w, PLUG_WIDTH / 3, PLUG_WIDTH * 4);
  h = Clip(h, PLUG_HEIGHT / 3, PLUG_HEIGHT * 4);

  return false;
}

void Impulse::OnParamChange(int paramIdx)
{
  switch (paramIdx)
  {
    case kInputLevel:
    case kInputTrim:
    {
      double inputGainDB = GetParam(kInputLevel)->Value() + GetParam(kInputTrim)->Value();
      mInputGain = DBToAmp(inputGainDB);
      break;
    }
    case kOutputLevel:
    {
      mOutputGain = DBToAmp(GetParam(kOutputLevel)->Value());
      break;
    }
    case kIRMode:
    {
      mIRMode = GetParam(kIRMode)->Int();
      for (auto& slot : mIRSlots)
        if (slot.ir) slot.ir->SetMode(mIRMode);
      break;
    }
    case kIRPan:
    case kIRPan2:
    case kIRPan3:
    case kIRPan4:
      if (NOutChansConnected() <= 1)
        GetParam(paramIdx)->Set(0.0);
      break;
    default: break;
  }
}

void Impulse::OnParamChangeUI(int paramIdx, EParamSource source)
{
#ifndef NO_IGRAPHICS
  if (auto pGraphics = GetUI())
  {
    bool active = GetParam(paramIdx)->Bool();
    const bool stereo = mStereoOutput.load();

    auto _SetPanState = [&](int toggleParam, int panParam)
    {
      bool toggleOn = toggleParam == paramIdx ? active : GetParam(toggleParam)->Bool();
      pGraphics->ForControlWithParam(panParam, [toggleOn, stereo](IControl* pPan) {
        pPan->SetDisabled(!toggleOn);
        pPan->Hide(!toggleOn || !stereo);
      });
      if (!stereo)
        GetParam(panParam)->Set(0.0);
    };

    for (int i = 0; i < kNumIRs; i++)
      _SetPanState(kIRToggle + i * 6, kIRPan + i * 6);

    switch (paramIdx)
    {
      case kIRToggle:
      case kIRToggle2:
      case kIRToggle3:
      case kIRToggle4:
        mFadeCounter = 256;
        break;
      default:
        break;
    }
  }
#endif
}

bool Impulse::OnMessage(int msgTag, int ctrlTag, int dataSize, const void* pData)
{
  switch (msgTag)
  {
    case kMsgTagClearIR:
    case kMsgTagClearIR2:
    case kMsgTagClearIR3:
    case kMsgTagClearIR4:
    {
      int irIndex = msgTag - kMsgTagClearIR;
      mIRSlots[irIndex].ir = nullptr;
      mIRSlots[irIndex].path.Set("");
      mIRSlots[irIndex].shouldRemove = false;
      return true;
    }
    case kMsgTagHighlightColor:
    {
      mHighLightColor.Set((const char*)pData);
#ifndef NO_IGRAPHICS
      if (GetUI())
      {
        GetUI()->ForStandardControlsFunc([&](IControl* pControl) {
          if (auto* pVectorBase = pControl->As<IVectorBase>())
          {
            IColor color = IColor::FromColorCodeStr(mHighLightColor.Get());
            pVectorBase->SetColor(kX1, color);
            pVectorBase->SetColor(kPR, color.WithOpacity(0.3f));
            pVectorBase->SetColor(kFR, color.WithOpacity(0.4f));
            pVectorBase->SetColor(kX3, color.WithContrast(0.1f));
          }
          pControl->GetUI()->SetAllControlsDirty();
        });
      }
#endif
      return true;
    }
    default: return false;
  }
}

// Private methods ============================================================

void Impulse::_AllocateIOPointers(const size_t nChans)
{
  if (mInputPointers != nullptr)
    throw std::runtime_error("Tried to re-allocate mInputPointers without freeing");
  mInputPointers = new sample*[nChans];
  if (mInputPointers == nullptr)
    throw std::runtime_error("Failed to allocate pointer to input buffer!\n");
  if (mOutputPointers != nullptr)
    throw std::runtime_error("Tried to re-allocate mOutputPointers without freeing");
  mOutputPointers = new sample*[nChans];
  if (mOutputPointers == nullptr)
    throw std::runtime_error("Failed to allocate pointer to output buffer!\n");
}

void Impulse::_ApplyDSPStaging()
{
  for (int i = 0; i < kNumIRs; i++)
  {
    if (mIRSlots[i].shouldRemove)
    {
      mIRSlots[i].ir = nullptr;
      mIRSlots[i].path.Set("");
      mIRSlots[i].shouldRemove = false;
    }
    if (mIRSlots[i].stagedIR != nullptr)
    {
      // Retire old IR for deferred destruction (avoids vDSP_destroy_fftsetup on audio thread)
      if (mIRSlots[i].ir)
        mIRRetirement.push_back(std::move(mIRSlots[i].ir));
      mIRSlots[i].ir = std::move(mIRSlots[i].stagedIR);
      mIRSlots[i].stagedIR = nullptr;
      // Reset delay buffer for clean start
      mIRSlots[i].delayBuffer.assign(mIRSlots[i].delayBuffer.size(), 0.0);
      mIRSlots[i].delayWritePos = 0;
    }
  }
}

void Impulse::_DeallocateIOPointers()
{
  if (mInputPointers != nullptr)
  {
    delete[] mInputPointers;
    mInputPointers = nullptr;
  }
  if (mInputPointers != nullptr)
    throw std::runtime_error("Failed to deallocate pointer to input buffer!\n");
  if (mOutputPointers != nullptr)
  {
    delete[] mOutputPointers;
    mOutputPointers = nullptr;
  }
  if (mOutputPointers != nullptr)
    throw std::runtime_error("Failed to deallocate pointer to output buffer!\n");
}

void Impulse::_ResetIRs(const double sampleRate, const int maxBlockSize)
{
  auto resetIR = [&](std::unique_ptr<dsp::ImpulseResponse>& staged, std::unique_ptr<dsp::ImpulseResponse>& active) {
    if (staged != nullptr)
    {
      staged->ResetFFT();
      const double irSampleRate = staged->GetSampleRate();
      if (irSampleRate != sampleRate)
      {
        const auto irData = staged->GetData();
        staged = std::make_unique<dsp::ImpulseResponse>(irData, sampleRate);
      }
    }
    else if (active != nullptr)
    {
      active->ResetFFT();
      const double irSampleRate = active->GetSampleRate();
      if (irSampleRate != sampleRate)
      {
        const auto irData = active->GetData();
        staged = std::make_unique<dsp::ImpulseResponse>(irData, sampleRate);
      }
    }
  };

  for (auto& slot : mIRSlots)
    resetIR(slot.stagedIR, slot.ir);
}

dsp::wav::LoadReturnCode Impulse::_StageIR(const WDL_String& irPath, int irIndex)
{
  WDL_String previousIRPath = mIRSlots[irIndex].path;
  const double sampleRate = GetSampleRate();
  dsp::wav::LoadReturnCode wavState = dsp::wav::LoadReturnCode::ERROR_OTHER;
  try
  {
    auto irPathU8 = std::filesystem::u8path(irPath.Get());
    mIRSlots[irIndex].stagedIR = std::make_unique<dsp::ImpulseResponse>(irPathU8.string().c_str(), sampleRate);
    wavState = mIRSlots[irIndex].stagedIR->GetWavState();
  }
  catch (std::runtime_error& e)
  {
    wavState = dsp::wav::LoadReturnCode::ERROR_OTHER;
    std::cerr << "Caught unhandled exception while attempting to load IR:" << std::endl;
    std::cerr << e.what() << std::endl;
  }

  if (wavState == dsp::wav::LoadReturnCode::SUCCESS)
  {
    mIRSlots[irIndex].path = irPath;
    int msgTag = kMsgTagLoadedIR + irIndex;
    int ctrlTag = kCtrlTagIRFileBrowser + irIndex;
    SendControlMsgFromDelegate(ctrlTag, msgTag, mIRSlots[irIndex].path.GetLength(), mIRSlots[irIndex].path.Get());
  }
  else
  {
    if (mIRSlots[irIndex].stagedIR != nullptr)
      mIRSlots[irIndex].stagedIR = nullptr;
    mIRSlots[irIndex].path = previousIRPath;
    int msgTag = kMsgTagLoadFailed + irIndex;
    int ctrlTag = kCtrlTagIRFileBrowser + irIndex;
    SendControlMsgFromDelegate(ctrlTag, msgTag, 0, nullptr);
  }

  return wavState;
}

size_t Impulse::_GetBufferNumChannels() const
{
  return mInputArray.size();
}

size_t Impulse::_GetBufferNumFrames() const
{
  if (_GetBufferNumChannels() == 0)
    return 0;
  return mInputArray[0].size();
}

void Impulse::_PrepareBuffers(const size_t numChannels, const size_t numFrames)
{
  const bool updateChannels = numChannels != _GetBufferNumChannels();
  const bool updateFrames = updateChannels || (_GetBufferNumFrames() != numFrames);

  if (updateChannels)
  {
    _PrepareIOPointers(numChannels);
    mInputArray.resize(numChannels);
    mOutputArray.resize(numChannels);
  }
  if (updateFrames)
  {
    for (auto c = 0; c < mInputArray.size(); c++)
    {
      mInputArray[c].resize(numFrames);
      std::fill(mInputArray[c].begin(), mInputArray[c].end(), 0.0);
    }
    for (auto c = 0; c < mOutputArray.size(); c++)
    {
      mOutputArray[c].resize(numFrames);
      std::fill(mOutputArray[c].begin(), mOutputArray[c].end(), 0.0);
    }
    mPreGainBuffer.resize(numFrames);
    std::fill(mPreGainBuffer.begin(), mPreGainBuffer.end(), 0.0);
  }
  for (auto c = 0; c < mInputArray.size(); c++)
    mInputPointers[c] = mInputArray[c].data();
  for (auto c = 0; c < mOutputArray.size(); c++)
    mOutputPointers[c] = mOutputArray[c].data();
  mPreGainPointers[0] = mPreGainBuffer.data();
}

void Impulse::_PrepareIOPointers(const size_t numChannels)
{
  _DeallocateIOPointers();
  _AllocateIOPointers(numChannels);
}

void Impulse::_ProcessInput(iplug::sample** inputs, const size_t nFrames, const size_t nChansIn,
                                   const size_t nChansOut)
{
  if (nChansOut != 1)
  {
    std::stringstream ss;
    ss << "Expected mono output, but " << nChansOut << " output channels are requested!";
    throw std::runtime_error(ss.str());
  }

  double gain = mInputGain;
#ifndef APP_API
  gain /= (float)nChansIn;
#endif

  for (size_t c = 0; c < nChansIn; c++)
    for (size_t s = 0; s < nFrames; s++)
      if (c == 0)
        mInputArray[0][s] = gain * inputs[c][s];
      else
        mInputArray[0][s] += gain * inputs[c][s];

  for (size_t s = 0; s < nFrames; s++)
    mPreGainBuffer[s] = mInputArray[0][s];
}

void Impulse::_ProcessOutput(iplug::sample** inputs, iplug::sample** outputs, const size_t nFrames,
                                      const size_t nChansIn, const size_t nChansOut)
{
  const double gain = mOutputGain;
  if (nChansIn == 1)
  {
    for (auto cout = 0; cout < nChansOut; cout++)
      for (auto s = 0; s < nFrames; s++)
#ifdef APP_API
        outputs[cout][s] = std::clamp(gain * inputs[0][s], -1.0, 1.0);
#else
        outputs[cout][s] = gain * inputs[0][s];
#endif
  }
  else if (nChansIn == 2)
  {
    if (nChansOut == 1)
    {
      for (auto s = 0; s < nFrames; s++)
      {
        const double sum = gain * (inputs[0][s] + inputs[1][s]) * 0.5;
#ifdef APP_API
        outputs[0][s] = std::clamp(sum, -1.0, 1.0);
#else
        outputs[0][s] = sum;
#endif
      }
    }
    else
    {
      for (auto cout = 0; cout < nChansOut; cout++)
        for (auto s = 0; s < nFrames; s++)
#ifdef APP_API
          outputs[cout][s] = std::clamp(gain * inputs[cout][s], -1.0, 1.0);
#else
          outputs[cout][s] = gain * inputs[cout][s];
#endif
    }
  }
  else
  {
    throw std::runtime_error("Plugin does not support more than 2 internal channels.");
  }
}

void Impulse::_ApplyIRDelay(iplug::sample** signal, int delay, std::vector<double>& buf, size_t& writePos, size_t nChans, size_t nFrames)
{
  if (delay == 0) return;

  const size_t bufLen = static_cast<size_t>(delay) + nFrames;
  if (buf.size() < bufLen * nChans)
    buf.assign(bufLen * nChans, 0.0);

  for (size_t c = 0; c < nChans; c++)
  {
    for (size_t i = 0; i < nFrames; i++)
    {
      const double input = signal[c][i];
      const size_t readPos = (writePos + i + bufLen - static_cast<size_t>(delay)) % bufLen;
      signal[c][i] = buf[c * bufLen + readPos];
      buf[c * bufLen + ((writePos + i) % bufLen)] = input;
    }
  }
  writePos = (writePos + nFrames) % bufLen;
}

void Impulse::_UpdateMeters(iplug::sample** inputPointer, iplug::sample** outputPointer, const size_t nFrames,
                                     const size_t nChansIn, const size_t nChansOut)
{
  const int nChansHack = 1;
  mInputSender.ProcessBlock(inputPointer, (int)nFrames, kCtrlTagInputMeter, nChansHack);
  mOutputSender.ProcessBlock(outputPointer, (int)nFrames, kCtrlTagOutputMeter, nChansHack);
}

// Unserialization is included from Unserialization.cpp
#include "Unserialization.cpp"
