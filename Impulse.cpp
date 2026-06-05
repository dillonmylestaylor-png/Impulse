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
  GetParam(kIRMode)->InitEnum("IR Mode", 0, {"Zero Latency", "Normal"});

  // IR 0
  GetParam(kIRToggle)->InitBool("IR 1", true);
  GetParam(kIRPolarity)->InitBool("Phase", false);
  GetParam(kIRLevel)->InitDouble("Level", 1.0, 0.0, 1.0, 0.01, "");
  GetParam(kIRMute)->InitBool("Mute", false);
  GetParam(kIRDelay)->InitInt("Delay", 0, 0, 99, "%i samples");
  GetParam(kIRPan)->InitDouble("Pan", 0.0, -1.0, 1.0, 0.01, "");

  // IR 1
  GetParam(kIRToggle2)->InitBool("IR 2", true);
  GetParam(kIRPolarity2)->InitBool("Phase", false);
  GetParam(kIRLevel2)->InitDouble("Level", 1.0, 0.0, 1.0, 0.01, "");
  GetParam(kIRMute2)->InitBool("Mute", false);
  GetParam(kIRDelay2)->InitInt("Delay", 0, 0, 99, "%i samples");
  GetParam(kIRPan2)->InitDouble("Pan", 0.0, -1.0, 1.0, 0.01, "");

  // IR 2
  GetParam(kIRToggle3)->InitBool("IR 3", true);
  GetParam(kIRPolarity3)->InitBool("Phase", false);
  GetParam(kIRLevel3)->InitDouble("Level", 1.0, 0.0, 1.0, 0.01, "");
  GetParam(kIRMute3)->InitBool("Mute", false);
  GetParam(kIRDelay3)->InitInt("Delay", 0, 0, 99, "%i samples");
  GetParam(kIRPan3)->InitDouble("Pan", 0.0, -1.0, 1.0, 0.01, "");

  // IR 3
  GetParam(kIRToggle4)->InitBool("IR 4", true);
  GetParam(kIRPolarity4)->InitBool("Phase", false);
  GetParam(kIRLevel4)->InitDouble("Level", 1.0, 0.0, 1.0, 0.01, "");
  GetParam(kIRMute4)->InitBool("Mute", false);
  GetParam(kIRDelay4)->InitInt("Delay", 0, 0, 99, "%i samples");
  GetParam(kIRPan4)->InitDouble("Pan", 0.0, -1.0, 1.0, 0.01, "");

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

    const auto backgroundBitmap = pGraphics->LoadBitmap(BACKGROUND_FN);
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

    // IN label + meter
    const float inLabelW = 22.0f;
    const auto inLabelArea = IRECT(topBar.L + topBarPad, topBar.T,
                                   topBar.L + topBarPad + inLabelW, topBar.B)
                               .GetCentredInside(inLabelW, 22);
    const auto inMeterArea = IRECT(inLabelArea.R + 4, topBar.T,
                                   inLabelArea.R + 4 + meterW, topBar.B)
                               .GetCentredInside(meterW, meterH);

    // Input trim knob
    const float inTrimW = 64.0f;
    const float labelH = 12.0f;
    const auto inTrimArea = IRECT(inMeterArea.R + 4, topBar.T + 2,
                                  inMeterArea.R + 4 + inTrimW, topBar.B - 2);
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

    // IR Mode switch (small, between logo and OUT)
    const auto irModeArea = IRECT(logoArea.R + 10, topBar.T + 2, outSection.L - 10, topBar.B - 2);
    const auto irModeLabelArea = irModeArea.GetFromTop(labelH).GetCentredInside(irModeArea.W(), labelH);
    const auto irModeSwitchArea = IRECT(irModeArea.L, irModeLabelArea.B + 2,
                                        irModeArea.R, irModeLabelArea.B + 2 + 30);

    // ===== MAIN CONTENT AREA: IR slots =====
    const auto mainContentArea = contentArea.GetReducedFromTop(topBarHeight);
    const auto irSection = mainContentArea;

    // IR section layout: 4 rows stacked vertically
    const auto irRowHeight = irSection.H() / 4.0f;
    const auto btnSz = 26.0f;
    const auto panKnobSz = 52.0f;
    const auto irLabelH = 11.0f;
    const auto irLabelGap = 2.0f;
    const auto fileHeight = 30.0f;

    const char* const defaultIRString = "Select IR...";
    const char* const getUrl = "https://www.tonkraf.com";

    // Draw background
    pGraphics->AttachControl(new NAMBackgroundControl(b, PluginColors::NAM_BG_BLUE));
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
    pGraphics->AttachControl(new ITextControl(irModeLabelArea, "IR Mode", IText(11, PluginColors::NAM_THEMEFONTCOLOR, "Michroma-Regular")));
    {
      auto* irModeSwitch = new NAMSwitchControl(irModeSwitchArea, kIRMode, "", style, switchHandleBitmap);
      irModeSwitch->SetTooltip("IR convolution mode: Zero Latency vs Normal (FFT)");
      pGraphics->AttachControl(irModeSwitch);
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
      const auto ctrls = controlsRow.GetFromRight(controlsRow.W() - 100.0f);
      const auto ctrlW = ctrls.W() / 5.0f;
      const auto phaseArea = IRECT(ctrls.L, ctrls.T, ctrls.L + ctrlW, ctrls.B).GetCentredInside(btnSz, btnSz);
      const auto muteArea = IRECT(ctrls.L + ctrlW, ctrls.T, ctrls.L + ctrlW * 2.0f, ctrls.B).GetCentredInside(btnSz, btnSz);
      const auto delayCircleArea = IRECT(ctrls.L + ctrlW * 2.0f, ctrls.T, ctrls.L + ctrlW * 3.0f, ctrls.B).GetCentredInside(btnSz, btnSz);
      const auto delayLabelArea = IRECT(ctrls.L + ctrlW * 2.0f, delayCircleArea.B + irLabelGap, ctrls.L + ctrlW * 3.0f, delayCircleArea.B + irLabelGap + irLabelH);
      const auto panArea = IRECT(ctrls.L + ctrlW * 3.0f, ctrls.T, ctrls.L + ctrlW * 4.0f, ctrls.B);
      const auto labelArea = IRECT(ctrls.L + ctrlW * 4.0f, ctrls.T, ctrls.R, ctrls.B).GetCentredInside(40.0f, 20.0f);

      const int toggleParam = kIRToggle + i * 6;
      const int phaseParam = kIRPolarity + i * 6;
      const int levelParam = kIRLevel + i * 6;
      const int muteParam = kIRMute + i * 6;
      const int delayParam = kIRDelay + i * 6;
      const int panParam = kIRPan + i * 6;
      const int browserTag = kCtrlTagIRFileBrowser + i;
      const int clearMsg = kMsgTagClearIR + i;

      // IR label
      char irLabel[8];
      snprintf(irLabel, sizeof(irLabel), "IR %d", i + 1);
      pGraphics->AttachControl(new ITextControl(labelArea, irLabel, IText(14, PluginColors::NAM_THEMEFONTCOLOR, "Michroma-Regular")));

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
      pGraphics->AttachControl(new NAMKnobControl(levelArea, levelParam, irLabel, style, knobBackgroundBitmap));

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
      pGraphics->AttachControl(new ITextControl(delayLabelArea, "Delay", IText(11, PluginColors::NAM_THEMEFONTCOLOR, "Roboto-Regular")));

      // Pan
      pGraphics->AttachControl(new NAMKnobControl(panArea, panParam, "Pan", style, knobBackgroundBitmap));
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
  for (int i = 0; i < kNumIRs; i++)
  {
    const int toggleParam = kIRToggle + i * 6;
    const int muteParam = kIRMute + i * 6;
    if (mIRSlots[i].ir && GetParam(toggleParam)->Value() && !GetParam(muteParam)->Bool())
      activeCount++;
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

      for (size_t s = 0; s < numFrames; s++)
      {
        const double s_val = irOut[0][s] * pol * level * mute * blendScale * kIRMakeupGain;
        mIRStereoBuffer[s] += s_val * leftGain;
        mIRStereoBuffer[numFrames + s] += s_val * rightGain;
      }
    }

    // If no IRs active, pass through
    if (activeCount == 0)
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

    // HPF
    const double highPassCutoffFreq = kDCBlockerFrequency;
    const recursive_linear_filter::HighPassParams highPassParams(sampleRate, highPassCutoffFreq);
    mHighPass.SetParams(highPassParams);
    sample** hpfPointers = mHighPass.Process(stereoPtrs, stereoChans, numFrames);

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

      for (size_t c = 0; c < numChannelsInternal; c++)
        for (size_t s = 0; s < numFrames; s++)
          mIRBlendBuffer[c * numFrames + s] += irOut[c][s] * pol * level * mute * blendScale * kIRMakeupGain;
    }

    // If no IRs active, pass through
    if (activeCount == 0)
    {
      for (size_t c = 0; c < numChannelsInternal; c++)
        for (size_t s = 0; s < numFrames; s++)
          mIRBlendBuffer[c * numFrames + s] = mInputPointers[c][s];
    }

    sample** monoPtrs = new sample*[numChannelsInternal];
    for (size_t c = 0; c < numChannelsInternal; c++)
      monoPtrs[c] = &mIRBlendBuffer[c * numFrames];

    // HPF
    const double highPassCutoffFreq = kDCBlockerFrequency;
    const recursive_linear_filter::HighPassParams highPassParams(sampleRate, highPassCutoffFreq);
    mHighPass.SetParams(highPassParams);
    sample** hpfPointers = mHighPass.Process(monoPtrs, numChannelsInternal, numFrames);

    std::feupdateenv(&fe_state);
    _ProcessOutput(hpfPointers, outputs, numFrames, numChannelsInternal, numChannelsExternalOut);
    delete[] monoPtrs;
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
      auto* pPan = pGraphics->GetControlWithParamIdx(kIRPan + i * 6);
      if (pPan)
      {
        pPan->SetDisabled(!stereo);
        pPan->Hide(!stereo);
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
      auto* pPan = pGraphics->GetControlWithParamIdx(panParam);
      if (pPan)
      {
        pPan->SetDisabled(!toggleOn);
        pPan->Hide(!toggleOn || !stereo);
      }
      if (!stereo)
        GetParam(panParam)->Set(0.0);
    };

    for (int i = 0; i < kNumIRs; i++)
      _SetPanState(kIRToggle + i * 6, kIRPan + i * 6);

    switch (paramIdx)
    {
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
      mIRSlots[i].ir = std::move(mIRSlots[i].stagedIR);
      mIRSlots[i].stagedIR = nullptr;
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
