#include <algorithm> // std::clamp, std::min
#include <cmath> // pow
#include <filesystem>
#include <iostream>
#include <utility>

static constexpr int kBlendValues[3] = {1, 2, 3};
#include <dlfcn.h>

#include "Colors.h"
#include "IPlugPaths.h"
#include "../NeuralAmpModelerCore/NAM/activations.h"
#include "../NeuralAmpModelerCore/NAM/get_dsp.h"
// clang-format off
// These includes need to happen in this order or else the latter won't know
// a bunch of stuff.
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
const IVStyle radioButtonStyle =
  style
    .WithColor(EVColor::kON, PluginColors::NAM_THEMECOLOR) // Pressed buttons and their labels
    .WithColor(EVColor::kOFF, PluginColors::NAM_THEMECOLOR.WithOpacity(0.1f)) // Unpressed buttons
    .WithColor(EVColor::kX1, PluginColors::NAM_THEMECOLOR.WithOpacity(0.6f)); // Unpressed buttons' labels

#ifndef NO_IGRAPHICS
EMsgBoxResult _ShowMessageBox(iplug::igraphics::IGraphics* pGraphics, const char* str, const char* caption,
                              EMsgBoxType type)
{
#ifdef OS_MAC
  // macOS is backwards?
  return pGraphics->ShowMessageBox(caption, str, type);
#else
  return pGraphics->ShowMessageBox(str, caption, type);
#endif
}
#endif

const std::string kCalibrateInputParamName = "CalibrateInput";
const bool kDefaultCalibrateInput = true;
const std::string kInputCalibrationLevelParamName = "InputCalibrationLevel";
const double kDefaultInputCalibrationLevel = 22.5;
const double kDefaultInputCalibrationLevel2 = 17.8;

static int _ImpulseBinaryMarker = 0;


Impulse::Impulse(const InstanceInfo& info)
: Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
  nam::activations::Activation::enable_fast_tanh();
  GetParam(kInputLevel)->InitDouble("Gain", 5.0, 0.0, 10.0, 0.1, "");
  GetParam(kToneMid)->InitDouble("Tone", 5.0, 0.0, 10.0, 0.1);

  GetParam(kOutputLevel)->InitGain("LVL", 0.0, -45.0, 0.0, 0.1);
  GetParam(kInputLevel2)->InitDouble("Volume", 5.0, 0.0, 10.0, 0.1, "");
  GetParam(kOutputLevel2)->InitGain("Output", 0.0, -40.0, 40.0, 0.1, IParam::kFlagMeta);
  GetParam(kInputTrim)->InitDouble("Input Trim", 0.0, -40.0, 40.0, 0.1, "dB");
  GetParam(kNoiseGateThreshold)->InitGain("Gate", -80.0, -100.0, 0.0, 0.1);
  GetParam(kNoiseGateActive)->InitBool("NoiseGateActive", true);
  mNoiseGateTrigger.AddListener(&mNoiseGateGain);
  GetParam(kOutputMode)->InitEnum("OutputMode", 0, {"Raw"});
  GetParam(kOutputMode2)->InitEnum("OutputMode2", 0, {"Normalized"});
  GetParam(kAutoGain)->InitBool("AutoGain", true);
  GetParam(kOversampling)->InitEnum("OS", 0, {"Off"});
  GetParam(kIRToggle)->InitBool("IRToggle", true);
  GetParam(kIRToggle2)->InitBool("IRToggle2", true);
  GetParam(kIRPolarity)->InitBool("IRPolarity", false);
  GetParam(kIRPolarity2)->InitBool("IRPolarity2", false);
  GetParam(kIRLevel)->InitDouble("IRLevel", 1.0, 0.0, 1.0, 0.01, "");
  GetParam(kIRLevel2)->InitDouble("IRLevel2", 1.0, 0.0, 1.0, 0.01, "");
  GetParam(kIRMute)->InitBool("IRMute", false);
  GetParam(kIRMute2)->InitBool("IRMute2", false);
  GetParam(kIRDelay)->InitInt("IR Delay", 0, 0, 99, "%i samples");
  GetParam(kIRDelay2)->InitInt("IR Delay 2", 0, 0, 99, "%i samples");
  GetParam(kCalibrateInput)->InitBool(kCalibrateInputParamName.c_str(), kDefaultCalibrateInput);
  GetParam(kInputCalibrationLevel)
    ->InitDouble(kInputCalibrationLevelParamName.c_str(), kDefaultInputCalibrationLevel, -60.0, 60.0, 0.1, "dBu");
