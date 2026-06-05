// Unserialization
//
// Simplified for Impulse IR-only plugin.

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

int _UnserializePathsAndParams(const iplug::IByteChunk& chunk, int startPos, nlohmann::json& config,
                               std::vector<std::string>& paramNames)
{
  int pos = startPos;
  WDL_String path;

  for (int i = 0; i < kNumIRs; i++)
  {
    pos = chunk.GetStr(path, pos);
    config["IRPath" + std::to_string(i)] = std::string(path.Get());
  }

  for (auto it = paramNames.begin(); it != paramNames.end(); ++it)
  {
    double v = 0.0;
    pos = chunk.Get(&v, pos);
    config[*it] = v;
  }
  return pos;
}

int _GetConfigFrom_0_9_2(const iplug::IByteChunk& chunk, int startPos, nlohmann::json& config)
{
  std::vector<std::string> paramNames{
    "Input", "Gate", "Output", "Trim", "NoiseGateActive", "IR Mode",
    "IR 1", "Phase", "Level", "Mute", "Delay", "Pan",
    "IR 2", "Phase", "Level", "Mute", "Delay", "Pan",
    "IR 3", "Phase", "Level", "Mute", "Delay", "Pan",
    "IR 4", "Phase", "Level", "Mute", "Delay", "Pan"
  };

  int pos = _UnserializePathsAndParams(chunk, startPos, config, paramNames);
  return pos;
}

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

  nlohmann::json config;
  if (version >= _Version(0, 9, 2))
  {
    pos = _GetConfigFrom_0_9_2(chunk, pos, config);
  }
  else
  {
    // Legacy fallback: just try to load params directly
    pos = UnserializeParams(chunk, pos);
    return pos;
  }
  _UnserializeApplyConfig(config);
  return pos;
}

int Impulse::_UnserializeStateWithUnknownVersion(const iplug::IByteChunk& chunk, int startPos)
{
  // Try to read as legacy format
  int pos = UnserializeParams(chunk, startPos);
  return pos;
}
