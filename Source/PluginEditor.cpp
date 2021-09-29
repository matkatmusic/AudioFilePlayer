/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

DemoThumbnailComp::DemoThumbnailComp (AudioFormatManager& formatManager,
                   AudioTransportSource& source,
                   Slider& slider)
: transportSource (source),
zoomSlider (slider),
thumbnail (512, formatManager, thumbnailCache)
{
    thumbnail.addChangeListener (this);
    
    addAndMakeVisible (scrollbar);
    scrollbar.setRangeLimits (visibleRange);
    scrollbar.setAutoHide (false);
    scrollbar.addListener (this);
    
    currentPositionMarker.setFill (Colours::white.withAlpha (0.85f));
    addAndMakeVisible (currentPositionMarker);
}

DemoThumbnailComp::~DemoThumbnailComp()
{
    scrollbar.removeListener (this);
    thumbnail.removeChangeListener (this);
}

void DemoThumbnailComp::setURL (const URL& url)
{
    InputSource* inputSource = nullptr;
    
#if ! JUCE_IOS
    if (url.isLocalFile())
    {
        inputSource = new FileInputSource (url.getLocalFile());
    }
    else
#endif
    {
        if (inputSource == nullptr)
            inputSource = new URLInputSource (url);
    }
    
    if (inputSource != nullptr)
    {
        thumbnail.setSource (inputSource);
        
        Range<double> newRange (0.0, thumbnail.getTotalLength());
        scrollbar.setRangeLimits (newRange);
        setRange (newRange);
        
        startTimerHz (40);
    }
}

URL DemoThumbnailComp::getLastDroppedFile() const noexcept { return lastFileDropped; }

void DemoThumbnailComp::setZoomFactor (double amount)
{
    if (thumbnail.getTotalLength() > 0)
    {
        auto newScale = jmax (0.001, thumbnail.getTotalLength() * (1.0 - jlimit (0.0, 0.99, amount)));
        auto timeAtCentre = xToTime ((float) getWidth() / 2.0f);
        
        setRange ({ timeAtCentre - newScale * 0.5, timeAtCentre + newScale * 0.5 });
    }
}

void DemoThumbnailComp::setRange (Range<double> newRange)
{
    visibleRange = newRange;
    scrollbar.setCurrentRange (visibleRange);
    updateCursorPosition();
    repaint();
}

void DemoThumbnailComp::setFollowsTransport (bool shouldFollow)
{
    isFollowingTransport = shouldFollow;
}

void DemoThumbnailComp::paint (Graphics& g)
{
    g.fillAll (Colours::darkgrey);
    g.setColour (Colours::lightblue);
    
    if (thumbnail.getTotalLength() > 0.0)
    {
        auto thumbArea = getLocalBounds();
        
        thumbArea.removeFromBottom (scrollbar.getHeight() + 4);
        thumbnail.drawChannels (g, thumbArea.reduced (2),
                                visibleRange.getStart(), visibleRange.getEnd(), 1.0f);
    }
    else
    {
        g.setFont (14.0f);
        g.drawFittedText ("(No audio file selected)", getLocalBounds(), Justification::centred, 2);
    }
}

void DemoThumbnailComp::resized()
{
    scrollbar.setBounds (getLocalBounds().removeFromBottom (14).reduced (2));
}

void DemoThumbnailComp::changeListenerCallback (ChangeBroadcaster*)
{
    // this method is called by the thumbnail when it has changed, so we should repaint it..
    repaint();
}

bool DemoThumbnailComp::isInterestedInFileDrag (const StringArray& /*files*/)
{
    return true;
}

void DemoThumbnailComp::filesDropped (const StringArray& files, int /*x*/, int /*y*/)
{
    lastFileDropped = URL (File (files[0]));
    sendChangeMessage();
}

void DemoThumbnailComp::mouseDown (const MouseEvent& e)
{
    mouseDrag (e);
}

void DemoThumbnailComp::mouseDrag (const MouseEvent& e)
{
    if (canMoveTransport())
        transportSource.setPosition (jmax (0.0, xToTime ((float) e.x)));
}

void DemoThumbnailComp::mouseUp (const MouseEvent&)
{
    transportSource.start();
}

