#pragma once

#include <cmath>
#include <string>
#include <vector>

#include "../AudioDSPTools/dsp/ImpulseResponse.h"
#include "../AudioDSPTools/dsp/NoiseGate.h"
#include "../AudioDSPTools/dsp/RecursiveLinearFilter.h"
#include "../AudioDSPTools/dsp/dsp.h"
#include "../AudioDSPTools/dsp/wav.h"
#include "../AudioDSPTools/dsp/ResamplingContainer/ResamplingContainer.h"
#include "../NeuralAmpModelerCore/NAM/dsp.h"

#include "Colors.h"

#include "IPlug_include_in_plug_hdr.h"
#include "ISender.h"


const int kNumPresets = 1;
// The plugin is mono inside
constexpr size_t kNumChannelsInternal = 1;

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
  // These need to be the first ones because I use their indices to place
  // their rects in the GUI.
  kInputLevel = 0,
  kNoiseGateThreshold,
  kToneMid,
  kOutputLevel,
  kInputLevel2,
  kOutputLevel2,
  kInputTrim,
  // The rest is fine though.
  kNoiseGateActive,
  kIRToggle,
  // Input calibration for model 1
  kCalibrateInput,
  kInputCalibrationLevel,
  // Input calibration for model 2
  kCalibrateInput2,
  kInputCalibrationLevel2,
  kOutputMode,
  kOutputMode2,
  kAutoGain,
  kOversampling,
  kIRToggle2,
  kIRPolarity,
  kIRPolarity2,
  kIRLevel,
  kIRLevel2,
  kIRMute,
  kIRMute2,
  kIRDelay,
  kIRDelay2,
  kIRMode,
  kBlendNearest,
  kIRPan,
  kIRPan2,
  kNumParams
};

const int numKnobs = 5;

enum ECtrlTags
{
  kCtrlTagIRFileBrowser = 0,
  kCtrlTagIRFileBrowser2,
  kCtrlTagInputMeter,
  kCtrlTagOutputMeter,
  kCtrlTagSettingsBox,
  kCtrlTagOutputMode2,
  kCtrlTagCalibrateInput,
  kCtrlTagInputCalibrationLevel,
  kCtrlTagCalibrateInput2,
  kCtrlTagInputCalibrationLevel2,
  kNumCtrlTags
};

enum EMsgTags
{
  // These tags are used from UI -> DSP
  kMsgTagClearModel = 0,
  kMsgTagClearModel2,
  kMsgTagClearIR,
  kMsgTagClearIR2,
  kMsgTagHighlightColor,
  // The following tags are from DSP -> UI
  kMsgTagLoadFailed,
  kMsgTagLoadedModel,
  kMsgTagLoadedModel2,
  kMsgTagLoadFailed2,
  kMsgTagLoadedIR,
  kMsgTagLoadedIR2,
  kMsgTagLoadFailedIR2,
  kNumMsgTags
};

// Get the sample rate of a NAM model.
// Sometimes, the model doesn't know its own sample rate; this wrapper guesses 48k based on the way that most
// people have used NAM in the past.
double GetNAMSampleRate(const std::unique_ptr<nam::DSP>& model)
{
  // Some models are from when we didn't have sample rate in the model.
  // For those, this wraps with the assumption that they're 48k models, which is probably true.
  const double assumedSampleRate = 48000.0;
  const double reportedEncapsulatedSampleRate = model->GetExpectedSampleRate();
  const double encapsulatedSampleRate =
    reportedEncapsulatedSampleRate <= 0.0 ? assumedSampleRate : reportedEncapsulatedSampleRate;
  return encapsulatedSampleRate;
};

class ResamplingNAM : public nam::DSP
{
public:
  static constexpr double kRateBase = 48000.0;
  static constexpr double kRateNorm = 384000.0;
  static constexpr double kMaxRenderRate = 192000.0;
  static int MaxOversamplingFactorForRate(double projectSampleRate)
  {
    return static_cast<int>(kMaxRenderRate / projectSampleRate);
  }

