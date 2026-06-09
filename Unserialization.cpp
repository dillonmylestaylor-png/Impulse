// Unserialization
//
// Simplified for Impulse IR-only plugin.

#include "Impulse.h"
#include <iostream>
#include "json.hpp"

void Impulse::_UnserializeApplyConfig(nlohmann::json& config)
{
  auto getParamByName = [&](std::string& name) {
    for (int i = 0; i < kNumParams; i++)
    {
      iplug::IParam* param = GetParam(i);
      if (strcmp(param->GetName(), name.c_str()) == 0)
      {
        return param;
      }
    }
    return (iplug::IParam*)nullptr;
  };
  TRACE
  ENTER_PARAMS_MUTEX
  for (auto it = config.begin(); it != config.end(); ++it)
  {
    std::string name = it.key();
    iplug::IParam* pParam = getParamByName(name);
    if (pParam != nullptr)
    {
      try
      {
        pParam->Set(*it);
      }
      catch (nlohmann::json::type_error& e)
      {
        std::cerr << "[Impulse] ERROR setting " << name << ": " << e.what() << " (value: " << *it << ")" << std::endl;
      }
      iplug::Trace(TRACELOC, "%s %f", pParam->GetName(), pParam->Value());
    }
    else
    {
      iplug::Trace(TRACELOC, "%s NOT-FOUND", name.c_str());
    }
  }
  OnParamReset(iplug::EParamSource::kPresetRecall);
  LEAVE_PARAMS_MUTEX

  for (int i = 0; i < kNumIRs; i++)
  {
    std::string key = "IRPath" + std::to_string(i);
    std::string path = config.value(key, "");
    if (!path.empty())
    {
      mIRSlots[i].path.Set(path.c_str());
      _StageIR(mIRSlots[i].path, i);
    }
  }
}

// Direct index mapping from old 0.9.2 format (30 doubles at old param indices)
// to current param indices. Old indices 1 (Gate) and 4 (NoiseGateActive) were
// removed, so the current index order is shifted: old[0->Input]->current[0],
// old[2->Output]->current[1], old[3->Trim]->current[2], old[5->Mode]->current[3],
// old[6..29]->current[4..27].
static const int kOldToNewIdx[30] = {
   0, -1,  1,  2, -1,  3,   // Input, Gate(skip), Output, Trim, NoiseGate(skip), Mode
   4,  5,  6,  7,  8,  9,   // IR1
  10, 11, 12, 13, 14, 15,   // IR2
  16, 17, 18, 19, 20, 21,   // IR3
  22, 23, 24, 25, 26, 27    // IR4
};

class _Version
{
public:
  _Version(const int major, const int minor, const int patch)
  : mMajor(major), mMinor(minor), mPatch(patch) {};
  _Version(const std::string& versionStr)
  {
    std::istringstream stream(versionStr);
    std::string token;
    std::vector<int> parts;

    while (std::getline(stream, token, '.'))
    {
      parts.push_back(std::stoi(token));
    }

    if (parts.size() != 3)
    {
      throw std::invalid_argument("Input string does not contain exactly 3 segments separated by '.'");
    }

    mMajor = parts[0];
    mMinor = parts[1];
    mPatch = parts[2];
  };

  bool operator==(const _Version& other) const
  {
    return GetMajor() == other.GetMajor() && GetMinor() == other.GetMinor() && GetPatch() == other.GetPatch();
  };
  bool operator>=(const _Version& other) const
  {
    if (GetMajor() > other.GetMajor()) return true;
    if (GetMajor() < other.GetMajor()) return false;
    if (GetMinor() > other.GetMinor()) return true;
    if (GetMinor() < other.GetMinor()) return false;
    return GetPatch() >= other.GetPatch();
  };

  int GetMajor() const { return mMajor; };
  int GetMinor() const { return mMinor; };
  int GetPatch() const { return mPatch; };

private:
  int mMajor;
  int mMinor;
  int mPatch;
};

int Impulse::_UnserializeStateWithKnownVersion(const iplug::IByteChunk& chunk, int startPos)
{
  int pos = startPos;

  WDL_String wVersion;
  pos = chunk.GetStr(wVersion, pos);
  std::string versionStr(wVersion.Get());
  _Version version(versionStr);

  if (version == _Version(0, 9, 2))
  {
    // Old 0.9.2 format: 4 paths + 30 doubles at old param indices
    // (Gate at index 1, NoiseGateActive at index 4 both removed)
    WDL_String paths[kNumIRs];
    for (int i = 0; i < kNumIRs; i++)
      pos = chunk.GetStr(paths[i], pos);

    ENTER_PARAMS_MUTEX

    for (int i = 0; i < 30; i++)
    {
      double v = 0.0;
      pos = chunk.Get(&v, pos);
      if (kOldToNewIdx[i] >= 0)
        GetParam(kOldToNewIdx[i])->Set(v);
    }

    OnParamReset(iplug::EParamSource::kPresetRecall);
    LEAVE_PARAMS_MUTEX

    for (int i = 0; i < kNumIRs; i++)
    {
      mIRSlots[i].path.Set(paths[i].Get());
      if (paths[i].GetLength())
        _StageIR(mIRSlots[i].path, i);
    }
  }
  else
  {
    // Current or future format: paths are manually serialized before
    // SerializeParams; read them here before UnserializeParams.
    WDL_String path;
    for (int i = 0; i < kNumIRs; i++)
    {
      pos = chunk.GetStr(path, pos);
      mIRSlots[i].path.Set(path.Get());
      if (path.GetLength())
        _StageIR(mIRSlots[i].path, i);
    }
    pos = UnserializeParams(chunk, pos);
  }

  return pos;
}

int Impulse::_UnserializeStateWithUnknownVersion(const iplug::IByteChunk& chunk, int startPos)
{
  // Try to read as legacy format
  int pos = UnserializeParams(chunk, startPos);
  return pos;
}