void DemoThumbnailComp::mouseWheelMove (const MouseEvent&, const MouseWheelDetails& wheel)
{
    if (thumbnail.getTotalLength() > 0.0)
    {
        auto newStart = visibleRange.getStart() - wheel.deltaX * (visibleRange.getLength()) / 10.0;
        newStart = jlimit (0.0, jmax (0.0, thumbnail.getTotalLength() - (visibleRange.getLength())), newStart);
        
        if (canMoveTransport())
            setRange ({ newStart, newStart + visibleRange.getLength() });
        
        if (wheel.deltaY != 0.0f)
            zoomSlider.setValue (zoomSlider.getValue() - wheel.deltaY);
        
        repaint();
    }
}

float DemoThumbnailComp::timeToX (const double time) const
{
    if (visibleRange.getLength() <= 0)
        return 0;
    
    return (float) getWidth() * (float) ((time - visibleRange.getStart()) / visibleRange.getLength());
}

double DemoThumbnailComp::xToTime (const float x) const
{
    return (x / (float) getWidth()) * (visibleRange.getLength()) + visibleRange.getStart();
}

bool DemoThumbnailComp::canMoveTransport() const noexcept
{
    return ! (isFollowingTransport && transportSource.isPlaying());
}

void DemoThumbnailComp::scrollBarMoved (ScrollBar* scrollBarThatHasMoved, double newRangeStart)
{
    if (scrollBarThatHasMoved == &scrollbar)
        if (! (isFollowingTransport && transportSource.isPlaying()))
            setRange (visibleRange.movedToStartAt (newRangeStart));
}

void DemoThumbnailComp::timerCallback()
{
    if (canMoveTransport())
        updateCursorPosition();
    else
        setRange (visibleRange.movedToStartAt (transportSource.getCurrentPosition() - (visibleRange.getLength() / 2.0)));
}

void DemoThumbnailComp::updateCursorPosition()
{
    currentPositionMarker.setVisible (transportSource.isPlaying() || isMouseButtonDown());
    
    currentPositionMarker.setRectangle (Rectangle<float> (timeToX (transportSource.getCurrentPosition()) - 0.75f, 0,
                                                          1.5f, (float) (getHeight() - scrollbar.getHeight())));
}
//==============================================================================
AudioFilePlayerAudioProcessorEditor::AudioFilePlayerAudioProcessorEditor(AudioFilePlayerAudioProcessor& p) :
AudioProcessorEditor (&p),
audioProcessor (p),
transportSource(p.transportSource)
{
    addAndMakeVisible (zoomLabel);
    zoomLabel.setFont (Font (15.00f, Font::plain));
    zoomLabel.setJustificationType (Justification::centredRight);
    zoomLabel.setEditable (false, false, false);
    zoomLabel.setColour (TextEditor::textColourId, Colours::black);
    zoomLabel.setColour (TextEditor::backgroundColourId, Colour (0x00000000));
    
    addAndMakeVisible (followTransportButton);
    followTransportButton.onClick = [this] { updateFollowTransportState(); };
    
    
    addAndMakeVisible (fileTreeComp);
    
    directoryList.setDirectory (File::getSpecialLocation (File::userHomeDirectory), true, true);
    
    fileTreeComp.setColour (FileTreeComponent::backgroundColourId, Colours::lightgrey.withAlpha (0.6f));
    fileTreeComp.addListener (this);
    
    addAndMakeVisible (explanation);
    explanation.setFont (Font (14.00f, Font::plain));
    explanation.setJustificationType (Justification::bottomRight);
    explanation.setEditable (false, false, false);
    explanation.setColour (TextEditor::textColourId, Colours::black);
    explanation.setColour (TextEditor::backgroundColourId, Colour (0x00000000));
    
    addAndMakeVisible (zoomSlider);
    zoomSlider.setRange (0, 1, 0);
    zoomSlider.onValueChange = [this] { thumbnail->setZoomFactor (zoomSlider.getValue()); };
    zoomSlider.setSkewFactor (2);
    
    thumbnail.reset (new DemoThumbnailComp (formatManager, transportSource, zoomSlider));
    addAndMakeVisible (thumbnail.get());
    thumbnail->addChangeListener (this);
    audioProcessor.transportSource.addChangeListener(this);
    
    startStopButton.setClickingTogglesState(true);
    addAndMakeVisible (startStopButton);
    startStopButton.setColour (TextButton::buttonColourId, Colour (0xff79ed7f));
    startStopButton.setColour (TextButton::textColourOffId, Colours::black);
    startStopButton.onClick = [this] { startOrStop(); };
    startStopButton.setEnabled( audioProcessor.transportSource.getTotalLength() > 0);
    
    // audio setup
    formatManager.registerBasicFormats();
    
    directoryScannerBackgroundThread.startThread (3);
    
    setOpaque (true);
    setSize (500, 500);
}