  ResamplingNAM(std::unique_ptr<nam::DSP> encapsulated, const double expected_sample_rate)
  : nam::DSP(1, encapsulated->NumOutputChannels(), expected_sample_rate)
  , mEncapsulated(std::move(encapsulated))
  , mResampler(std::make_shared<ResamplingContainer>(GetNAMSampleRate(mEncapsulated)))
  {
    auto ProcessBlockFunc = [&](NAM_SAMPLE** input, NAM_SAMPLE** output, int numFrames) {
      if (mEncapsulated->NumInputChannels() == 2)
      {
        mRateChannelBuffer.assign(numFrames, mNormalizedRate);
        NAM_SAMPLE* twoChannelInput[2] = {input[0], mRateChannelBuffer.data()};
        mEncapsulated->process(twoChannelInput, output, numFrames);
      }
      else
      {
        mEncapsulated->process(input, output, numFrames);
      }
    };
    mBlockProcessFunc = ProcessBlockFunc;

    if (mEncapsulated->HasLoudness())
      SetLoudness(mEncapsulated->GetLoudness());
    if (mEncapsulated->HasInputLevel())
      SetInputLevel(mEncapsulated->GetInputLevel());
    if (mEncapsulated->HasOutputLevel())
      SetOutputLevel(mEncapsulated->GetOutputLevel());

    int maxBlockSize = 2048;
    Reset(expected_sample_rate, maxBlockSize);
  };

  ~ResamplingNAM() = default;

  void SetOversamplingFactor(int factor, double projectSampleRate)
  {
    double renderingRate = factor > 1 ? projectSampleRate * factor : projectSampleRate;
    renderingRate = std::ceil(renderingRate / kRateBase) * kRateBase;
    mNormalizedRate = (renderingRate - kRateBase) / kRateNorm;
    if (projectSampleRate != renderingRate)
    {
      auto newResampler = std::make_shared<ResamplingContainer>(renderingRate);
      newResampler->Reset(projectSampleRate, mMaxExternalBlockSize);
      {
        std::lock_guard<std::mutex> lock(mResamplerMutex);
        mResampler = std::move(newResampler);
      }
    }
    mOversamplingFactor = factor;
  }

  void prewarm() override { mEncapsulated->prewarm(); };

  void process(NAM_SAMPLE** input, NAM_SAMPLE** output, const int num_frames) override
  {
    if (num_frames > mMaxExternalBlockSize)
      throw std::runtime_error("More frames were provided than the max expected!");

    int osFactor = mOversamplingFactor;
    double renderingRate = osFactor > 1 ? mExpectedSampleRate * osFactor : mExpectedSampleRate;
    renderingRate = std::ceil(renderingRate / kRateBase) * kRateBase;

    if (mExpectedSampleRate != renderingRate)
    {
      std::shared_ptr<ResamplingContainer> resampler;
      {
        std::lock_guard<std::mutex> lock(mResamplerMutex);
        resampler = mResampler;
      }
      if (resampler) resampler->ProcessBlock(input, output, num_frames, mBlockProcessFunc);
    }
    else
    {
      if (mEncapsulated->NumInputChannels() == 2)
      {
        mRateChannelBuffer.assign(num_frames, mNormalizedRate);
        NAM_SAMPLE* twoChannelInput[2] = {input[0], mRateChannelBuffer.data()};
        mEncapsulated->process(twoChannelInput, output, num_frames);
      }
      else
      {
        mEncapsulated->process(input, output, num_frames);
      }
    }
  };

  int GetLatency() const
  {
    if (NeedToResample())
    {
      std::lock_guard<std::mutex> lock(mResamplerMutex);
      if (mResampler) return mResampler->GetLatency();
    }
    return 0;
  };

