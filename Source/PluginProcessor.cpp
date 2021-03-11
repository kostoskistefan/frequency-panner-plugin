/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

#define WIND 10
#define CYCLE 400

//==============================================================================
FrequencyPannerAudioProcessor::FrequencyPannerAudioProcessor(): inputData(), weights(normalDistribution()), pitch(0)
#ifndef JucePlugin_PreferredChannelConfigurations
     , AudioProcessor (BusesProperties().withInput("Input", AudioChannelSet::mono(), true).withOutput("Output", AudioChannelSet::stereo(), true))
#endif
{}

FrequencyPannerAudioProcessor::~FrequencyPannerAudioProcessor()
{
}

//==============================================================================
const String FrequencyPannerAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool FrequencyPannerAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool FrequencyPannerAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool FrequencyPannerAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double FrequencyPannerAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int FrequencyPannerAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int FrequencyPannerAudioProcessor::getCurrentProgram()
{
    return 0;
}

void FrequencyPannerAudioProcessor::setCurrentProgram (int index)
{
}

const String FrequencyPannerAudioProcessor::getProgramName (int index)
{
    return {};
}

void FrequencyPannerAudioProcessor::changeProgramName (int index, const String& newName)
{
}

//==============================================================================
void FrequencyPannerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
}

void FrequencyPannerAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool FrequencyPannerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

std::vector<float> FrequencyPannerAudioProcessor::normalDistribution()
{
    using namespace std;

    vector<float> weights;
    vector<float> rev;
    float sum = 0;

    for (int i = 0; i <= WIND; i++)
    {
        float data = exp(-pow(2.0f * i / 11.0f, 2));
        weights.push_back(data);

        if (i > 0)
        {
            rev.insert(rev.begin(), data);
            sum += (data * 2);
        }
    }

    sum++;

    weights.insert(weights.begin(), rev.begin(), rev.end());

    for (float& weight : weights)
        weight /= sum;

    return weights;
}

std::vector<float> FrequencyPannerAudioProcessor::convolute(std::vector<float> in)
{
    std::vector<float> output;

    for (int i = 0; i <= in.size() - weights.size(); i++)
    {
        float sum = 0;

        for (int j = 0; j < weights.size(); j++)
            sum += in[i + j] * weights[j];

        output.push_back(sum);
    }

    return output;
}

void FrequencyPannerAudioProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midiMessages)
{
    ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    auto* channelData = buffer.getReadPointer(0);
    auto* outputL = buffer.getWritePointer(0);
    auto* outputR = buffer.getWritePointer(1);

    std::vector<float> samples;

    for (int i = 0; i < buffer.getNumSamples(); i++)
        samples.push_back(channelData[i]);

    std::vector<float> smoothData = convolute(samples);
    std::vector<float> smoothPitches(1, 0);
    std::vector<float> dips;

    for (int i = 1; i < CYCLE; i++)
    {
        float sum = 0;

        for (int j = 0; j < smoothData.size() - i; j++)
            sum += smoothData[j] - smoothData[j + i];

        smoothPitches.push_back(sum / (smoothData.size() - i));
    }

    for (int i = WIND; i < CYCLE - WIND; i++)
    {
        float min = smoothPitches[i - WIND];

        for (int j = i - WIND; j < i + WIND; j++)
        {
            if (min > smoothPitches[j])
                min = smoothPitches[j];
        }

        if (smoothPitches[i] == min)
            dips.push_back(i);
    }

    if (dips.size() > 1)
    {
        int size = dips.size() - 1;
        float sum = 0;

        for (int i = 0; i < size; i++)
            sum += dips[i + 1] - dips[i];

        float avDip = sum / size;
        float cheqFreq = 44100 / avDip;
        pitch = pitch * 0.5 + cheqFreq * 0.5;
    }

    float pan = pitch / 2000.0f;

    for (int i = 0; i < buffer.getNumSamples(); i++)
    {
        outputL[i] *= pan;
        outputR[i] *= (1 - pan);
    }
}

//==============================================================================
bool FrequencyPannerAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

AudioProcessorEditor* FrequencyPannerAudioProcessor::createEditor()
{
    return new FrequencyPannerAudioProcessorEditor (*this);
}

//==============================================================================
void FrequencyPannerAudioProcessor::getStateInformation (MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void FrequencyPannerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FrequencyPannerAudioProcessor();
}
