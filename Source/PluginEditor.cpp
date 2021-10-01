/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

DemoThumbnailComp::DemoThumbnailComp (AudioFormatManager& formatManager,
                                      Slider& slider,
                                      AudioTransportSource& source)
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
//    transportSource.start();
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
    {
        if (! (isFollowingTransport &&
               transportSource.isPlaying()))
        {
            setRange (visibleRange.movedToStartAt (newRangeStart));
        }
    }
}

void DemoThumbnailComp::timerCallback()
{
    if (canMoveTransport())
    {
        updateCursorPosition();
    }
    else
    {
        setRange (visibleRange.movedToStartAt (transportSource.getCurrentPosition() - (visibleRange.getLength() / 2.0)));
    }
}

void DemoThumbnailComp::updateCursorPosition()
{
    currentPositionMarker.setRectangle (Rectangle<float> (timeToX (transportSource.getCurrentPosition()) - 0.75f, 0,
                                                          1.5f, (float) (getHeight() - scrollbar.getHeight())));
}
//==============================================================================
AudioFilePlayerAudioProcessorEditor::AudioFilePlayerAudioProcessorEditor(AudioFilePlayerAudioProcessor& p) :
AudioProcessorEditor (&p),
audioProcessor (p),
//transportSource(p.transportSource)
directoryList(nullptr, audioProcessor.directoryScannerBackgroundThread)
{
    addAndMakeVisible (zoomLabel);
    zoomLabel.setFont (Font (15.00f, Font::plain));
    zoomLabel.setJustificationType (Justification::centredRight);
    zoomLabel.setEditable (false, false, false);
    zoomLabel.setColour (TextEditor::textColourId, Colours::black);
    zoomLabel.setColour (TextEditor::backgroundColourId, Colour (0x00000000));
    
    addAndMakeVisible (followTransportButton);
    followTransportButton.onClick = [this] { updateFollowTransportState(); };
    
    directoryList.setDirectory (File::getSpecialLocation (File::userHomeDirectory), true, true);
    
    addAndMakeVisible (fileTreeComp);
    
    
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
    
    thumbnail.reset (new DemoThumbnailComp (audioProcessor.formatManager,
                                            zoomSlider,
                                            audioProcessor.transportSource));
    addAndMakeVisible (thumbnail.get());
    thumbnail->addChangeListener (this); //listen for dragAndDrop activities
    /*
     Problem:
        there is no means of refreshing the startStopButton when playback is started or stopped
        there is no means of refreshing the DemoThumbnailComp when the audio file changes
     Cause:
        the activeSource->transportSource is created on a background thread now
        it is not a member variable that continually exists
     
     */
//    audioProcessor.transportSource.addChangeListener(this);
    
    startStopButton.setClickingTogglesState(true);
    addAndMakeVisible (startStopButton);
    startStopButton.setColour (TextButton::buttonColourId, Colour (0xff79ed7f));
    startStopButton.setColour (TextButton::textColourOffId, Colours::black);
    startStopButton.onClick = [this] { startOrStop(); };
//    startStopButton.setEnabled( audioProcessor.transportSource.getTotalLength() > 0);
    
    
    
    startTimerHz(50);
    setOpaque (true);
    setSize (500, 500);
}

AudioFilePlayerAudioProcessorEditor::~AudioFilePlayerAudioProcessorEditor()
{
//    audioProcessor.transportSource.removeChangeListener(this);
//    transportSource  .setSource (nullptr); //TODO: figure out where this should go.
    
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
void AudioFilePlayerAudioProcessorEditor::startOrStop()
{
    auto shouldPlay = startStopButton.getToggleState();
    if( shouldPlay )
    {
        audioProcessor.transportSource.start();
    }
    else
    {
        audioProcessor.transportSource.stop();
    }
}

void AudioFilePlayerAudioProcessorEditor::updateFollowTransportState()
{
    thumbnail->setFollowsTransport (followTransportButton.getToggleState());
}

void AudioFilePlayerAudioProcessorEditor::selectionChanged()
{
    audioProcessor.transportSourceCreator.requestTransportForURL(URL (fileTreeComp.getSelectedFile()));
}

void AudioFilePlayerAudioProcessorEditor::fileClicked (const File&, const MouseEvent&)          {}
void AudioFilePlayerAudioProcessorEditor::fileDoubleClicked (const File&)                       {}
void AudioFilePlayerAudioProcessorEditor::browserRootChanged (const File&)                      {}

void AudioFilePlayerAudioProcessorEditor::changeListenerCallback (ChangeBroadcaster* source)
{
    if (source == thumbnail.get())
    {
        audioProcessor.transportSourceCreator.requestTransportForURL(URL(thumbnail->getLastDroppedFile()));
    }
}

void AudioFilePlayerAudioProcessorEditor::timerCallback()
{
    if( audioProcessor.sourceHasChanged.compareAndSetBool(false, true) )
    {
        auto& src = audioProcessor.activeSource; //a local copy.  causes a data race, which is weird because activeSource is reference counted, and the reference counting is atomic..
        bool hasValidSource = src.get() != nullptr;
        if( hasValidSource )
        {
            if( src.get() != activeSource.get() )
            {
                //we have a new source!
                //update the file path in the APVTS.
                //update the thumbnail.
                AudioFilePlayerAudioProcessor::refreshCurrentFileInAPVTS(audioProcessor.apvts, src->currentAudioFile);
                activeSource = src;
                
                zoomSlider.setValue (0, dontSendNotification);
            
                thumbnail->setURL (activeSource->currentAudioFile);
            }
        }

        startStopButton.setEnabled( hasValidSource );
    }
    
    //update the startStopButton
    auto isPlaying = audioProcessor.transportSource.isPlaying();
    if( audioProcessor.transportSource.getTotalLength() > 0 )
        startStopButton.setButtonText( ! isPlaying ? "Start" : "Stop" );
    
    startStopButton.setToggleState(isPlaying, dontSendNotification);
}