  void Reset(const double sampleRate, const int maxBlockSize) override
  {
    mExpectedSampleRate = sampleRate;
    mMaxExternalBlockSize = maxBlockSize;
    mNormalizedRate = (GetRenderingRate(sampleRate) - kRateBase) / kRateNorm;
    SetupResampler(sampleRate, maxBlockSize);

    double renderingRate = GetRenderingRate(sampleRate);
    const double maxRenderingRate = std::max(GetEncapsulatedSampleRate(), sampleRate) * 4.0;
    const double worstUpRatio = sampleRate / maxRenderingRate;
    const int worstBlockSize = static_cast<int>(std::ceil(static_cast<double>(maxBlockSize) / worstUpRatio));
    mEncapsulated->ResetAndPrewarm(renderingRate, worstBlockSize);
  };

  double GetEncapsulatedSampleRate() const { return GetNAMSampleRate(mEncapsulated); };
  nam::DSP* GetEncapsulated() const { return mEncapsulated.get(); };

private:
  double GetRenderingRate(double projectSampleRate) const
  {
    double rate = mOversamplingFactor > 1 ? projectSampleRate * mOversamplingFactor : projectSampleRate;
    return std::ceil(rate / kRateBase) * kRateBase;
  }

  void SetupResampler(double projectSampleRate, int maxBlockSize)
  {
    double renderingRate = GetRenderingRate(projectSampleRate);
    auto newResampler = std::make_shared<ResamplingContainer>(renderingRate);
    newResampler->Reset(projectSampleRate, maxBlockSize);
    std::lock_guard<std::mutex> lock(mResamplerMutex);
    mResampler = std::move(newResampler);
  }

  bool NeedToResample() const
  {
    return mExpectedSampleRate != GetRenderingRate(mExpectedSampleRate);
  }

  std::unique_ptr<nam::DSP> mEncapsulated;

  using ResamplingContainer = dsp::ResamplingContainer<NAM_SAMPLE, 1, 6>;
  std::shared_ptr<ResamplingContainer> mResampler;
  mutable std::mutex mResamplerMutex;

  int mOversamplingFactor = 1;
  int mMaxExternalBlockSize = 0;
  double mNormalizedRate = 0.0;
  std::vector<NAM_SAMPLE> mRateChannelBuffer;

  std::function<void(NAM_SAMPLE**, NAM_SAMPLE**, int)> mBlockProcessFunc;
};

struct NAMCapture
{
  double tone = -1.0;
  double gain = -1.0;
  std::string path;
  std::unique_ptr<ResamplingNAM> model;
};

struct BlendWeight
{
  size_t captureIndex;
  double weight;
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
  void _ComputeBlendWeights();
  void _ComputePowerAmpBlendWeights();
  void _DeallocateIOPointers();
  void _FallbackDSP(iplug::sample** inputs, iplug::sample** outputs, const size_t numChannels, const size_t numFrames);
  size_t _GetBufferNumChannels() const;
  size_t _GetBufferNumFrames() const;
  // Load all NAM captures from a directory. Parses filenames to determine
  // tone/gain positions. Returns empty string on success.
  std::string _LoadCapturesFromDirectory(const WDL_String& dirPath);
  std::string _StageModel2(const WDL_String& dspFile);
  dsp::wav::LoadReturnCode _StageIR(const WDL_String& irPath);
  dsp::wav::LoadReturnCode _StageIR2(const WDL_String& irPath);

  void _PrepareBuffers(const size_t numChannels, const size_t numFrames);
  void _PrepareIOPointers(const size_t nChans);
  void _ProcessInput(iplug::sample** inputs, const size_t nFrames, const size_t nChansIn, const size_t nChansOut);
  void _ProcessOutput(iplug::sample** inputs, iplug::sample** outputs, const size_t nFrames, const size_t nChansIn,
                      const size_t nChansOut);
  void _ResetModelAndIR(const double sampleRate, const int maxBlockSize);

  void _SetInputGain();
  void _SetOutputGain();
  void _SetInputGain2();
  void _SetOutputGain2();
  void _UpdateAutoGainOutputKnob();