GetParam(kCalibrateInput2)->InitBool("CalibrateInput2", true);
  GetParam(kInputCalibrationLevel2)
  ->InitDouble("InputCalibrationLevel2", kDefaultInputCalibrationLevel2, -60.0, 60.0, 0.1, "dBu");
  GetParam(kIRPan)->InitDouble("IR 1 Pan", 0.0, -1.0, 1.0, 0.01, "");
  GetParam(kIRPan2)->InitDouble("IR 2 Pan", 0.0, -1.0, 1.0, 0.01, "");
  GetParam(kIRMode)->InitEnum("IR Mode", 0, {"Zero Latency", "Normal"});
  GetParam(kBlendNearest)->InitEnum("CPU Usage", 2, {"Light", "Normal", "HQ"});


  mAutoGainOutputBase = GetParam(kOutputLevel2)->Value();

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
    const auto polarityOnSVG = pGraphics->LoadSVG(POLARITY_ON_FN);
    const auto polarityOffSVG = pGraphics->LoadSVG(POLARITY_OFF_FN);

    const auto backgroundBitmap = pGraphics->LoadBitmap(BACKGROUND_FN);
    const auto controlsBackgroundBitmap = pGraphics->LoadBitmap(CONTROLSBACKGROUND_FN);
    const auto fileBackgroundBitmap = pGraphics->LoadBitmap(FILEBACKGROUND_FN);
    const auto inputLevelBackgroundBitmap = pGraphics->LoadBitmap(INPUTLEVELBACKGROUND_FN);
    const auto linesBitmap = pGraphics->LoadBitmap(LINES_FN);
    const auto knobBackgroundBitmap = pGraphics->LoadBitmap(KNOBBACKGROUND_FN);
    const auto switchHandleBitmap = pGraphics->LoadBitmap(SLIDESWITCHHANDLE_FN);
    const auto meterBackgroundBitmap = pGraphics->LoadBitmap(METERBACKGROUND_FN);
    const auto logoBitmap = pGraphics->LoadBitmap(LOGO_FN);

    const auto b = pGraphics->GetBounds();
    const auto mainArea = b.GetPadded(-20);
    const auto contentArea = mainArea.GetPadded(-10);
    const auto topBarHeight = 96.0f;
    const float topBarKnobH = 64.0f;

    // ===== TOP BAR: IN + meter | Gate | Trim | Logo | OUT =====
    const auto topBar = contentArea.GetFromTop(topBarHeight);
    const float meterW = 14.0f;
    const float meterH = 32.0f;
    const float topBarPad = 25.0f;

    // IN label + meter — compact
    const float inLabelW = 22.0f;
    const auto inLabelArea = IRECT(topBar.L + topBarPad, topBar.T,
                                   topBar.L + topBarPad + inLabelW, topBar.B)
                               .GetCentredInside(inLabelW, 22);
    const auto inMeterArea = IRECT(inLabelArea.R + 4, topBar.T,
                                   inLabelArea.R + 4 + meterW, topBar.B)
                               .GetCentredInside(meterW, meterH);

    // Gate knob between IN meter and Input Trim
    const float gateKnobW = 64.0f;
    const float gateLabelH = 12.0f;
    const auto gateArea = IRECT(inMeterArea.R + 4, topBar.T + 2,
                                inMeterArea.R + 4 + gateKnobW, topBar.B - 2);
    const auto gateLabelArea = gateArea.GetFromTop(gateLabelH).GetCentredInside(gateKnobW, gateLabelH);
    const auto gateKnobArea = IRECT(gateArea.L, gateLabelArea.B + 2,
                                    gateArea.R, gateLabelArea.B + 2 + topBarKnobH);
    const auto gateValueArea = IRECT(gateArea.L, gateKnobArea.B + 2,
                                     gateArea.R, gateArea.B);

    // Input trim knob
    const float inTrimW = 64.0f;
    const auto inTrimArea = IRECT(gateArea.R + 4, topBar.T + 2,
                                  gateArea.R + 4 + inTrimW, topBar.B - 2);
    const auto inTrimLabelArea = inTrimArea.GetFromTop(gateLabelH).GetCentredInside(inTrimW, gateLabelH);
    const auto inTrimKnobArea = IRECT(inTrimArea.L, inTrimLabelArea.B + 2,
                                      inTrimArea.R, inTrimLabelArea.B + 2 + topBarKnobH);
    const auto inTrimValueArea = IRECT(inTrimArea.L, inTrimKnobArea.B + 2,
                                       inTrimArea.R, inTrimArea.B);

    // OUT section: label above knob | OUT label | meter (mirrors IN side)
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
    const auto outKnobLabelArea = outKnobContainer.GetFromTop(gateLabelH).GetCentredInside(outKnobContainer.W(), gateLabelH);
    const auto outKnobSubArea = IRECT(outKnobContainer.L, outKnobLabelArea.B + 2,
                                      outKnobContainer.R, outKnobLabelArea.B + 2 + topBarKnobH);
    const auto outKnobValueArea = IRECT(outKnobContainer.L, outKnobSubArea.B + 2,
                                        outKnobContainer.R, outKnobContainer.B);

    // Center logo area
    const auto logoArea = topBar.GetCentredInside(140, 55);

    // ===== MAIN CONTENT AREA: Amp image | IR section =====
    const auto mainContentArea = contentArea.GetReducedFromTop(topBarHeight).GetReducedFromBottom(NAM_KNOB_HEIGHT);
    const float halfW = contentArea.W() * 0.5f;

    // Left half: amp image
    const auto ampImageArea = IRECT(contentArea.L, mainContentArea.T,
                                    contentArea.L + halfW, mainContentArea.B);

    // Right half: IR section
    const auto irSection = IRECT(contentArea.L + halfW, mainContentArea.T,
                                 contentArea.R, mainContentArea.B)
                             .GetPadded(-10, -10, -10, -10);

    // Vertical separator in main area
    const auto separatorArea = IRECT(contentArea.L + halfW - 1, mainContentArea.T,
                                     contentArea.L + halfW + 1, mainContentArea.B);

    // ===== BOTTOM CONTROLS =====
    const auto controlsBgTop = mainContentArea.B;
    const auto controlsBgArea = IRECT(b.L, controlsBgTop, b.R, b.B);

    const auto numKnobs = 5;
    const auto singleKnobPad = -2.0f;
    const auto knobsPad = 12.0f;
    const auto knobsCenterY = controlsBgArea.MH();
    const auto knobsTop = knobsCenterY - NAM_KNOB_HEIGHT / 2.0f;
    const auto knobsArea = IRECT(contentArea.L + knobsPad, knobsTop,
                                 contentArea.R - knobsPad, knobsTop + NAM_KNOB_HEIGHT);

    const auto inputKnobArea = knobsArea.GetGridCell(0, 0, 1, numKnobs).GetPadded(-singleKnobPad);
    const auto midKnobArea = knobsArea.GetGridCell(0, 1, 1, numKnobs).GetPadded(-singleKnobPad);
    const auto outputKnobArea = knobsArea.GetGridCell(0, 2, 1, numKnobs).GetPadded(-singleKnobPad);
    const auto input2KnobArea = knobsArea.GetGridCell(0, 3, 1, numKnobs).GetPadded(-singleKnobPad);

    // IR section layout
    const auto knobSize = 130.0f;
    const auto spacing = 5.0f;
    const auto fileHeight = 30.0f;
    const auto fileWidth = irSection.W() * 0.5f;

    // IR controls in upper portion of IR section
    const auto irControlsTop = irSection.T + irSection.H() * 0.12f;
    const auto irKnobRow = IRECT(irSection.L, irControlsTop, irSection.R, irControlsTop + knobSize)
                              .GetPadded(-8, -4, -8, -4);
    const auto ir1Half = irKnobRow.GetFromLeft(irKnobRow.W() * 0.5f);
    const auto ir2Half = irKnobRow.GetFromRight(irKnobRow.W() * 0.5f);
    const float btnSz = 26.0f;
    const float panKnobSz = 52.0f;
    const float irLabelH = 11.0f;
    const float irLabelGap = 2.0f;
    // IR1 controls: Phase | Mute | Delay | Pan (same Y center = straight line)
    const auto irLevelArea = ir1Half.GetFromLeft(100.0f).GetCentredInside(100.0f, knobSize);
    const auto ir1Ctrls = ir1Half.GetFromRight(ir1Half.W() - 100.0f);
    const auto ir1CtrlW = ir1Ctrls.W() / 4.0f;
    const auto ir1PhaseArea = IRECT(ir1Ctrls.L, ir1Ctrls.T, ir1Ctrls.L + ir1CtrlW, ir1Ctrls.B).GetCentredInside(btnSz, btnSz);
    const auto ir1MuteArea = IRECT(ir1Ctrls.L + ir1CtrlW, ir1Ctrls.T, ir1Ctrls.L + ir1CtrlW * 2.0f, ir1Ctrls.B).GetCentredInside(btnSz, btnSz);
    const auto ir1DelayCircleArea = IRECT(ir1Ctrls.L + ir1CtrlW * 2.0f, ir1Ctrls.T, ir1Ctrls.L + ir1CtrlW * 3.0f, ir1Ctrls.B).GetCentredInside(btnSz, btnSz);
    const auto ir1DelayLabelArea = IRECT(ir1Ctrls.L + ir1CtrlW * 2.0f, ir1DelayCircleArea.B + irLabelGap, ir1Ctrls.L + ir1CtrlW * 3.0f, ir1DelayCircleArea.B + irLabelGap + irLabelH);
    const auto ir1PanColumnArea = IRECT(ir1Ctrls.L + ir1CtrlW * 3.0f, ir1Ctrls.T, ir1Ctrls.R, ir1Ctrls.B);
    // IR2 controls: Phase | Mute | Delay | Pan (same Y center = straight line)
    const auto ir2LevelArea = ir2Half.GetFromLeft(100.0f).GetCentredInside(100.0f, knobSize);
    const auto ir2Ctrls = ir2Half.GetFromRight(ir2Half.W() - 100.0f);
    const auto ir2CtrlW = ir2Ctrls.W() / 4.0f;
    const auto ir2PhaseArea = IRECT(ir2Ctrls.L, ir2Ctrls.T, ir2Ctrls.L + ir2CtrlW, ir2Ctrls.B).GetCentredInside(btnSz, btnSz);
    const auto ir2MuteArea = IRECT(ir2Ctrls.L + ir2CtrlW, ir2Ctrls.T, ir2Ctrls.L + ir2CtrlW * 2.0f, ir2Ctrls.B).GetCentredInside(btnSz, btnSz);
    const auto ir2DelayCircleArea = IRECT(ir2Ctrls.L + ir2CtrlW * 2.0f, ir2Ctrls.T, ir2Ctrls.L + ir2CtrlW * 3.0f, ir2Ctrls.B).GetCentredInside(btnSz, btnSz);
    const auto ir2DelayLabelArea = IRECT(ir2Ctrls.L + ir2CtrlW * 2.0f, ir2DelayCircleArea.B + irLabelGap, ir2Ctrls.L + ir2CtrlW * 3.0f, ir2DelayCircleArea.B + irLabelGap + irLabelH);
    const auto ir2PanColumnArea = IRECT(ir2Ctrls.L + ir2CtrlW * 3.0f, ir2Ctrls.T, ir2Ctrls.R, ir2Ctrls.B);

    // File rows near bottom of IR section
    const auto irFilesBottom = irSection.B - 15.0f;
    const auto ir2Row = IRECT(irSection.L, irFilesBottom - fileHeight,
                                irSection.R, irFilesBottom).GetMidHPadded(fileWidth);
    const auto ir2SwitchArea = ir2Row.GetFromLeft(22.0f).GetScaledAboutCentre(0.55f);
    const auto ir2Area = ir2Row.GetPadded(-14.0f, 0.0f, -4.0f, 0.0f);

    const auto irRow = ir2Row.GetVShifted(-spacing - fileHeight);
    const auto irSwitchArea = irRow.GetFromLeft(22.0f).GetScaledAboutCentre(0.55f);
    const auto irArea = irRow.GetPadded(-14.0f, 0.0f, -4.0f, 0.0f);

    // Misc Areas
    const auto settingsButtonArea = CornerButtonArea(b);

    // IR loader button
    auto loadIRCompletionHandler = [&](const WDL_String& fileName, const WDL_String& path) {
      if (fileName.GetLength())
      {
        mIRPath = fileName;
        const dsp::wav::LoadReturnCode retCode = _StageIR(fileName);
        if (retCode != dsp::wav::LoadReturnCode::SUCCESS)
        {
          std::stringstream message;
          message << "Failed to load IR file " << fileName.Get() << ":\n";
          message << dsp::wav::GetMsgForLoadReturnCode(retCode);

          _ShowMessageBox(GetUI(), message.str().c_str(), "Failed to load IR!", kMB_OK);
        }
      }
    };

    // Draw background
    pGraphics->AttachControl(new NAMBackgroundControl(b, PluginColors::NAM_BG_BLUE));
    // Controls background image at bottom
    pGraphics->AttachControl(new NAMAmpImageControl(controlsBgArea, controlsBackgroundBitmap, true));
    // Amp image on left
    pGraphics->AttachControl(new NAMAmpImageControl(ampImageArea, backgroundBitmap));
    // Vertical separator in main area
    pGraphics->AttachControl(new NAMLineControl(separatorArea, PluginColors::NAM_THEMEFONTCOLOR.WithOpacity(0.4f), 2.0f));

    // Top bar elements
    pGraphics->AttachControl(new NAMAmpImageControl(logoArea, logoBitmap));
    pGraphics->AttachControl(new ITextControl(inLabelArea, "IN", IText(16, PluginColors::NAM_THEMEFONTCOLOR, "Michroma-Regular")));
    pGraphics->AttachControl(new NAMMeterControl(inMeterArea, meterBackgroundBitmap, style), kCtrlTagInputMeter);
    pGraphics->AttachControl(new ITextControl(gateLabelArea, "Gate", IText(11, PluginColors::NAM_THEMEFONTCOLOR, "Michroma-Regular")));
    {
      auto* gateKnob = new NAMGateKnobControl(gateKnobArea, kNoiseGateThreshold, "Gate", style, knobBackgroundBitmap);
      gateKnob->HideLabel();
      gateKnob->HideValue();
      pGraphics->AttachControl(gateKnob);
    }
    pGraphics->AttachControl(new ICaptionControl(gateValueArea, kNoiseGateThreshold,
      IText(11, PluginColors::NAM_THEMEFONTCOLOR, "Michroma-Regular"), COLOR_TRANSPARENT, true));
    pGraphics->AttachControl(new ITextControl(inTrimLabelArea, "Input", IText(11, PluginColors::NAM_THEMEFONTCOLOR, "Michroma-Regular")));
    {
      auto* trimKnob = new NAMKnobControl(inTrimKnobArea, kInputTrim, "", style, knobBackgroundBitmap);
      trimKnob->HideLabel();
      trimKnob->HideValue();
      pGraphics->AttachControl(trimKnob);
    }
    pGraphics->AttachControl(new ICaptionControl(inTrimValueArea, kInputTrim,
      IText(11, PluginColors::NAM_THEMEFONTCOLOR, "Michroma-Regular"), COLOR_TRANSPARENT, true));
    pGraphics->AttachControl(new ITextControl(outKnobLabelArea, "Output", IText(11, PluginColors::NAM_THEMEFONTCOLOR, "Michroma-Regular")));
    {
      auto* outKnob = new NAMKnobControl(outKnobSubArea, kOutputLevel2, "", style, knobBackgroundBitmap);
      outKnob->HideLabel();
      outKnob->HideValue();
      pGraphics->AttachControl(outKnob);
    }
    pGraphics->AttachControl(new ICaptionControl(outKnobValueArea, kOutputLevel2,
      IText(11, PluginColors::NAM_THEMEFONTCOLOR, "Michroma-Regular"), COLOR_TRANSPARENT, true));
    pGraphics->AttachControl(new ITextControl(outLabelArea, "OUT", IText(16, PluginColors::NAM_THEMEFONTCOLOR, "Michroma-Regular")));
    pGraphics->AttachControl(new NAMMeterControl(outMeterArea, meterBackgroundBitmap, style), kCtrlTagOutputMeter);

    const char* const defaultIRString = "Select IR...";

    // Getting started page listing additional resources
    const char* const getUrl = "https://www.tonkraf.com/store/p/the-phoenix-head";

    // IR 1 loader (stacked)
    pGraphics->AttachControl(new ISVGSwitchControl(irSwitchArea, {irIconOffSVG, irIconOnSVG}, kIRToggle));
    pGraphics->AttachControl(
      new NAMFileBrowserControl(irArea, kMsgTagClearIR, defaultIRString, "wav", loadIRCompletionHandler, style,
                                fileSVG, crossSVG, leftArrowSVG, rightArrowSVG, fileBackgroundBitmap, globeSVG,
                                "More Info", getUrl),
      kCtrlTagIRFileBrowser);

    // IR 2 loader (stacked below IR1)
    auto loadIR2CompletionHandler = [&](const WDL_String& fileName, const WDL_String& path) {
      if (fileName.GetLength())
      {
        mIRPath2 = fileName;
        const dsp::wav::LoadReturnCode retCode = _StageIR2(fileName);
        if (retCode != dsp::wav::LoadReturnCode::SUCCESS)
        {
          std::stringstream message;
          message << "Failed to load IR file " << fileName.Get() << ":\n";
          message << dsp::wav::GetMsgForLoadReturnCode(retCode);

          _ShowMessageBox(GetUI(), message.str().c_str(), "Failed to load IR!", kMB_OK);
        }
      }
    };
    pGraphics->AttachControl(new ISVGSwitchControl(ir2SwitchArea, {irIconOffSVG, irIconOnSVG}, kIRToggle2));
    pGraphics->AttachControl(
      new NAMFileBrowserControl(ir2Area, kMsgTagClearIR2, defaultIRString, "wav", loadIR2CompletionHandler, style,
                                fileSVG, crossSVG, leftArrowSVG, rightArrowSVG, fileBackgroundBitmap, globeSVG,
                                "More Info", getUrl),
      kCtrlTagIRFileBrowser2);

    // IR controls row (level knobs, phase, mute, delay, pan in one row per IR) — after file browsers for click priority
    pGraphics->AttachControl(new NAMKnobControl(irLevelArea, kIRLevel, "IR 1", style, knobBackgroundBitmap));
    pGraphics->AttachControl(new NAMPhaseFlipControl(ir1PhaseArea, kIRPolarity));
    pGraphics->AttachControl(new NAMMuteControl(ir1MuteArea, kIRMute));
    {
      auto* pDelay1 = new NAMDelayControl(ir1DelayCircleArea, kIRDelay);
      pDelay1->HideLabel();
      pGraphics->AttachControl(pDelay1);
    }
    pGraphics->AttachControl(new ITextControl(ir1DelayLabelArea, "Delay", IText(11, PluginColors::NAM_THEMEFONTCOLOR, "Roboto-Regular")));
    pGraphics->AttachControl(new NAMKnobControl(ir1PanColumnArea, kIRPan, "Pan", style, knobBackgroundBitmap));
    pGraphics->AttachControl(new NAMKnobControl(ir2LevelArea, kIRLevel2, "IR 2", style, knobBackgroundBitmap));
    pGraphics->AttachControl(new NAMPhaseFlipControl(ir2PhaseArea, kIRPolarity2));
    pGraphics->AttachControl(new NAMMuteControl(ir2MuteArea, kIRMute2));
    {
      auto* pDelay2 = new NAMDelayControl(ir2DelayCircleArea, kIRDelay2);
      pDelay2->HideLabel();
      pGraphics->AttachControl(pDelay2);
    }
    pGraphics->AttachControl(new ITextControl(ir2DelayLabelArea, "Delay", IText(11, PluginColors::NAM_THEMEFONTCOLOR, "Roboto-Regular")));
    pGraphics->AttachControl(new NAMKnobControl(ir2PanColumnArea, kIRPan2, "Pan", style, knobBackgroundBitmap));
    // The knobs
    pGraphics->AttachControl(new NAMKnobControl(inputKnobArea, kInputLevel, "", knobStyle, knobBackgroundBitmap));
    pGraphics->AttachControl(
      new NAMKnobControl(midKnobArea, kToneMid, "", knobStyle, knobBackgroundBitmap), -1, "EQ_KNOBS");
    pGraphics->AttachControl(new NAMKnobControl(outputKnobArea, kOutputLevel, "", knobStyle, knobBackgroundBitmap));
    pGraphics->AttachControl(new NAMKnobControl(input2KnobArea, kInputLevel2, "", knobStyle, knobBackgroundBitmap));

    // Settings/help/about box
    pGraphics->AttachControl(new NAMCircleButtonControl(
      settingsButtonArea,
      [pGraphics](IControl* pCaller) {
        pGraphics->GetControlWithTag(kCtrlTagSettingsBox)->As<NAMSettingsPageControl>()->HideAnimated(false);
      },
      gearSVG));

    pGraphics
      ->AttachControl(new NAMSettingsPageControl(b, inputLevelBackgroundBitmap, switchHandleBitmap, crossSVG, style,
                                                  radioButtonStyle),
                      kCtrlTagSettingsBox)
      ->Hide(true);

    pGraphics->ForAllControlsFunc([](IControl* pControl) {
      pControl->SetMouseEventsWhenDisabled(true);
      pControl->SetMouseOverWhenDisabled(true);
    });
    // Prevent pan knobs from moving when disabled (mono output)
    pGraphics->ForAllControlsFunc([](IControl* pControl) {
      if (pControl->GetParamIdx() == kIRPan || pControl->GetParamIdx() == kIRPan2)
        pControl->SetMouseEventsWhenDisabled(false);
    });

    // pGraphics->GetControlWithTag(kCtrlTagOutNorm)->SetMouseEventsWhenDisabled(false);
    // pGraphics->GetControlWithTag(kCtrlTagCalibrateInput)->SetMouseEventsWhenDisabled(false);
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
  // Input is collapsed to mono in preparation for the NAM.
  _ProcessInput(inputs, numFrames, numChannelsExternalIn, numChannelsInternal);
  _ApplyDSPStaging();
  const bool noiseGateActive = GetParam(kNoiseGateActive)->Value();

  // Noise gate trigger
  sample** triggerOutput = mInputPointers;
  if (noiseGateActive)
  {
    const double time = 0.01;
    const double threshold = GetParam(kNoiseGateThreshold)->Value(); // GetParam...
    const double ratio = 0.1; // Quadratic...
    const double openTime = 0.005;
    const double holdTime = 0.01;
    const double closeTime = 0.05;
    const dsp::noise_gate::TriggerParams triggerParams(time, threshold, ratio, openTime, holdTime, closeTime);
    mNoiseGateTrigger.SetParams(triggerParams);
    mNoiseGateTrigger.SetSampleRate(sampleRate);
    triggerOutput = mNoiseGateTrigger.Process(mInputPointers, numChannelsInternal, numFrames);
  }

  // Multi-capture preamp blending (weights recomputed from audio thread)
  if (mBlendWeightsDirty)
  {
    _ComputeBlendWeights();
    mBlendWeightsDirty = false;
  }
  if (!mPreampCaptures.empty() && !mBlendWeights.empty())
  {
    std::fill(mBlendTempOutput.begin(), mBlendTempOutput.end(), 0.0);
    for (auto& bw : mBlendWeights)
    {
      auto& capture = mPreampCaptures[bw.captureIndex];
      capture.model->process(triggerOutput, mOutputPointers, nFrames);
      for (size_t s = 0; s < numFrames; s++)
        mBlendTempOutput[s] += bw.weight * mOutputArray[0][s];
    }
    for (size_t s = 0; s < numFrames; s++)
      mOutputArray[0][s] = mBlendTempOutput[s];
  }
  else
  {
    _FallbackDSP(triggerOutput, mOutputPointers, numChannelsInternal, nFrames);
  }

  // Power amp blending (gain-indexed)
  if (mPowerAmpBlendWeightsDirty)
  {
    _ComputePowerAmpBlendWeights();
    mPowerAmpBlendWeightsDirty = false;
  }
  if (!mPowerAmpCaptures.empty() && !mPowerAmpBlendWeights.empty())
  {
    const double gain2 = mInputGain2;
    for (size_t c = 0; c < numChannelsInternal; c++)
      for (size_t s = 0; s < numFrames; s++)
        mInputArray[c][s] = gain2 * mOutputArray[c][s];
    std::fill(mBlendTempOutput.begin(), mBlendTempOutput.end(), 0.0);
    for (auto& bw : mPowerAmpBlendWeights)
    {
      auto& capture = mPowerAmpCaptures[bw.captureIndex];
      capture.model->process(mInputPointers, mOutputPointers, nFrames);
      for (size_t s = 0; s < numFrames; s++)
        mBlendTempOutput[s] += bw.weight * mOutputArray[0][s];
    }
    for (size_t s = 0; s < numFrames; s++)
      mOutputArray[0][s] = mBlendTempOutput[s];
    const double outGain2 = mOutputGain2;
    for (size_t c = 0; c < numChannelsInternal; c++)
      for (size_t s = 0; s < numFrames; s++)
        mOutputArray[c][s] *= outGain2;
  }

  // Apply the noise gate after the NAM(s)
  sample** gateGainOutput =
    noiseGateActive ? mNoiseGateGain.Process(mOutputPointers, numChannelsInternal, numFrames) : mOutputPointers;

  // Set IR convolution mode
  if (mIR) mIR->SetMode(mIRMode);
  if (mIR2) mIR2->SetMode(mIRMode);

  sample** irPointers = gateGainOutput;
  const bool ir1active = mIR != nullptr && GetParam(kIRToggle)->Value();
  const bool ir2active = mIR2 != nullptr && GetParam(kIRToggle2)->Bool();
  std::vector<sample*> irBlendPointers;

  constexpr double kIRMakeupGain = 1.012;

  // Stereo pan: only applies when output is stereo
  const bool useStereo = numChannelsExternalOut > 1 && (ir1active || ir2active);

  auto _ApplyPan = [&](sample** src, size_t nChans, int panParamIdx, double pol, double level, double mute, double blendScale)
  {
    const double pan = GetParam(panParamIdx)->Value();
    const double leftGain = pan <= 0.0 ? 1.0 : 1.0 - pan;
    const double rightGain = pan >= 0.0 ? 1.0 : 1.0 + pan;
    const size_t stereoChans = 2;
    mIRStereoBuffer.resize(stereoChans * numFrames);
    std::fill(mIRStereoBuffer.begin(), mIRStereoBuffer.end(), 0.0);
    for (size_t i = 0; i < numFrames; i++)
    {
      const double s = src[0][i] * pol * level * mute * blendScale * kIRMakeupGain;
      mIRStereoBuffer[i] = s * leftGain;
      mIRStereoBuffer[numFrames + i] = s * rightGain;
    }
    irBlendPointers.resize(stereoChans);
    irBlendPointers[0] = mIRStereoBuffer.data();
    irBlendPointers[1] = mIRStereoBuffer.data() + numFrames;
    return irBlendPointers.data();
  };

  if (ir1active && ir2active)
  {
    sample** ir1Out = mIR->Process(gateGainOutput, numChannelsInternal, numFrames);
    sample** ir2Out = mIR2->Process(gateGainOutput, numChannelsInternal, numFrames);
    const int delay1 = GetParam(kIRDelay)->Int();
    const int delay2 = GetParam(kIRDelay2)->Int();
    const double pol1 = GetParam(kIRPolarity)->Bool() ? -1.0 : 1.0;
    const double pol2 = GetParam(kIRPolarity2)->Bool() ? -1.0 : 1.0;
    const double level1 = GetParam(kIRLevel)->Value();
    const double level2 = GetParam(kIRLevel2)->Value();
    const double mute1 = GetParam(kIRMute)->Bool() ? 0.0 : 1.0;
    const double mute2 = GetParam(kIRMute2)->Bool() ? 0.0 : 1.0;
    const double blendScale = (mute1 > 0.0 && mute2 > 0.0) ? 0.5 : 1.0;
    _ApplyIRDelay(ir1Out, delay1, mIR1DelayBuffer, mIR1DelayWritePos, numChannelsInternal, numFrames);
    _ApplyIRDelay(ir2Out, delay2, mIR2DelayBuffer, mIR2DelayWritePos, numChannelsInternal, numFrames);
    if (useStereo)
    {
      const double pan1 = GetParam(kIRPan)->Value();
      const double pan2 = GetParam(kIRPan2)->Value();
      const double leftGain1 = pan1 <= 0.0 ? 1.0 : 1.0 - pan1;
      const double rightGain1 = pan1 >= 0.0 ? 1.0 : 1.0 + pan1;
      const double leftGain2 = pan2 <= 0.0 ? 1.0 : 1.0 - pan2;
      const double rightGain2 = pan2 >= 0.0 ? 1.0 : 1.0 + pan2;
      const size_t stereoChans = 2;
      mIRStereoBuffer.resize(stereoChans * numFrames);
      std::fill(mIRStereoBuffer.begin(), mIRStereoBuffer.end(), 0.0);
      for (size_t i = 0; i < numFrames; i++)
      {
        const double s1 = ir1Out[0][i] * pol1 * level1 * mute1 * blendScale * kIRMakeupGain;
        const double s2 = ir2Out[0][i] * pol2 * level2 * mute2 * blendScale * kIRMakeupGain;
        mIRStereoBuffer[i] = s1 * leftGain1 + s2 * leftGain2;
        mIRStereoBuffer[numFrames + i] = s1 * rightGain1 + s2 * rightGain2;
      }
      irBlendPointers.resize(stereoChans);
      irBlendPointers[0] = mIRStereoBuffer.data();
      irBlendPointers[1] = mIRStereoBuffer.data() + numFrames;
      irPointers = irBlendPointers.data();
    }
    else
    {
      mIRBlendBuffer.resize(numChannelsInternal * numFrames);
      for (size_t c = 0; c < numChannelsInternal; c++)
        for (size_t i = 0; i < numFrames; i++)
          mIRBlendBuffer[c * numFrames + i] = (ir1Out[c][i] * pol1 * level1 * mute1 + ir2Out[c][i] * pol2 * level2 * mute2) * blendScale * kIRMakeupGain;
      irBlendPointers.resize(numChannelsInternal);
      for (size_t c = 0; c < numChannelsInternal; c++)
        irBlendPointers[c] = &mIRBlendBuffer[c * numFrames];
      irPointers = irBlendPointers.data();
    }
  }
  else if (ir1active)
  {
    irPointers = mIR->Process(gateGainOutput, numChannelsInternal, numFrames);
    const int delay1 = GetParam(kIRDelay)->Int();
    const double pol1 = GetParam(kIRPolarity)->Bool() ? -1.0 : 1.0;
    const double level1 = GetParam(kIRLevel)->Value();
    const double mute1 = GetParam(kIRMute)->Bool() ? 0.0 : 1.0;
    for (size_t c = 0; c < numChannelsInternal; c++)
      for (size_t i = 0; i < numFrames; i++)
        irPointers[c][i] *= pol1 * level1 * mute1 * kIRMakeupGain;
    _ApplyIRDelay(irPointers, delay1, mIR1DelayBuffer, mIR1DelayWritePos, numChannelsInternal, numFrames);
    if (useStereo)
      irPointers = _ApplyPan(irPointers, numChannelsInternal, kIRPan, 1.0, 1.0, 1.0, 1.0);
  }
  else if (ir2active)
  {
    irPointers = mIR2->Process(gateGainOutput, numChannelsInternal, numFrames);
    const int delay2 = GetParam(kIRDelay2)->Int();
    const double pol2 = GetParam(kIRPolarity2)->Bool() ? -1.0 : 1.0;
    const double level2 = GetParam(kIRLevel2)->Value();
    const double mute2 = GetParam(kIRMute2)->Bool() ? 0.0 : 1.0;
    for (size_t c = 0; c < numChannelsInternal; c++)
      for (size_t i = 0; i < numFrames; i++)
        irPointers[c][i] *= pol2 * level2 * mute2 * kIRMakeupGain;
    _ApplyIRDelay(irPointers, delay2, mIR2DelayBuffer, mIR2DelayWritePos, numChannelsInternal, numFrames);
    if (useStereo)
      irPointers = _ApplyPan(irPointers, numChannelsInternal, kIRPan2, 1.0, 1.0, 1.0, 1.0);
  }

  // And the HPF for DC offset (Issue 271)
  const double highPassCutoffFreq = kDCBlockerFrequency;
  const recursive_linear_filter::HighPassParams highPassParams(sampleRate, highPassCutoffFreq);
  mHighPass.SetParams(highPassParams);
  const size_t nIRChannels = useStereo ? 2 : numChannelsInternal;
  sample** hpfPointers = mHighPass.Process(irPointers, nIRChannels, numFrames);

  // restore previous floating point state
  std::feupdateenv(&fe_state);

  // Let's get outta here
  // This is where we exit mono for whatever the output requires.
  _ProcessOutput(hpfPointers, outputs, numFrames, nIRChannels, numChannelsExternalOut);
  // * Output of input leveling (inputs -> mInputPointers),
  // * Output of output leveling (mOutputPointers -> outputs)
  _UpdateMeters(mPreGainPointers, outputs, numFrames, numChannelsInternal, numChannelsExternalOut);
}

