/*
 ==============================================================================
 
 This file contains the basic framework code for a JUCE plugin editor.
 
 ==============================================================================
 */

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

using namespace juce;

inline Colour getUIColourIfAvailable (LookAndFeel_V4::ColourScheme::UIColour uiColour, Colour fallback = Colour (0xff4d4d4d)) noexcept
{
    if (auto* v4 = dynamic_cast<LookAndFeel_V4*> (&LookAndFeel::getDefaultLookAndFeel()))
        return v4->getCurrentColourScheme().getUIColour (uiColour);
    
    return fallback;
}

class DemoThumbnailComp  : public Component,
public ChangeListener,
public FileDragAndDropTarget,
public ChangeBroadcaster,
private ScrollBar::Listener,
private Timer
{
public:
    DemoThumbnailComp (AudioFormatManager& formatManager,
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
    
    ~DemoThumbnailComp() override
    {
        scrollbar.removeListener (this);
        thumbnail.removeChangeListener (this);
    }
    
    void setURL (const URL& url)
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
    
    URL getLastDroppedFile() const noexcept { return lastFileDropped; }
    
    void setZoomFactor (double amount)
    {
        if (thumbnail.getTotalLength() > 0)
        {
            auto newScale = jmax (0.001, thumbnail.getTotalLength() * (1.0 - jlimit (0.0, 0.99, amount)));
            auto timeAtCentre = xToTime ((float) getWidth() / 2.0f);
            
            setRange ({ timeAtCentre - newScale * 0.5, timeAtCentre + newScale * 0.5 });
        }
    }
    
    void setRange (Range<double> newRange)
    {
        visibleRange = newRange;
        scrollbar.setCurrentRange (visibleRange);
        updateCursorPosition();
        repaint();
    }
    
    void setFollowsTransport (bool shouldFollow)
    {
        isFollowingTransport = shouldFollow;
    }
    
    void paint (Graphics& g) override
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
    
    void resized() override
    {
        scrollbar.setBounds (getLocalBounds().removeFromBottom (14).reduced (2));
    }
    
    void changeListenerCallback (ChangeBroadcaster*) override
    {
        // this method is called by the thumbnail when it has changed, so we should repaint it..
        repaint();
    }
    
    bool isInterestedInFileDrag (const StringArray& /*files*/) override
    {
        return true;
    }
    
    void filesDropped (const StringArray& files, int /*x*/, int /*y*/) override
    {
        lastFileDropped = URL (File (files[0]));
        sendChangeMessage();
    }
    
    void mouseDown (const MouseEvent& e) override
    {
        mouseDrag (e);
    }
    
    void mouseDrag (const MouseEvent& e) override
    {
        if (canMoveTransport())
            transportSource.setPosition (jmax (0.0, xToTime ((float) e.x)));
    }
    
    void mouseUp (const MouseEvent&) override
    {
        transportSource.start();
    }
    
    void mouseWheelMove (const MouseEvent&, const MouseWheelDetails& wheel) override
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
    
    
private:
    AudioTransportSource& transportSource;
    Slider& zoomSlider;
    ScrollBar scrollbar  { false };
    
    AudioThumbnailCache thumbnailCache  { 5 };
    AudioThumbnail thumbnail;
    Range<double> visibleRange;
    bool isFollowingTransport = false;
    URL lastFileDropped;
    
    DrawableRectangle currentPositionMarker;
    
    float timeToX (const double time) const
    {
        if (visibleRange.getLength() <= 0)
            return 0;
        
        return (float) getWidth() * (float) ((time - visibleRange.getStart()) / visibleRange.getLength());
    }
    
    double xToTime (const float x) const
    {
        return (x / (float) getWidth()) * (visibleRange.getLength()) + visibleRange.getStart();
    }
    
    bool canMoveTransport() const noexcept
    {
        return ! (isFollowingTransport && transportSource.isPlaying());
    }
    
    void scrollBarMoved (ScrollBar* scrollBarThatHasMoved, double newRangeStart) override
    {
        if (scrollBarThatHasMoved == &scrollbar)
            if (! (isFollowingTransport && transportSource.isPlaying()))
                setRange (visibleRange.movedToStartAt (newRangeStart));
    }
    
    void timerCallback() override
    {
        if (canMoveTransport())
            updateCursorPosition();
        else
            setRange (visibleRange.movedToStartAt (transportSource.getCurrentPosition() - (visibleRange.getLength() / 2.0)));
    }
    
    void updateCursorPosition()
    {
        currentPositionMarker.setVisible (transportSource.isPlaying() || isMouseButtonDown());
        
        currentPositionMarker.setRectangle (Rectangle<float> (timeToX (transportSource.getCurrentPosition()) - 0.75f, 0,
                                                              1.5f, (float) (getHeight() - scrollbar.getHeight())));
    }
};

class AudioFilePlayerAudioProcessorEditor  : public juce::AudioProcessorEditor,
private FileBrowserListener,
private ChangeListener
{
public:
    AudioFilePlayerAudioProcessorEditor(AudioFilePlayerAudioProcessor& p) :
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
        
        addAndMakeVisible (startStopButton);
        startStopButton.setColour (TextButton::buttonColourId, Colour (0xff79ed7f));
        startStopButton.setColour (TextButton::textColourOffId, Colours::black);
        startStopButton.onClick = [this] { startOrStop(); };
        
        // audio setup
        formatManager.registerBasicFormats();
        
        thread.startThread (3);
        
        setOpaque (true);
        setSize (500, 500);
    }
    
    ~AudioFilePlayerAudioProcessorEditor() override
    {
        transportSource  .setSource (nullptr);
        
        fileTreeComp.removeListener (this);
        
        thumbnail->removeChangeListener (this);
    }
    
    void paint (Graphics& g) override
    {
        g.fillAll (getUIColourIfAvailable (LookAndFeel_V4::ColourScheme::UIColour::windowBackground));
    }
    
    void resized() override
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
    
private:
    // if this PIP is running inside the demo runner, we'll use the shared device manager instead
    AudioFilePlayerAudioProcessor& audioProcessor;
    AudioFormatManager formatManager;
    TimeSliceThread thread  { "audio file preview" };
    
    
    DirectoryContentsList directoryList {nullptr, thread};
    FileTreeComponent fileTreeComp {directoryList};
    Label explanation { {}, "Select an audio file in the treeview above, and this page will display its waveform, and let you play it.." };
    
    URL currentAudioFile;
    //    AudioSourcePlayer audioSourcePlayer;
    AudioTransportSource& transportSource;
    std::unique_ptr<AudioFormatReaderSource> currentAudioFileSource;
    
    std::unique_ptr<DemoThumbnailComp> thumbnail;
    Label zoomLabel                     { {}, "zoom:" };
    Slider zoomSlider                   { Slider::LinearHorizontal, Slider::NoTextBox };
    ToggleButton followTransportButton  { "Follow Transport" };
    TextButton startStopButton          { "Play/Stop" };
    
    //==============================================================================
    void showAudioResource (URL resource)
    {
        if (loadURLIntoTransport (resource))
            currentAudioFile = std::move (resource);
        
        zoomSlider.setValue (0, dontSendNotification);
        thumbnail->setURL (currentAudioFile);
    }
    
    bool loadURLIntoTransport (const URL& audioURL)
    {
        // unload the previous file source and delete it..
        transportSource.stop();
        transportSource.setSource (nullptr);
        currentAudioFileSource.reset();
        
        AudioFormatReader* reader = nullptr;
        
        if (audioURL.isLocalFile())
        {
            reader = formatManager.createReaderFor (audioURL.getLocalFile());
        }
        else
        {
            if (reader == nullptr)
                reader = formatManager.createReaderFor (audioURL.createInputStream (false));
        }
        
        if (reader != nullptr)
        {
            currentAudioFileSource.reset (new AudioFormatReaderSource (reader, true));
            
            // ..and plug it into our transport source
            transportSource.setSource (currentAudioFileSource.get(),
                                       32768,                   // tells it to buffer this many samples ahead
                                       &thread,                 // this is the background thread to use for reading-ahead
                                       reader->sampleRate);     // allows for sample rate correction
            
            return true;
        }
        
        return false;
    }
    
    void startOrStop()
    {
        if (transportSource.isPlaying())
        {
            transportSource.stop();
        }
        else
        {
            transportSource.setPosition (0);
            transportSource.start();
        }
    }
    
    void updateFollowTransportState()
    {
        thumbnail->setFollowsTransport (followTransportButton.getToggleState());
    }
    
    
    void selectionChanged() override
    {
        showAudioResource (URL (fileTreeComp.getSelectedFile()));
    }
    
    void fileClicked (const File&, const MouseEvent&) override          {}
    void fileDoubleClicked (const File&) override                       {}
    void browserRootChanged (const File&) override                      {}
    
    void changeListenerCallback (ChangeBroadcaster* source) override
    {
        if (source == thumbnail.get())
            showAudioResource (URL (thumbnail->getLastDroppedFile()));
    }
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioFilePlayerAudioProcessorEditor)
};

//==============================================================================
/**
 */