AudioFilePlayerAudioProcessorEditor::~AudioFilePlayerAudioProcessorEditor()
{
    audioProcessor.transportSource.removeChangeListener(this);
    transportSource  .setSource (nullptr); //TODO: figure out where this should go.
    
    fileTreeComp.removeListener (this);
    
    thumbnail->removeChangeListener (this);
}

void AudioFilePlayerAudioProcessorEditor::paint (Graphics& g)
{
    g.fillAll (getUIColourIfAvailable (LookAndFeel_V4::ColourScheme::UIColour::windowBackground));
}

void AudioFilePlayerAudioProcessorEditor::resized()
{
    auto r = getLocalBounds().reduced (4);
    
    auto controls = r.removeFromBottom (90);
    
    auto controlRightBounds = controls.removeFromRight (controls.getWidth() / 3);
    
    
    explanation.setBounds (controlRightBounds);
    
    auto zoom = controls.removeFromTop (25);
    zoomLabel .setBounds (zoom.removeFromLeft (50));
    zoomSlider.setBounds (zoom);
    
    followTransportButton.setBounds (controls.removeFromTop (25));
    startStopButton      .setBounds (controls);
    
    r.removeFromBottom (6);
    
    
    thumbnail->setBounds (r.removeFromBottom (140));
    r.removeFromBottom (6);
    
    fileTreeComp.setBounds (r);
}

//==============================================================================
void AudioFilePlayerAudioProcessorEditor::showAudioResource (URL resource)
{
    auto successfullyLoaded = loadURLIntoTransport(resource);
    if( successfullyLoaded )
    {
        currentAudioFile = std::move (resource);
    }
    
    startStopButton.setEnabled(successfullyLoaded);
    startStopButton.setButtonText( successfullyLoaded ? "Start" : "Stop" );
    
    zoomSlider.setValue (0, dontSendNotification);
    thumbnail->setURL (currentAudioFile);
}

bool AudioFilePlayerAudioProcessorEditor::loadURLIntoTransport (const URL& audioURL)
{
    // unload the previous file source and delete it..
    transportSource.stop();
    transportSource.setSource (nullptr);
    currentAudioFileSource.reset();
    
    std::unique_ptr<AudioFormatReader> reader;
    
    if (audioURL.isLocalFile())
    {
        reader.reset(formatManager.createReaderFor (audioURL.getLocalFile()));
    }
    else
    {
        reader.reset(formatManager.createReaderFor (audioURL.createInputStream (false)));
    }
    
    if (reader != nullptr)
    {
        auto sampleRate = reader->sampleRate;
        currentAudioFileSource.reset (new AudioFormatReaderSource (reader.release(), true));
        
        // ..and plug it into our transport source
        transportSource.setSource (currentAudioFileSource.get(),
                                   32768,                   // tells it to buffer this many samples ahead
                                   &directoryScannerBackgroundThread,                 // this is the background thread to use for reading-ahead
                                   sampleRate);     // allows for sample rate correction
        
        return true;
    }
    
    return false;
}

/*
 migrate this to the audio processor
 the startStopButton should be attached to an AudioparameterBool
 */
void AudioFilePlayerAudioProcessorEditor::startOrStop()
{
    audioProcessor.transportIsPlaying.set( startStopButton.getToggleState() );
}

void AudioFilePlayerAudioProcessorEditor::updateFollowTransportState()
{
    thumbnail->setFollowsTransport (followTransportButton.getToggleState());
}


void AudioFilePlayerAudioProcessorEditor::selectionChanged()
{
    showAudioResource (URL (fileTreeComp.getSelectedFile()));
}

void AudioFilePlayerAudioProcessorEditor::fileClicked (const File&, const MouseEvent&)          {}
void AudioFilePlayerAudioProcessorEditor::fileDoubleClicked (const File&)                       {}
void AudioFilePlayerAudioProcessorEditor::browserRootChanged (const File&)                      {}

void AudioFilePlayerAudioProcessorEditor::changeListenerCallback (ChangeBroadcaster* source)
{
    if (source == thumbnail.get())
    {
        showAudioResource (URL (thumbnail->getLastDroppedFile()));
    }
    else if( source == &audioProcessor.transportSource )
    {
        auto isPlaying = audioProcessor.transportSource.isPlaying();
        startStopButton.setButtonText( isPlaying ? "Stop" : "Start" );
        startStopButton.setEnabled( audioProcessor.transportSource.getTotalLength() > 0);
    }
}