void Impulse::OnReset()
{
  const auto sampleRate = GetSampleRate();
  const int maxBlockSize = GetBlockSize();

  // Update OS enum items based on current sample rate
  {
    int maxFactor = ResamplingNAM::MaxOversamplingFactorForRate(sampleRate);
    auto* osParam = GetParam(kOversampling);
    int currentOS = osParam->Int();
    if (maxFactor >= 4)
    {
      int clampedOS = std::min(currentOS, 2);
      osParam->InitEnum("OS", 0, {"Off", "2x", "4x"});
      osParam->Set((double)clampedOS);
    }
    else if (maxFactor >= 2)
    {
      int clampedOS = std::min(currentOS, 1);
      osParam->InitEnum("OS", 0, {"Off", "2x"});
      osParam->Set((double)clampedOS);
    }
    else
    {
      osParam->InitEnum("OS", 0, {"Off"});
      osParam->Set(0.0);
    }
  }

  // Tail is because the HPF DC blocker has a decay.
  // 10 cycles should be enough to pass the VST3 tests checking tail behavior.
  // I'm ignoring the model & IR, but it's not the end of the world.
  const int tailCycles = 10;
  SetTailSize(tailCycles * (int)(sampleRate / kDCBlockerFrequency));
  mInputSender.Reset(sampleRate);
  mOutputSender.Reset(sampleRate);
  // If there is a model or IR loaded, they need to be checked for resampling.
  _ResetModelAndIR(sampleRate, GetBlockSize());

  _UpdateLatency();

#ifndef NO_IGRAPHICS
  // Refresh pan disabled state on reset (handles channel config changes)
  if (auto pGraphics = GetUI())
  {
    const bool stereo = mStereoOutput.load();
    auto* pPan1 = pGraphics->GetControlWithParamIdx(kIRPan);
    auto* pPan2 = pGraphics->GetControlWithParamIdx(kIRPan2);
    if (pPan1)
    {
      pPan1->SetDisabled(!stereo);
      pPan1->Hide(!stereo);
    }
    if (pPan2)
    {
      pPan2->SetDisabled(!stereo);
      pPan2->Hide(!stereo);
    }
    if (!stereo)
    {
      GetParam(kIRPan)->Set(0.0);
      GetParam(kIRPan2)->Set(0.0);
    }
  }
#endif
}
#endif

