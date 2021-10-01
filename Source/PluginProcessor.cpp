/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AudioFilePlayerAudioProcessor::AudioFilePlayerAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
    formatManager.registerBasicFormats();
    directoryScannerBackgroundThread.startThread (3);
}

AudioFilePlayerAudioProcessor::~AudioFilePlayerAudioProcessor()
{
}

//==============================================================================
const juce::String AudioFilePlayerAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool AudioFilePlayerAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool AudioFilePlayerAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool AudioFilePlayerAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double AudioFilePlayerAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int AudioFilePlayerAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int AudioFilePlayerAudioProcessor::getCurrentProgram()
{
    return 0;
}

void AudioFilePlayerAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String AudioFilePlayerAudioProcessor::getProgramName (int index)
{
    return {};
}

void AudioFilePlayerAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void AudioFilePlayerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    transportSource.prepareToPlay(samplesPerBlock, sampleRate);
}

void AudioFilePlayerAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool AudioFilePlayerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
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

void AudioFilePlayerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());
    
    ReferencedTransportSourceData::Ptr ptr;
    while( fifo.pull(ptr) )
    {
        ;
    }
    
    if( ptr != nullptr )
    {
        pool.add(activeSource);
        activeSource = ptr;
        transportSource.stop();
        transportSource.setSource(activeSource->currentAudioFileSource.get(),
                                  32768,
                                  &directoryScannerBackgroundThread,
                                  activeSource->audioFileSourceSampleRate);
        sourceHasChanged.set(true);
    }
    
    AudioSourceChannelInfo asci(&buffer, 0, buffer.getNumSamples());
    transportSource.getNextAudioBlock(asci);
}

//==============================================================================
bool AudioFilePlayerAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* AudioFilePlayerAudioProcessor::createEditor()
{
    return new AudioFilePlayerAudioProcessorEditor (*this);
}

//==============================================================================
void AudioFilePlayerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    if( activeSource != nullptr )
    {
        refreshCurrentFileInAPVTS(apvts, activeSource->currentAudioFile);
        
        juce::MemoryOutputStream mos(destData, true);
        apvts.state.writeToStream(mos);
    }
}

void AudioFilePlayerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    auto tree = juce::ValueTree::readFromData(data, sizeInBytes);
    if( tree.isValid() )
    {
        apvts.replaceState(tree);
        if( auto url = apvts.state.getProperty("CurrentFile", {});
           url != var() )
        {
            File file( url.toString() );
            jassert(file.existsAsFile());
            if( file.existsAsFile() )
            {
                juce::URL path( file );
                transportSourceCreator.requestTransportForURL(path);
            }
        }
    }
}

AudioProcessorValueTreeState::ParameterLayout AudioFilePlayerAudioProcessor::createParameterLayout()
{
    AudioProcessorValueTreeState::ParameterLayout layout;
    
    using namespace Params;
//    const auto& paramNames = GetParamNames();
    
    return layout;
}
//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioFilePlayerAudioProcessor();
}
