#pragma once

#include <cmath>
#include <string>
#include <vector>

#include "AudioDSPTools/dsp/ImpulseResponse.h"
#include "AudioDSPTools/dsp/RecursiveLinearFilter.h"
#include "AudioDSPTools/dsp/dsp.h"
#include "AudioDSPTools/dsp/wav.h"

#include "Colors.h"

#include "IPlug_include_in_plug_hdr.h"
#include "ISender.h"

#include "json.hpp"

const int kNumPresets = 1;
constexpr size_t kNumChannelsInternal = 1;
constexpr int kNumIRs = 4;

class NAMSender : public iplug::IPeakAvgSender<>
{
public:
  NAMSender()
  : iplug::IPeakAvgSender<>(-90.0, true, 5.0f, 1.0f, 300.0f, 500.0f)
  {
  }
};

enum EParams
{
  kInputLevel = 0,
  kOutputLevel,
  kInputTrim,
  kIRMode,
  // IR 0
  kIRToggle,
  kIRPolarity,
  kIRLevel,
  kIRMute,
  kIRDelay,
  kIRPan,
  // IR 1
  kIRToggle2,
  kIRPolarity2,
  kIRLevel2,
  kIRMute2,
  kIRDelay2,
  kIRPan2,
  // IR 2
  kIRToggle3,
  kIRPolarity3,
  kIRLevel3,
  kIRMute3,
  kIRDelay3,
  kIRPan3,
  // IR 3
  kIRToggle4,
  kIRPolarity4,
  kIRLevel4,
  kIRMute4,
  kIRDelay4,
  kIRPan4,
  // Per-IR HPF/LPF (6 params per IR)
  kIRHPFreq,
  kIRHPFSlope,
  kIRHPFBypass,
  kIRLPFreq,
  kIRLPFSlope,
  kIRLPFBypass,
  kIRHPFreq2,
  kIRHPFSlope2,
  kIRHPFBypass2,
  kIRLPFreq2,
  kIRLPFSlope2,
  kIRLPFBypass2,
  kIRHPFreq3,
  kIRHPFSlope3,
  kIRHPFBypass3,
  kIRLPFreq3,
  kIRLPFSlope3,
  kIRLPFBypass3,
  kIRHPFreq4,
  kIRHPFSlope4,
  kIRHPFBypass4,
  kIRLPFreq4,
  kIRLPFSlope4,
  kIRLPFBypass4,
  kNumParams
};

enum ECtrlTags
{
  kCtrlTagIRFileBrowser = 0,
  kCtrlTagIRFileBrowser2,
  kCtrlTagIRFileBrowser3,
  kCtrlTagIRFileBrowser4,
  kCtrlTagPanLabel,
  kCtrlTagPanLabel2,
  kCtrlTagPanLabel3,
  kCtrlTagPanLabel4,
  kCtrlTagInputMeter,
  kCtrlTagOutputMeter,
  kCtrlTagSettingsBox,
  kNumCtrlTags
};

enum EMsgTags
{
  kMsgTagClearIR = 0,
  kMsgTagClearIR2,
  kMsgTagClearIR3,
  kMsgTagClearIR4,
  kMsgTagHighlightColor,
  kMsgTagLoadFailed,
  kMsgTagLoadedIR,
  kMsgTagLoadedIR2,
  kMsgTagLoadedIR3,
  kMsgTagLoadedIR4,
  kMsgTagLoadFailedIR2,
  kMsgTagLoadFailedIR3,
  kMsgTagLoadFailedIR4,
  kNumMsgTags
};

class Impulse final : public iplug::Plugin
{
public:
  Impulse(const iplug::InstanceInfo& info);
  ~Impulse();

#if !defined(WEB_API) && !defined(WASM_UI_API)
  void ProcessBlock(iplug::sample** inputs, iplug::sample** outputs, int nFrames) override;
  void OnReset() override;
#endif
  void OnIdle() override;

#if defined(WEB_API) || defined(WASM_UI_API)
  double GetSampleRate() const { return 48000.0; }
  int GetBlockSize() const { return 512; }
  int NInChansConnected() const { return 1; }
  int NOutChansConnected() const { return 1; }
  int GetLatency() const { return 0; }
  void SetLatency(int latency) { (void)latency; }
#endif

