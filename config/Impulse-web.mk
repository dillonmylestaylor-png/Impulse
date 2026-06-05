# IPLUG2_ROOT should point to the top level IPLUG2 folder from the project folder
# By default, that is three directories up from /Examples/NeuralAmpModeler/config
IPLUG2_ROOT = ../../iPlug2

include ../../common-web.mk

SRC += "$(PROJECT_ROOT)/Impulse.cpp"

# WAM_SRC += 
WAM_SRC += "$(PROJECT_ROOT)/../AudioDSPTools/dsp/dsp.cpp" \
	"$(PROJECT_ROOT)/../AudioDSPTools/dsp/ImpulseResponse.cpp" \
	"$(PROJECT_ROOT)/../AudioDSPTools/dsp/NoiseGate.cpp" \
	"$(PROJECT_ROOT)/../AudioDSPTools/dsp/RecursiveLinearFilter.cpp" \
	"$(PROJECT_ROOT)/../AudioDSPTools/dsp/wav.cpp" \
	"$(PROJECT_ROOT)/../NeuralAmpModelerCore/NAM/activations.cpp" \
	"$(PROJECT_ROOT)/../NeuralAmpModelerCore/NAM/dsp.cpp" \
	"$(PROJECT_ROOT)/../NeuralAmpModelerCore/NAM/get_dsp.cpp" \
	"$(PROJECT_ROOT)/../NeuralAmpModelerCore/NAM/container.cpp" \
	"$(PROJECT_ROOT)/../NeuralAmpModelerCore/NAM/conv1d.cpp" \
	"$(PROJECT_ROOT)/../NeuralAmpModelerCore/NAM/convnet.cpp" \
	"$(PROJECT_ROOT)/../NeuralAmpModelerCore/NAM/lstm.cpp" \
	"$(PROJECT_ROOT)/../NeuralAmpModelerCore/NAM/ring_buffer.cpp" \
	"$(PROJECT_ROOT)/../NeuralAmpModelerCore/NAM/util.cpp" \
	"$(PROJECT_ROOT)/../NeuralAmpModelerCore/NAM/wavenet/a2_fast.cpp" \
	"$(PROJECT_ROOT)/../NeuralAmpModelerCore/NAM/wavenet/model.cpp" \
	"$(PROJECT_ROOT)/../NeuralAmpModelerCore/NAM/wavenet/slimmable.cpp" \

# WAM_CFLAGS +=

WEB_CFLAGS += -DIGRAPHICS_NANOVG -DIGRAPHICS_GLES2

WAM_LDFLAGS += -O0 -s EXPORT_NAME="'AudioWorkletGlobalScope_WAM_Impulse'" -s ASSERTIONS=0

WEB_LDFLAGS += -O0 -s ASSERTIONS=0

WEB_LDFLAGS += $(NANOVG_LDFLAGS)