void Impulse::OnIdle()
{
  mInputSender.TransmitData(*this);
  mOutputSender.TransmitData(*this);

#ifndef NO_IGRAPHICS
  if (mNewCapturesLoaded)
  {
    if (auto* pGraphics = GetUI())
    {
      _UpdateControlsFromModel();
      mNewCapturesLoaded = false;
    }
  }
  if (mNewPowerAmpCapturesLoaded)
  {
    mNewPowerAmpCapturesLoaded = false;
  }
#endif
}

bool Impulse::SerializeState(IByteChunk& chunk) const
{
  WDL_String header("###NeuralAmpModeler###");
  chunk.PutStr(header.Get());
  WDL_String version(PLUG_VERSION_STR);
  chunk.PutStr(version.Get());
  chunk.PutStr(mCaptureDirPath.Get());
  chunk.PutStr(mNAMPath2.Get());
  chunk.PutStr(mIRPath.Get());
  chunk.PutStr(mIRPath2.Get());
  return SerializeParams(chunk);
}

int Impulse::UnserializeState(const IByteChunk& chunk, int startPos)
{
  // Look for the expected header. If it's there, then we'll know what to do.
  WDL_String header;
  int pos = startPos;
  pos = chunk.GetStr(header, pos);

  const char* kExpectedHeader = "###NeuralAmpModeler###";
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

// Auto-load captures from bundle resources
  if (mPreampCaptures.empty())
  {
    WDL_String capturesPath;

    // Method 1: dladdr to find our binary, then locate Resources/captures
    Dl_info info;
    if (dladdr((const void*)&_ImpulseBinaryMarker, &info))
    {
      std::filesystem::path binPath(info.dli_fname);
      // Standard bundle: <Bundle>.app/Contents/MacOS/<binary>
      auto bundleDir = binPath.parent_path().parent_path() / "Resources";
      for (auto& dir : {bundleDir / "captures", binPath.parent_path() / "captures", bundleDir.parent_path() / "captures"})
      {
        if (std::filesystem::exists(dir))
        {
          capturesPath.Set(dir.string().c_str());
          break;
        }
      }
    }

    // Method 2: BundleResourcePath with format-specific bundle ID
    if (!capturesPath.GetLength())
    {
      WDL_String resPath;
      WDL_String bundleID;
#if defined(AU_API)
      bundleID.SetFormatted(100, "%s.%s.audiounit.%s", BUNDLE_DOMAIN, BUNDLE_MFR, BUNDLE_NAME);
#elif defined(VST3_API)
      bundleID.SetFormatted(100, "%s.%s.vst3.%s", BUNDLE_DOMAIN, BUNDLE_MFR, BUNDLE_NAME);
#elif defined(APP_API)
      bundleID.SetFormatted(100, "%s.%s.app.%s", BUNDLE_DOMAIN, BUNDLE_MFR, BUNDLE_NAME);
#elif defined(AAX_API)
      bundleID.SetFormatted(100, "%s.%s.aax.%s", BUNDLE_DOMAIN, BUNDLE_MFR, BUNDLE_NAME);
#else
      bundleID.Set("");
#endif
      BundleResourcePath(resPath, bundleID.Get());
      if (resPath.GetLength())
      {
        resPath.Append("/captures");
        if (std::filesystem::exists(std::filesystem::u8path(resPath.Get())))
          capturesPath.Set(resPath.Get());
      }

    }

    if (capturesPath.GetLength())
    {
      _LoadCapturesFromDirectory(capturesPath);
    }
  }

  if (mIRPath.GetLength())
  {
    SendControlMsgFromDelegate(kCtrlTagIRFileBrowser, kMsgTagLoadedIR, mIRPath.GetLength(), mIRPath.Get());
    if (mIR == nullptr && mStagedIR == nullptr)
      SendControlMsgFromDelegate(kCtrlTagIRFileBrowser, kMsgTagLoadFailed, 0, nullptr);
  }

  if (mIRPath2.GetLength())
  {
    SendControlMsgFromDelegate(kCtrlTagIRFileBrowser2, kMsgTagLoadedIR2, mIRPath2.GetLength(), mIRPath2.Get());
    if (mIR2 == nullptr && mStagedIR2 == nullptr)
      SendControlMsgFromDelegate(kCtrlTagIRFileBrowser2, kMsgTagLoadFailedIR2, 0, nullptr);
  }

#ifndef NO_IGRAPHICS
  if (!mPreampCaptures.empty())
  {
    _UpdateControlsFromModel();
  }
#endif
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
    case kCalibrateInput:
    case kInputCalibrationLevel: _SetInputGain(); break;
    case kInputLevel: mBlendWeightsDirty = true; _SetInputGain(); break;
    case kInputTrim: _SetInputGain(); break;
    case kOutputLevel: _UpdateAutoGainOutputKnob(); _SetInputGain2(); _SetOutputGain(); break;
    case kOutputMode: _SetOutputGain(); break;
    case kCalibrateInput2:
    case kInputCalibrationLevel2:
    case kInputLevel2: mPowerAmpBlendWeightsDirty = true; _UpdateAutoGainOutputKnob(); _SetInputGain2(); _SetOutputGain(); break;
    case kOutputLevel2:
    case kOutputMode2:
      if (GetParam(kAutoGain)->Bool())
        _UpdateAutoGainOutputKnob();
      else
        _SetOutputGain2();
      break;
    case kToneMid: mBlendWeightsDirty = true; break;
    case kBlendNearest: mBlendWeightsDirty = true; mPowerAmpBlendWeightsDirty = true; break;
    case kIRMode:
    {
      mIRMode = GetParam(kIRMode)->Int();
      if (mIR) mIR->SetMode(mIRMode);
      if (mIR2) mIR2->SetMode(mIRMode);
      break;
    }
    case kAutoGain:
      if (GetParam(kAutoGain)->Bool())
      {
        _UpdateAutoGainOutputKnob();
      }
      else
      {
        GetParam(kOutputLevel2)->Set(0.0);
        _SetOutputGain2();
        double norm = (0.0 + 40.0) / 80.0;
        if (auto pGraphics = GetUI())
        {
          auto* ctrl = pGraphics->GetControlWithParamIdx(kOutputLevel2);
          if (ctrl)
            ctrl->SetValueFromDelegate(norm, 0);
        }
      }
      _SetInputGain2(); _SetOutputGain(); break;
    case kIRPan:
    case kIRPan2:
      if (NOutChansConnected() <= 1)
        GetParam(paramIdx)->Set(0.0);
      break;
    case kOversampling:
    {
      int idx = GetParam(kOversampling)->Int();
      int maxFactor = ResamplingNAM::MaxOversamplingFactorForRate(GetSampleRate());
      int factors[3] = {1, 2, 4};
      mOversamplingFactor = std::min(factors[idx], maxFactor);
      _ApplyOversamplingToCaptures();
      break;
    }
    default: break;
  }
}