  bool SerializeState(iplug::IByteChunk& chunk) const override;
  int UnserializeState(const iplug::IByteChunk& chunk, int startPos) override;
  void OnUIOpen() override;
  void OnParentWindowResize(int width, int height) override;
  bool ConstrainEditorResize(int& w, int& h) const override;
  bool OnHostRequestingSupportedViewConfiguration(int width, int height) override { return true; }

  void OnParamChange(int paramIdx) override;
  void OnParamChangeUI(int paramIdx, iplug::EParamSource source) override;
  bool OnMessage(int msgTag, int ctrlTag, int dataSize, const void* pData) override;

private:
  void _AllocateIOPointers(const size_t nChans);
  void _ApplyDSPStaging();
  void _DeallocateIOPointers();
  void _PrepareBuffers(const size_t numChannels, const size_t numFrames);
  void _PrepareIOPointers(const size_t nChans);
  void _ProcessInput(iplug::sample** inputs, const size_t nFrames, const size_t nChansIn, const size_t nChansOut);
  void _ProcessOutput(iplug::sample** inputs, iplug::sample** outputs, const size_t nFrames, const size_t nChansIn,
                      const size_t nChansOut);
  void _ResetIRs(const double sampleRate, const int maxBlockSize);

  dsp::wav::LoadReturnCode _StageIR(const WDL_String& irPath, int irIndex);

  void _ApplyIRDelay(iplug::sample** signal, int delay, std::vector<double>& buf, size_t& writePos, size_t nChans, size_t nFrames);
  void _UpdateMeters(iplug::sample** inputPointer, iplug::sample** outputPointer, const size_t nFrames,
                     const size_t nChansIn, const size_t nChansOut);

  size_t _GetBufferNumChannels() const;
  size_t _GetBufferNumFrames() const;

  int _UnserializeStateWithKnownVersion(const iplug::IByteChunk& chunk, int startPos);
  int _UnserializeStateWithUnknownVersion(const iplug::IByteChunk& chunk, int startPos);
  void _UnserializeApplyConfig(nlohmann::json& config);

  // Member data

  std::vector<std::vector<iplug::sample>> mInputArray;
  std::vector<std::vector<iplug::sample>> mOutputArray;
  iplug::sample** mInputPointers = nullptr;
  iplug::sample** mOutputPointers = nullptr;

  std::vector<iplug::sample> mPreGainBuffer;
  iplug::sample* mPreGainPointers[1] = {};

  double mInputGain = 1.0;
  double mOutputGain = 1.0;

  // IRs
  struct IRSlot {
    std::unique_ptr<dsp::ImpulseResponse> ir;
    std::unique_ptr<dsp::ImpulseResponse> stagedIR;
    std::atomic<bool> shouldRemove = false;
    std::vector<double> delayBuffer;
    size_t delayWritePos = 0;
    WDL_String path;
    // Per-IR HPF/LPF filters (up to 3 cascaded stages)
    std::vector<recursive_linear_filter::HighPass> hpfStages;
    std::vector<recursive_linear_filter::LowPass> lpfStages;
  };
  std::array<IRSlot, kNumIRs> mIRSlots;

  // Retired IRs awaiting destruction on idle thread (avoids audio thread stall)
  std::vector<std::unique_ptr<dsp::ImpulseResponse>> mIRRetirement;

  // DC blocker (post-mix, before output)
  recursive_linear_filter::HighPass mDCBlocker;

  // Cached IR mode to avoid per-block param lookup
  int mIRMode = 0;

  // Temp buffer for parallel IR blend
  std::vector<double> mIRBlendBuffer;

  // Stereo IR buffer for pan
  std::vector<iplug::sample> mIRStereoBuffer;
  // Stereo output state (updated in ProcessBlock, read in OnParamChangeUI)
  std::atomic<bool> mStereoOutput{true};

  WDL_String mHighLightColor{PluginColors::NAM_THEMECOLOR.ToColorCode()};

  NAMSender mInputSender, mOutputSender;
  
  // Click-free fade for IR toggles
  int mFadeCounter = 0;
};
