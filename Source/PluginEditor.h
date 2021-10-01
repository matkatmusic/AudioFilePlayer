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
                       Slider& slider,
                       AudioTransportSource& source);
    
    ~DemoThumbnailComp() override;
    
    void setURL (const URL& url);
    
    URL getLastDroppedFile() const noexcept;
    
    void setZoomFactor (double amount);
    
    void setRange (Range<double> newRange);
    
    void setFollowsTransport (bool shouldFollow);
    
    void paint (Graphics& g) override;
    
    void resized() override;
    
    void changeListenerCallback (ChangeBroadcaster*) override;
    
    bool isInterestedInFileDrag (const StringArray& /*files*/) override;
    
    void filesDropped (const StringArray& files, int /*x*/, int /*y*/) override;
    
    void mouseDown (const MouseEvent& e) override;
    
    void mouseDrag (const MouseEvent& e) override;
    
    void mouseUp (const MouseEvent&) override;
    
    void mouseWheelMove (const MouseEvent&, const MouseWheelDetails& wheel) override;
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
    
    float timeToX (const double time) const;
    
    double xToTime (const float x) const;
    
    bool canMoveTransport() const noexcept;
    
    void scrollBarMoved (ScrollBar* scrollBarThatHasMoved, double newRangeStart) override;
    
    void timerCallback() override;
    
    void updateCursorPosition();
};

class AudioFilePlayerAudioProcessorEditor  : public juce::AudioProcessorEditor,
private FileBrowserListener,
private ChangeListener,
public Timer
{
public:
    AudioFilePlayerAudioProcessorEditor(AudioFilePlayerAudioProcessor& p);
    
    ~AudioFilePlayerAudioProcessorEditor() override;
    
    void paint (Graphics& g) override;
    
    void resized() override;
    
    void timerCallback() override;
private:
    // if this PIP is running inside the demo runner, we'll use the shared device manager instead
    AudioFilePlayerAudioProcessor& audioProcessor;
    
    DirectoryContentsList directoryList;
    FileTreeComponent fileTreeComp {directoryList};
    Label explanation { {}, "Select an audio file in the treeview above, and this page will display its waveform, and let you play it.." };
    
    /*
     find the code that configures this
     */
//    AudioTransportSource& transportSource;
//    std::unique_ptr<AudioFormatReaderSource> currentAudioFileSource;
    
    std::unique_ptr<DemoThumbnailComp> thumbnail;
    Label zoomLabel                     { {}, "zoom:" };
    Slider zoomSlider                   { Slider::LinearHorizontal, Slider::NoTextBox };
    ToggleButton followTransportButton  { "Follow Transport" };
    TextButton startStopButton          { "Load an audio file first..." };
    
    ReferencedTransportSourceData::Ptr activeSource;
    
    //==============================================================================
    void startOrStop();
    
    void updateFollowTransportState();
    
    
    void selectionChanged() override;
    
    void fileClicked (const File&, const MouseEvent&) override;
    void fileDoubleClicked (const File&) override;
    void browserRootChanged (const File&) override;
    
    void changeListenerCallback (ChangeBroadcaster* source) override;
    
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioFilePlayerAudioProcessorEditor)
};

//==============================================================================
/**
 */