void Impulse::_UpdateAutoGainOutputKnob()
{
  if (!GetParam(kAutoGain)->Bool())
    return;
  double lvl = GetParam(kOutputLevel)->Value();
  double vol = GetParam(kInputLevel2)->Value();

  // LVL contribution: 0.20 above -35, 2.0 below
  double newOutput = lvl >= -35.0 ? -(lvl * 0.20) : -(-35.0 * 0.20 + (lvl + 35.0) * 2.0);

  // Volume compensation: 1:1 (2.5–5) / 2:1 (below 2.5)
  if (vol < 5.0)
  {
    if (vol >= 2.5)
      newOutput -= (vol - 5.0);
    else
      newOutput -= (vol - 5.0) * 2.0;
  }

  // normOffset: model loudness → target -18dB
  if (!mPowerAmpCaptures.empty() && mPowerAmpCaptures[0].model)
  {
    auto& primary = mPowerAmpCaptures[0].model;
    if (primary->HasLoudness())
    {
      const double loudness = primary->GetLoudness();
      const double targetLoudness = -18.0;
      newOutput += (targetLoudness - loudness);
    }
  }

  newOutput = std::clamp(newOutput, -40.0, 40.0);
  GetParam(kOutputLevel2)->Set(newOutput);
  _SetOutputGain2();
  double norm = (newOutput + 40.0) / 80.0;
  if (auto pGraphics = GetUI())
  {
    auto* ctrl = pGraphics->GetControlWithParamIdx(kOutputLevel2);
    if (ctrl)
    {
      ctrl->SetValueFromDelegate(norm, 0);
    }
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
      auto* pPan = pGraphics->GetControlWithParamIdx(panParam);
      if (pPan)
      {
        pPan->SetDisabled(!toggleOn);
        // Only hide when mono, show when stereo
        pPan->Hide(!toggleOn || !stereo);
      }
      // Snap to 0 in mono
      if (!stereo)
        GetParam(panParam)->Set(0.0);
    };

    // Always refresh pan state (handles mono/stereo changes)
    _SetPanState(kIRToggle, kIRPan);
    _SetPanState(kIRToggle2, kIRPan2);

    switch (paramIdx)
    {
      case kNoiseGateActive: pGraphics->GetControlWithParamIdx(kNoiseGateThreshold)->SetDisabled(!active); break;
      case kIRToggle:
        pGraphics->GetControlWithTag(kCtrlTagIRFileBrowser)->SetDisabled(!active);
        pGraphics->GetControlWithParamIdx(kIRLevel)->SetDisabled(!active);
        break;
      case kIRToggle2:
        pGraphics->GetControlWithTag(kCtrlTagIRFileBrowser2)->SetDisabled(!active);
        pGraphics->GetControlWithParamIdx(kIRLevel2)->SetDisabled(!active);
        break;
      case kOutputLevel:
      case kInputLevel2:
      case kOutputLevel2: _UpdateAutoGainOutputKnob(); break;
      default: break;
    }
  }