  void _UnserializeApplyConfig(nlohmann::json& config);
  int _UnserializeStateWithKnownVersion(const iplug::IByteChunk& chunk, int startPos);
  int _UnserializeStateWithUnknownVersion(const iplug::IByteChunk& chunk, int startPos);

  void _ApplyIRDelay(iplug::sample** signal, int delay, std::vector<double>& buf, size_t& writePos, size_t nChans, size_t nFrames);
  void _UpdateControlsFromModel();
  void _UpdateLatency();
  void _UpdateMeters(iplug::sample** inputPointer, iplug::sample** outputPointer, const size_t nFrames,
                     const size_t nChansIn, const size_t nChansOut);

  // Member data

  std::vector<std::vector<iplug::sample>> mInputArray;
  std::vector<std::vector<iplug::sample>> mOutputArray;
  iplug::sample** mInputPointers = nullptr;
  iplug::sample** mOutputPointers = nullptr;

  std::vector<iplug::sample> mPreGainBuffer;
  iplug::sample* mPreGainPointers[1] = {};

  // Temporary buffer for blend accumulation
  std::vector<iplug::sample> mBlendTempOutput;

  double mInputGain = 1.0;
  double mOutputGain = 1.0;

  dsp::noise_gate::Trigger mNoiseGateTrigger;
  dsp::noise_gate::Gain mNoiseGateGain;

  // Preamp captures (multi-capture blending)
  std::vector<NAMCapture> mPreampCaptures;
  std::vector<NAMCapture> mStagedPreampCaptures;
  std::atomic<bool> mNewCapturesLoaded = false;
  // Blend weights for current tone/gain position
  std::vector<BlendWeight> mBlendWeights;
  std::atomic<bool> mBlendWeightsDirty{true};

  // IR
  std::unique_ptr<dsp::ImpulseResponse> mIR;
  std::unique_ptr<dsp::ImpulseResponse> mStagedIR;
  std::atomic<bool> mShouldRemoveIR = false;

  // IR 2 (parallel)
  std::unique_ptr<dsp::ImpulseResponse> mIR2;
  std::unique_ptr<dsp::ImpulseResponse> mStagedIR2;
  std::atomic<bool> mShouldRemoveIR2 = false;

  // Power amp captures (multi-capture, gain-indexed blending)
  std::vector<NAMCapture> mPowerAmpCaptures;
  std::vector<NAMCapture> mStagedPowerAmpCaptures;
  std::atomic<bool> mNewPowerAmpCapturesLoaded = false;
  // Blend weights for current gain position (power amp)
  std::vector<BlendWeight> mPowerAmpBlendWeights;
  std::atomic<bool> mPowerAmpBlendWeightsDirty{true};


  // Post-IR filters
  recursive_linear_filter::HighPass mHighPass;

  // Cached IR mode to avoid per-block param lookup
  int mIRMode = 0;

  // Temp buffer for parallel IR blend
  std::vector<double> mIRBlendBuffer;

  // Stereo IR buffer for pan
  std::vector<iplug::sample> mIRStereoBuffer;
  // Stereo output state (updated in ProcessBlock, read in OnParamChangeUI)
  std::atomic<bool> mStereoOutput{true};

  // IR sample delay buffers
  std::vector<double> mIR1DelayBuffer;
  std::vector<double> mIR2DelayBuffer;
  size_t mIR1DelayWritePos = 0;
  size_t mIR2DelayWritePos = 0;

  // Paths
  WDL_String mCaptureDirPath;
  WDL_String mNAMPath2;
  WDL_String mIRPath;
  WDL_String mIRPath2;

  WDL_String mHighLightColor{PluginColors::NAM_THEMECOLOR.ToColorCode()};

  // Input and output gain for model 2
  double mInputGain2 = 1.0;
  double mOutputGain2 = 1.0;

  // Auto-gain: saved Output base value for restoration when toggling off
  double mAutoGainOutputBase = 0.0;

  int mOversamplingFactor = 1;

  void _ApplyOversamplingToCaptures();

  NAMSender mInputSender, mOutputSender;
};
