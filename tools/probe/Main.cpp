// pakku_probe — offline rendering, no DAW involved.
//
// Loads the plugin the way a host would, lists the parameters it publishes and
// renders test signals to 32-bit float WAV. Useful for checking DSP changes
// outside a host, where the result is reproducible.

#include <juce_audio_utils/juce_audio_utils.h>

using namespace juce;

namespace
{

std::unique_ptr<AudioPluginInstance> loadPlugin (const String& path, double sr, int block, String& err)
{
    AudioPluginFormatManager fm;
   #if JUCE_PLUGINHOST_AU && JUCE_MAC
    fm.addFormat (new AudioUnitPluginFormat());
   #endif
   #if JUCE_PLUGINHOST_VST3
    fm.addFormat (new VST3PluginFormat());
   #endif

    OwnedArray<PluginDescription> descs;
    for (auto* fmt : fm.getFormats())
        fmt->findAllTypesForFile (descs, path);

    if (descs.isEmpty())
    {
        err = "nenhum plugin encontrado em: " + path;
        return nullptr;
    }

    return fm.createPluginInstance (*descs[0], sr, block, err);
}

String csvEscape (const String& s)
{
    return "\"" + s.replace ("\"", "\"\"") + "\"";
}

void listParameters (AudioPluginInstance& p)
{
    std::cout << "index,name,label,default,current,text,num_steps,discrete,automatable\n";

    const auto& params = p.getParameters();
    for (int i = 0; i < params.size(); ++i)
    {
        auto* par = params[i];
        std::cout << i << ","
                  << csvEscape (par->getName (256)) << ","
                  << csvEscape (par->getLabel()) << ","
                  << par->getDefaultValue() << ","
                  << par->getValue() << ","
                  << csvEscape (par->getText (par->getValue(), 64)) << ","
                  << par->getNumSteps() << ","
                  << (par->isDiscrete() ? 1 : 0) << ","
                  << (par->isAutomatable() ? 1 : 0) << "\n";
    }
}

// "3=0.75,7=0.1" -> aplica valores normalizados (0..1)
void applyParamSets (AudioPluginInstance& p, const String& spec)
{
    if (spec.isEmpty()) return;

    const auto& params = p.getParameters();
    for (const auto& pair : StringArray::fromTokens (spec, ",", ""))
    {
        const auto idx  = pair.upToFirstOccurrenceOf ("=", false, false).trim().getIntValue();
        const auto val  = pair.fromFirstOccurrenceOf ("=", false, false).trim().getFloatValue();

        if (isPositiveAndBelow (idx, params.size()))
        {
            params[idx]->beginChangeGesture();
            params[idx]->setValueNotifyingHost (jlimit (0.0f, 1.0f, val));
            params[idx]->endChangeGesture();
            std::cerr << "  set [" << idx << "] " << params[idx]->getName (64)
                      << " = " << val
                      << "  (" << params[idx]->getText (params[idx]->getValue(), 64) << ")\n";
        }
        else
        {
            std::cerr << "  WARNING: parameter index out of range: " << idx << "\n";
        }
    }
}

int render (AudioPluginInstance& p, const File& in, const File& out, int block)
{
    AudioFormatManager afm;
    afm.registerBasicFormats();

    std::unique_ptr<AudioFormatReader> reader (afm.createReaderFor (in));
    if (reader == nullptr)
    {
        std::cerr << "error: could not read " << in.getFullPathName() << "\n";
        return 1;
    }

    const auto sr        = reader->sampleRate;
    const auto numFrames = (int) reader->lengthInSamples;
    const int  numCh     = 2;

    AudioBuffer<float> buf (numCh, numFrames);
    reader->read (&buf, 0, numFrames, 0, true, true);

    // mono -> stereo, if needed
    if (reader->numChannels == 1)
        buf.copyFrom (1, 0, buf, 0, 0, numFrames);

    p.setPlayConfigDetails (numCh, numCh, sr, block);
    p.prepareToPlay (sr, block);

    const auto latency = p.getLatencySamples();
    std::cerr << "  reported latency: " << latency << " samples ("
              << String (1000.0 * latency / sr, 3) << " ms)\n";

    MidiBuffer midi;
    for (int pos = 0; pos < numFrames; pos += block)
    {
        const auto n = jmin (block, numFrames - pos);

        AudioBuffer<float> slice (buf.getArrayOfWritePointers(), numCh, pos, n);
        midi.clear();
        p.processBlock (slice, midi);
    }

    p.releaseResources();

    out.deleteFile();
    std::unique_ptr<OutputStream> os (out.createOutputStream());
    WavAudioFormat wav;

    /*  32-bit float, not PCM.

        PCM clips at +/-1.0, so any output above 0 dBFS would be cut on write —
        a non-linearity at the base rate, which manufactures aliasing and
        falsifies every saturation measurement. Float keeps what the plugin
        actually produced.                                                   */
    const auto opts = AudioFormatWriterOptions{}
                          .withSampleRate (sr)
                          .withNumChannels (numCh)
                          .withBitsPerSample (32)
                          .withSampleFormat (AudioFormatWriterOptions::SampleFormat::floatingPoint);

    auto writer = wav.createWriterFor (os, opts);

    if (writer == nullptr)
    {
        std::cerr << "error: could not write " << out.getFullPathName() << "\n";
        return 1;
    }
    writer->writeFromAudioSampleBuffer (buf, 0, numFrames);
    writer.reset();

    std::cerr << "  escrito: " << out.getFullPathName() << "\n";
    std::cout << "latency_samples," << latency << "\n";
    return 0;
}

} // namespace

int main (int argc, char* argv[])
{
    ScopedJuceInitialiser_GUI juceInit;
    ArgumentList args (argc, argv);

    const auto pluginPath = args.containsOption ("--plugin")
        ? args.getValueForOption ("--plugin")
        : String ("/Library/Audio/Plug-Ins/Components/Pakku.component");

    const auto sr    = args.containsOption ("--sr")    ? args.getValueForOption ("--sr").getDoubleValue() : 48000.0;
    const auto block = args.containsOption ("--block") ? args.getValueForOption ("--block").getIntValue() : 512;

    String err;
    auto plugin = loadPlugin (pluginPath, sr, block, err);

    if (plugin == nullptr)
    {
        std::cerr << "falha ao carregar: " << err << "\n";
        return 1;
    }

    std::cerr << "carregado: " << plugin->getName()
              << "  [" << plugin->getParameters().size() << " parameters]\n";

    if (args.containsOption ("--list"))
    {
        listParameters (*plugin);
        return 0;
    }

    if (args.containsOption ("--render"))
    {
        applyParamSets (*plugin, args.containsOption ("--set") ? args.getValueForOption ("--set") : String());

        return render (*plugin,
                       File (args.getValueForOption ("--in")),
                       File (args.getValueForOption ("--out")),
                       block);
    }

    std::cerr << "usage:\n"
              << "  pakku_probe --list\n"
              << "  pakku_probe --render --in=<wav> --out=<wav> [--set=0=0.5,3=1.0]\n"
              << "  options: --plugin=<path> --sr=<hz> --block=<n>\n"
              << "  (long options need '=' before the value)\n";
    return 0;
}
