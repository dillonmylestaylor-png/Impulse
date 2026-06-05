/* Declares the NeuralAmpModeler Audio Worklet Processor */

class NeuralAmpModeler_AWP extends AudioWorkletGlobalScope.WAMProcessor
{
  constructor(options) {
    options = options || {}
    options.mod = AudioWorkletGlobalScope.WAM.NeuralAmpModeler;
    super(options);
  }
}

registerProcessor("NeuralAmpModeler", NeuralAmpModeler_AWP);