#endif
}

bool Impulse::OnMessage(int msgTag, int ctrlTag, int dataSize, const void* pData)
{
  switch (msgTag)
  {
    case kMsgTagClearModel:
      mPreampCaptures.clear();
      mCaptureDirPath.Set("");
      _UpdateLatency();
      return true;
    case kMsgTagClearModel2:
      mPowerAmpCaptures.clear();
      mStagedPowerAmpCaptures.clear();
      mPowerAmpBlendWeights.clear();
      mNAMPath2.Set("");
      _UpdateLatency();
      return true;
    case kMsgTagClearIR: mShouldRemoveIR = true; return true;
    case kMsgTagClearIR2: mShouldRemoveIR2 = true; return true;
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
  // Swap staged captures into place
  if (!mStagedPreampCaptures.empty())
  {
    mPreampCaptures = std::move(mStagedPreampCaptures);
    mStagedPreampCaptures.clear();
    mBlendWeightsDirty = true;
    _UpdateLatency();
    _SetInputGain();
    mNewCapturesLoaded = true;
  }
  if (mShouldRemoveIR)
  {
    mIR = nullptr;
    mIRPath.Set("");
    mShouldRemoveIR = false;
  }
  if (!mStagedPowerAmpCaptures.empty())
  {
    mPowerAmpCaptures = std::move(mStagedPowerAmpCaptures);
    mStagedPowerAmpCaptures.clear();
    mPowerAmpBlendWeightsDirty = true;
    _UpdateLatency();
    _SetInputGain2();
    _UpdateAutoGainOutputKnob();
    if (!GetParam(kAutoGain)->Bool())
      _SetOutputGain2();
    mNewPowerAmpCapturesLoaded = true;
  }
  if (mStagedIR != nullptr)
  {
    mIR = std::move(mStagedIR);
    mStagedIR = nullptr;
  }
  if (mShouldRemoveIR2)
  {
    mIR2 = nullptr;
    mIRPath2.Set("");
    mShouldRemoveIR2 = false;
  }
  if (mStagedIR2 != nullptr)
  {
    mIR2 = std::move(mStagedIR2);
    mStagedIR2 = nullptr;
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

void Impulse::_FallbackDSP(iplug::sample** inputs, iplug::sample** outputs, const size_t numChannels,
                                    const size_t numFrames)
{
  for (auto c = 0; c < numChannels; c++)
    for (auto s = 0; s < numFrames; s++)
      mOutputArray[c][s] = mInputArray[c][s];
}

void Impulse::_ResetModelAndIR(const double sampleRate, const int maxBlockSize)
{
  // Preamp captures
  auto resetCaptures = [&](std::vector<NAMCapture>& captures) {
    for (auto& c : captures)
    {
      if (c.model)
        c.model->Reset(sampleRate, maxBlockSize);
    }
  };
  resetCaptures(mPreampCaptures);
  resetCaptures(mStagedPreampCaptures);
  resetCaptures(mPowerAmpCaptures);
  resetCaptures(mStagedPowerAmpCaptures);

  // IR
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
  resetIR(mStagedIR, mIR);
  resetIR(mStagedIR2, mIR2);
}

void Impulse::_SetInputGain()
{
  double inputGainDB = 0.0;
  if (!mPreampCaptures.empty())
  {
    auto& capture = mPreampCaptures[0];
    if (capture.model && capture.model->HasInputLevel() && GetParam(kCalibrateInput)->Bool())
    {
      inputGainDB = GetParam(kInputCalibrationLevel)->Value() - capture.model->GetInputLevel();
    }
  }
  // Input trim (after calibration, before Gain)
  inputGainDB += GetParam(kInputTrim)->Value();
  // Taper below lowest capture gain (1.5): 0→-18dB, 1.5→0dB
  double gain = GetParam(kInputLevel)->Value();
  if (gain < 1.5)
    inputGainDB += (gain - 1.5) * 12.0;
  mInputGain = DBToAmp(inputGainDB);
}

void Impulse::_SetOutputGain()
{
  // LVL is in mInputGain2 (between NAM1 and NAM2), not here.
  // Auto-gain compensation is reflected on the Output knob value, not hidden here.
  mOutputGain = DBToAmp(0.0);
}

void Impulse::_SetOutputGain2()
{
  if (GetParam(kAutoGain)->Bool())
  {
    mOutputGain2 = DBToAmp(GetParam(kOutputLevel2)->Value());
    return;
  }

  double gainDB = GetParam(kOutputLevel2)->Value();
  if (!mPowerAmpCaptures.empty() && mPowerAmpCaptures[0].model)
  {
    auto& primary = mPowerAmpCaptures[0].model;
    if (primary->HasLoudness())
    {
      const double loudness = primary->GetLoudness();
      const double targetLoudness = -18.0;
      gainDB += (targetLoudness - loudness);
    }
  }
  mOutputGain2 = DBToAmp(gainDB);
}

void Impulse::_SetInputGain2()
{
  // LVL knob controls interstage gain: 0 = unity, -20 = max cut
  double gainDB = GetParam(kOutputLevel)->Value();
  // Taper below lowest power amp capture (1.5): 0→-18dB, 1.5→0dB
  double vol = GetParam(kInputLevel2)->Value();
  if (vol < 1.5)
    gainDB += (vol - 1.5) * 12.0;
  if (!mPowerAmpCaptures.empty() && mPowerAmpCaptures[0].model
      && mPowerAmpCaptures[0].model->HasInputLevel() && GetParam(kCalibrateInput2)->Bool())
  {
    gainDB += GetParam(kInputCalibrationLevel2)->Value() - mPowerAmpCaptures[0].model->GetInputLevel();
  }
  mInputGain2 = DBToAmp(gainDB);
}


std::string Impulse::_StageModel2(const WDL_String& modelPath)
{
  WDL_String previousNAMPath2 = mNAMPath2;
  try
  {
    auto dspPath = std::filesystem::u8path(modelPath.Get());
    std::unique_ptr<nam::DSP> model = nam::get_dsp(dspPath);

    if (model->NumInputChannels() != 1 && model->NumInputChannels() != 2)
    {
      throw std::runtime_error("Model must have 1 or 2 input channels, but has " + std::to_string(model->NumInputChannels()));
    }
    if (model->NumOutputChannels() != 1)
    {
      throw std::runtime_error("Model must have 1 output channel, but has "
                                + std::to_string(model->NumOutputChannels()));
    }

    std::unique_ptr<ResamplingNAM> temp = std::make_unique<ResamplingNAM>(std::move(model), GetSampleRate());
    temp->Reset(GetSampleRate(), GetBlockSize());
    NAMCapture cap;
    cap.tone = -1.0;
    cap.gain = -1.0;
    cap.path = modelPath.Get();
    cap.model = std::move(temp);
    mStagedPowerAmpCaptures.clear();
    mStagedPowerAmpCaptures.push_back(std::move(cap));
    mNAMPath2 = modelPath;
  }
  catch (std::runtime_error& e)
  {
    mStagedPowerAmpCaptures.clear();
    mNAMPath2 = previousNAMPath2;
    std::cerr << "Failed to read DSP module" << std::endl;
    std::cerr << e.what() << std::endl;
    return e.what();
  }
  return "";
}

dsp::wav::LoadReturnCode Impulse::_StageIR(const WDL_String& irPath)
{
  // FIXME it'd be better for the path to be "staged" as well. Just in case the
  // path and the model got caught on opposite sides of the fence...
  WDL_String previousIRPath = mIRPath;
  const double sampleRate = GetSampleRate();
  dsp::wav::LoadReturnCode wavState = dsp::wav::LoadReturnCode::ERROR_OTHER;
  try
  {
    auto irPathU8 = std::filesystem::u8path(irPath.Get());
    mStagedIR = std::make_unique<dsp::ImpulseResponse>(irPathU8.string().c_str(), sampleRate);
    wavState = mStagedIR->GetWavState();
  }
  catch (std::runtime_error& e)
  {
    wavState = dsp::wav::LoadReturnCode::ERROR_OTHER;
    std::cerr << "Caught unhandled exception while attempting to load IR:" << std::endl;
    std::cerr << e.what() << std::endl;
  }

  if (wavState == dsp::wav::LoadReturnCode::SUCCESS)
  {
    mIRPath = irPath;
    SendControlMsgFromDelegate(kCtrlTagIRFileBrowser, kMsgTagLoadedIR, mIRPath.GetLength(), mIRPath.Get());
  }
  else
  {
    if (mStagedIR != nullptr)
      mStagedIR = nullptr;
    mIRPath = previousIRPath;
    SendControlMsgFromDelegate(kCtrlTagIRFileBrowser, kMsgTagLoadFailed, 0, nullptr);
  }

  return wavState;
}

dsp::wav::LoadReturnCode Impulse::_StageIR2(const WDL_String& irPath)
{
  WDL_String previousIRPath = mIRPath2;
  const double sampleRate = GetSampleRate();
  dsp::wav::LoadReturnCode wavState = dsp::wav::LoadReturnCode::ERROR_OTHER;
  try
  {
    auto irPathU8 = std::filesystem::u8path(irPath.Get());
    mStagedIR2 = std::make_unique<dsp::ImpulseResponse>(irPathU8.string().c_str(), sampleRate);
    wavState = mStagedIR2->GetWavState();
  }
  catch (std::runtime_error& e)
  {
    wavState = dsp::wav::LoadReturnCode::ERROR_OTHER;
    std::cerr << "Caught unhandled exception while attempting to load IR:" << std::endl;
    std::cerr << e.what() << std::endl;
  }

  if (wavState == dsp::wav::LoadReturnCode::SUCCESS)
  {
    mIRPath2 = irPath;
    SendControlMsgFromDelegate(kCtrlTagIRFileBrowser2, kMsgTagLoadedIR2, mIRPath2.GetLength(), mIRPath2.Get());
  }
  else
  {
    if (mStagedIR2 != nullptr)
      mStagedIR2 = nullptr;
    mIRPath2 = previousIRPath;
    SendControlMsgFromDelegate(kCtrlTagIRFileBrowser2, kMsgTagLoadFailedIR2, 0, nullptr);
  }

  return wavState;
}

size_t Impulse::_GetBufferNumChannels() const
{
  // Assumes input=output (no mono->stereo effects)
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
  //  if (!updateChannels && !updateFrames)  // Could we do this?
  //    return;

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
    mBlendTempOutput.resize(numFrames);
    std::fill(mBlendTempOutput.begin(), mBlendTempOutput.end(), 0.0);
  }
  // Would these ever get changed by something?
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
  // We'll assume that the main processing is mono for now. We'll handle dual amps later.
  if (nChansOut != 1)
  {
    std::stringstream ss;
    ss << "Expected mono output, but " << nChansOut << " output channels are requested!";
    throw std::runtime_error(ss.str());
  }

  // On the standalone, we can probably assume that the user has plugged into only one input and they expect it to be
  // carried straight through. Don't apply any division over nChansIn because we're just "catching anything out there."
  // However, in a DAW, it's probably something providing stereo, and we want to take the average in order to avoid
  // doubling the loudness. (This would change w/ double mono processing)
  double gain = mInputGain;
#ifndef APP_API
  gain /= (float)nChansIn;
#endif
  // Assume _PrepareBuffers() was already called

  for (size_t c = 0; c < nChansIn; c++)
    for (size_t s = 0; s < nFrames; s++)
      if (c == 0)
        mInputArray[0][s] = gain * inputs[c][s];
      else
        mInputArray[0][s] += gain * inputs[c][s];

  // Capture the Gain-affected signal for the input meter (before model 2's Volume knob overwrites mInputArray)
  for (size_t s = 0; s < nFrames; s++)
    mPreGainBuffer[s] = mInputArray[0][s];
}

void Impulse::_ProcessOutput(iplug::sample** inputs, iplug::sample** outputs, const size_t nFrames,
                                      const size_t nChansIn, const size_t nChansOut)
{
  const double gain = mOutputGain;
  // Assume _PrepareBuffers() was already called
  if (nChansIn == 1)
  {
    // Broadcast the internal mono stream to all output channels.
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
      // Sum stereo to mono
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
      // Pass stereo through
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

void Impulse::_UpdateControlsFromModel()
{
#ifndef NO_IGRAPHICS
  if (mPreampCaptures.empty())
  {
    return;
  }
  if (auto* pGraphics = GetUI())
  {
    const bool disableInputCalibrationControls = !mPreampCaptures.empty() && mPreampCaptures[0].model
      && !mPreampCaptures[0].model->HasInputLevel();
    pGraphics->GetControlWithTag(kCtrlTagCalibrateInput)->SetDisabled(disableInputCalibrationControls);
    pGraphics->GetControlWithTag(kCtrlTagInputCalibrationLevel)->SetDisabled(disableInputCalibrationControls);
  }
#endif
}

void Impulse::_UpdateLatency()
{
  int latency = 0;
  for (auto& c : mPreampCaptures)
  {
    if (c.model)
      latency = std::max(latency, c.model->GetLatency());
  }
  for (auto& c : mPowerAmpCaptures)
  {
    if (c.model)
      latency += c.model->GetLatency();
  }

  if (GetLatency() != latency)
  {
    SetLatency(latency);
  }
}

void Impulse::_ApplyOversamplingToCaptures()
{
  for (auto& c : mPreampCaptures)
  {
    if (c.model)
      c.model->SetOversamplingFactor(mOversamplingFactor, GetSampleRate());
  }
  for (auto& c : mPowerAmpCaptures)
  {
    if (c.model)
      c.model->SetOversamplingFactor(mOversamplingFactor, GetSampleRate());
  }
  _UpdateLatency();
}

void Impulse::_ApplyIRDelay(sample** signal, int delay, std::vector<double>& buf, size_t& writePos, size_t nChans, size_t nFrames)
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

void Impulse::_UpdateMeters(sample** inputPointer, sample** outputPointer, const size_t nFrames,
                                     const size_t nChansIn, const size_t nChansOut)
{
  // Right now, we didn't specify MAXNC when we initialized these, so it's 1.
  const int nChansHack = 1;
  mInputSender.ProcessBlock(inputPointer, (int)nFrames, kCtrlTagInputMeter, nChansHack);
  mOutputSender.ProcessBlock(outputPointer, (int)nFrames, kCtrlTagOutputMeter, nChansHack);
}

std::string Impulse::_LoadCapturesFromDirectory(const WDL_String& dirPath)
{
  auto dirPathStr = std::filesystem::u8path(dirPath.Get());
  if (!std::filesystem::exists(dirPathStr) || !std::filesystem::is_directory(dirPathStr))
    return "Directory does not exist: " + std::string(dirPath.Get());

  std::vector<NAMCapture> newPreamp;
  std::vector<NAMCapture> newPowerAmp;
  std::vector<std::string> errors;

  for (auto& entry : std::filesystem::directory_iterator(dirPathStr))
  {
    if (!entry.is_regular_file())
      continue;
    auto path = entry.path();
    if (path.extension() != ".nam")
      continue;

    std::string filename = path.stem().string();
    std::unique_ptr<nam::DSP> model;
    try
    {
      model = nam::get_dsp(path);
    }
    catch (std::exception& e)
    {
      errors.push_back("Failed to load " + filename + ": " + e.what());
      continue;
    }

    if ((model->NumInputChannels() != 1 && model->NumInputChannels() != 2) || model->NumOutputChannels() != 1)
    {
      errors.push_back("Skipping " + filename + ": must have 1-2 input channels and 1 output channel");
      continue;
    }

    auto resampled = std::make_unique<ResamplingNAM>(std::move(model), GetSampleRate());
    resampled->Reset(GetSampleRate(), GetBlockSize());
    if (mOversamplingFactor > 1)
      resampled->SetOversamplingFactor(mOversamplingFactor, GetSampleRate());

    // Parse filename to determine type and gain position
    double gainVal = -1.0;
    double toneVal = -1.0;

    if (filename.find("POWER") != std::string::npos || filename.find("Power") != std::string::npos)
    {
      // Parse gain from "POWER <gain>" or "Power Amp <gain>"
      size_t powPos = filename.find("POWER");
      if (powPos == std::string::npos)
        powPos = filename.find("Power");
      if (powPos != std::string::npos)
      {
        std::string after = filename.substr(powPos + 5);
        after.erase(0, after.find_first_not_of(" "));
        // Take the first token (e.g. "1.5" from "1.5_in_17.8_out_-15.4")
        auto underscorePos = after.find('_');
        std::string gainStr = (underscorePos != std::string::npos) ? after.substr(0, underscorePos) : after;
        gainStr.erase(gainStr.find_last_not_of(" ") + 1);
        try { gainVal = std::stod(gainStr); } catch (...) {}
      }
      NAMCapture cap;
      cap.tone = -1.0;
      cap.gain = gainVal;
      cap.path = path.string();
      cap.model = std::move(resampled);
      newPowerAmp.push_back(std::move(cap));
      continue;
    }

    // Parse "Impulse Pre <gain> Tone <tone>" format
    auto prePos = filename.find("Pre");
    auto tonePos = filename.find("Tone");
    if (prePos != std::string::npos && tonePos != std::string::npos)
    {
      try
      {
        std::string gainStr = filename.substr(prePos + 3, tonePos - prePos - 3);
        gainStr.erase(gainStr.find_last_not_of(" ") + 1);
        gainStr.erase(0, gainStr.find_first_not_of(" "));
        gainVal = std::stod(gainStr);

        std::string toneStr = filename.substr(tonePos + 4);
        toneStr.erase(toneStr.find_last_not_of(" ") + 1);
        toneStr.erase(0, toneStr.find_first_not_of(" "));
        // Strip calibration suffix if present
        auto calPos = toneStr.find("_in_");
        if (calPos != std::string::npos)
          toneStr = toneStr.substr(0, calPos);
        toneStr.erase(toneStr.find_last_not_of(" ") + 1);
        toneVal = std::stod(toneStr);
      }
      catch (...)
      {
        errors.push_back("Could not parse tone/gain from " + filename);
        continue;
      }
    }

    NAMCapture cap;
    cap.tone = toneVal;
    cap.gain = gainVal;
    cap.path = path.string();
    cap.model = std::move(resampled);
    newPreamp.push_back(std::move(cap));
  }

  // Prune near-duplicate captures
  {
    constexpr double kPruneEps = 1e-4;
    auto cmp = [](const NAMCapture& a, const NAMCapture& b) {
      if (a.tone != b.tone) return a.tone < b.tone;
      return a.gain < b.gain;
    };
    auto eq = [kPruneEps](const NAMCapture& a, const NAMCapture& b) {
      return std::abs(a.tone - b.tone) <= kPruneEps && std::abs(a.gain - b.gain) <= kPruneEps;
    };
    std::sort(newPreamp.begin(), newPreamp.end(), cmp);
    auto last = std::unique(newPreamp.begin(), newPreamp.end(), eq);
    newPreamp.erase(last, newPreamp.end());
  }

  if (newPreamp.empty())
    return "No preamp captures found in directory.";

  mStagedPreampCaptures = std::move(newPreamp);
  if (!newPowerAmp.empty())
  {
    mStagedPowerAmpCaptures = std::move(newPowerAmp);
    mNAMPath2.Set(mStagedPowerAmpCaptures[0].path.c_str());
  }
  mCaptureDirPath = dirPath;

  return "";
}

void Impulse::_ComputeBlendWeights()
{
  mBlendWeights.clear();
  if (mPreampCaptures.empty())
    return;

  const double tone = GetParam(kToneMid)->Value();
  const double gain = GetParam(kInputLevel)->Value();
  constexpr double kExactCaptureEps = 1e-4;

  struct Dist {
    size_t index;
    double distSq;
  };
  std::vector<Dist> distances;
  distances.reserve(mPreampCaptures.size());
  bool haveExactCapture = false;
  size_t exactCaptureIndex = 0;

  for (size_t i = 0; i < mPreampCaptures.size(); i++)
  {
    auto& c = mPreampCaptures[i];
    double dt = c.tone >= 0.0 ? (tone - c.tone) : 0.0;
    double dg = c.gain >= 0.0 ? (gain - c.gain) : 0.0;
    if (!haveExactCapture && c.tone >= 0.0 && c.gain >= 0.0
        && std::abs(dt) <= kExactCaptureEps && std::abs(dg) <= kExactCaptureEps)
    {
      haveExactCapture = true;
      exactCaptureIndex = i;
    }
    distances.push_back({i, dt * dt + dg * dg});
  }

  std::sort(distances.begin(), distances.end(),
            [](auto& a, auto& b) { return a.distSq < b.distSq; });
  if (haveExactCapture)
  {
    mBlendWeights.push_back({exactCaptureIndex, 1.0});
    return;
  }

  const size_t kMaxCaptures = kBlendValues[GetParam(kBlendNearest)->Int()];

  size_t n = std::min(kMaxCaptures, distances.size());

  double totalWeight = 0.0;
  for (size_t j = 0; j < n; j++)
  {
    double w = 1.0 / (distances[j].distSq + 0.0001);
    mBlendWeights.push_back({distances[j].index, w});
    totalWeight += w;
  }

  for (auto& bw : mBlendWeights)
    bw.weight /= totalWeight;

}

void Impulse::_ComputePowerAmpBlendWeights()
{
  mPowerAmpBlendWeights.clear();
  if (mPowerAmpCaptures.empty())
    return;

  // Volume knob is 0-10, directly matching power amp capture gains
  const double gain = GetParam(kInputLevel2)->Value();
  constexpr double kExactCaptureEps = 1e-4;

  struct Dist {
    size_t index;
    double distSq;
  };
  std::vector<Dist> distances;
  distances.reserve(mPowerAmpCaptures.size());
  bool haveExactCapture = false;
  size_t exactCaptureIndex = 0;

  for (size_t i = 0; i < mPowerAmpCaptures.size(); i++)
  {
    auto& c = mPowerAmpCaptures[i];
    double dg = c.gain >= 0.0 ? (gain - c.gain) : 0.0;
    if (!haveExactCapture && c.gain >= 0.0 && std::abs(dg) <= kExactCaptureEps)
    {
      haveExactCapture = true;
      exactCaptureIndex = i;
    }
    distances.push_back({i, dg * dg});
  }

  std::sort(distances.begin(), distances.end(),
            [](auto& a, auto& b) { return a.distSq < b.distSq; });
  if (haveExactCapture)
  {
    mPowerAmpBlendWeights.push_back({exactCaptureIndex, 1.0});
    return;
  }

  const size_t kMaxCaptures = kBlendValues[GetParam(kBlendNearest)->Int()];

  size_t n = std::min(kMaxCaptures, distances.size());

  double totalWeight = 0.0;
  for (size_t j = 0; j < n; j++)
  {
    double w = 1.0 / (distances[j].distSq + 0.0001);
    mPowerAmpBlendWeights.push_back({distances[j].index, w});
    totalWeight += w;
  }

  for (auto& bw : mPowerAmpBlendWeights)
    bw.weight /= totalWeight;
}

// HACK
#include "Unserialization.cpp"
