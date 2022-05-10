#pragma once

#include "FileOperations.h"
#include "RecordingUtilities.h"
#include "DemoUtilities.h"
#include "AudioLiveScrollingDisplay.h"
#include <random>

//==============================================================================
class AudioRecordingDemo : public juce::AudioAppComponent
{
    juce::AudioTransportSource transportSource;
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    AudioBuffer<float> recordingVoiceOnlyBuffer;

public:
    
    //This function records the VoiceOnly Track to a file called RecordingVoiceOnly.wav
    //It also sets the transportSource to point to the backing track.
    void startRecording()
    {
        //If permission to write to external storage hasn't been granted, there really isn't much we can do.
        //The best thing to do is to return from the startRecording() function
        if (!CheckIsPermissionGranted (juce::RuntimePermissions::writeExternalStorage))
        {
            return;
        }

        //start playing the backing track
        transportSource.start();

        recordingVoiceOnly = getParentDir().getNonexistentChildFile ("RecordingVoiceOnly", ".wav");
        recordingVoiceAndMusic = getParentDir().getNonexistentChildFile ("RecordingVoiceAndMusic", ".wav");

        //outputWavFile means backingTrackOnly file
        outputWavFile = getParentDir().getNonexistentChildFile ("BackingTrackOnly", ".wav");

        //The recorder starts recording from the input stream into the file RecordingVoiceOnly.wav
        recorder.startRecording (recordingVoiceOnly);

        recordButton.setButtonText ("Stop");
        recordingThumbnail.setDisplayFullThumbnail (false);
    }

    void stopRecording()
    {
        recorder.stop();

        //stop playing the backing track
        transportSource.stop();

       #if JUCE_CONTENT_SHARING
        SafePointer<AudioRecordingDemo> safeThis (this);
        File fileToShare = lastRecording;

        ContentSharer::getInstance()->shareFiles (Array<URL> ({URL (fileToShare)}),
                                                  [safeThis, fileToShare] (bool success, const String& error)
                                                  {
                                                      if (fileToShare.existsAsFile())
                                                          fileToShare.deleteFile();

                                                      if (! success && error.isNotEmpty())
                                                          NativeMessageBox::showAsync (MessageBoxOptions()
                                                                                         .withIconType (MessageBoxIconType::WarningIcon)
                                                                                         .withTitle ("Sharing Error")
                                                                                         .withMessage (error),
                                                                                       nullptr);
                                                  });
       #endif

        recordButton.setButtonText ("Record");
        recordingThumbnail.setDisplayFullThumbnail (true);
        
        //read RecordingVoiceOnly file
        loadFileIntoBuffer (recordingVoiceOnly, recordingVoiceOnlyBuffer, false);

        //Mix buffers here
        wavBuffer.addFrom (0, 0, recordingVoiceOnlyBuffer.getWritePointer (0), recordingVoiceOnlyBuffer.getNumSamples(), 1.0f);

        if (recordingVoiceOnlyBuffer.getNumChannels()>1)
            wavBuffer.addFrom (1, 0, recordingVoiceOnlyBuffer.getWritePointer (0), recordingVoiceOnlyBuffer.getNumSamples(), 1.0f);

        WriteBufferToFile ("RecordingVoiceAndMusic", wavBuffer, false);
    }

    //==============================================================================
    AudioRecordingDemo()
    {
        setOpaque (true);
        addAndMakeVisible (liveAudioScroller);

        addAndMakeVisible (explanationLabel);
        explanationLabel.setFont(Font(15.0f, Font::plain));
        explanationLabel.setJustificationType(Justification::topLeft);
        explanationLabel.setEditable(false, false, false);
        explanationLabel.setColour(TextEditor::textColourId, Colours::black);
        explanationLabel.setColour(TextEditor::backgroundColourId, Colour(0x00000000));

        addAndMakeVisible (recordButton);
        recordButton.setColour(TextButton::buttonColourId, Colour(0xffff5c5c));
        recordButton.setColour(TextButton::textColourOnId, Colours::black);

        recordButton.onClick = [this]
        {
            if (recorder.isRecording())
                stopRecording();
            else
                startRecording();
        };

        transportSource.start();

        addAndMakeVisible (recordingThumbnail);

#ifndef JUCE_DEMO_RUNNER
        RuntimePermissions::request (RuntimePermissions::recordAudio,
            [this] (bool granted)
            {
                int numInputChannels = granted ? 2 : 0;
                audioDeviceManager.initialise (numInputChannels, 2, nullptr, true, {}, nullptr);
            });
#endif

        audioDeviceManager.addAudioCallback (&liveAudioScroller);
        audioDeviceManager.addAudioCallback (&recorder);
   
        chooser = std::make_unique<juce::FileChooser> ("Select a Wave file to play...",
            juce::File{},
            "*.wav");
        auto chooserFlags = juce::FileBrowserComponent::openMode
            | juce::FileBrowserComponent::canSelectFiles;


        if (CheckIsPermissionGranted (juce::RuntimePermissions::readExternalStorage))
            chooser->launchAsync (chooserFlags, [this](const FileChooser& fc)
            { 
                loadFileIntoBuffer (fc.getResult(), wavBuffer, false);     
                loadFileIntoTransportSource (fc.getResult(), transportSource, readerSource, false); 
            });

        // Some platforms require permissions to open input channels so request that here
        if (CheckIsPermissionRequired (juce::RuntimePermissions::recordAudio))
            if (!CheckIsPermissionGranted (juce::RuntimePermissions::recordAudio))
            {
                setAudioChannels (2, 2);
            }
            else setAudioChannels (0, 2);

        else
        {
            // Specify the number of input and output channels that we want to open
            setAudioChannels (2, 2);
        }

        setSize (500, 500);
    }

    ~AudioRecordingDemo() override
    {
        shutdownAudio();
        transportSource.setSource (nullptr);
        audioDeviceManager.removeAudioCallback (&recorder);
        audioDeviceManager.removeAudioCallback (&liveAudioScroller);
    }

    WavAudioFormat wavFormat;

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override
    {
        transportSource.prepareToPlay (samplesPerBlockExpected, sampleRate);
    }

    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override
    {
        auto* device = deviceManager.getCurrentAudioDevice();
        auto activeInputChannels = device->getActiveInputChannels();
        auto activeOutputChannels = device->getActiveOutputChannels();
        auto maxInputChannels = activeInputChannels.getHighestBit() + 1;
        auto maxOutputChannels = activeOutputChannels.getHighestBit() + 1;
        auto level = 1.0f;

        AudioBuffer<float> buffer;
        buffer.makeCopyOf (*bufferToFill.buffer, false);

        //This adds the contents of the backing track to the real-time buffer
        transportSource.getNextAudioBlock (bufferToFill);

        //This captures input audio into "buffer"
        for (auto channel = 0; channel < maxOutputChannels; ++channel)
        {
            if ((!activeOutputChannels[channel]) || maxInputChannels == 0)
            {
                buffer.clear (channel, bufferToFill.startSample, bufferToFill.numSamples);
            }
            else
            {
                auto actualInputChannel = channel % maxInputChannels;

                if (!activeInputChannels[channel])
                {
                    buffer.clear (channel, bufferToFill.startSample, bufferToFill.numSamples);
                }
                else
                {
                    auto* inBuffer = buffer.getReadPointer(actualInputChannel,
                        bufferToFill.startSample);
                    auto* outBuffer = buffer.getWritePointer(channel, bufferToFill.startSample);

                    for (auto sample = 0; sample < bufferToFill.numSamples; ++sample)
                        outBuffer[sample] += inBuffer[sample] * level;
                }
            }
        }

        //This adds the contents of input audio to the backing track.
        bufferToFill.buffer->addFrom (0, 0, buffer.getWritePointer (0), buffer.getNumSamples(), 1.0f);
        bufferToFill.buffer->addFrom (1, 0, buffer.getWritePointer (0), buffer.getNumSamples(), 1.0f);
    }

    void releaseResources() override
    {
        transportSource.releaseResources();
    }

    void paint (Graphics& g) override
    {
        g.fillAll (getUIColourIfAvailable (LookAndFeel_V4::ColourScheme::UIColour::windowBackground));
    }

    void resized() override
    {
        auto area = getLocalBounds();

        liveAudioScroller.setBounds  (area.removeFromTop(80).reduced(8));
        recordingThumbnail.setBounds (area.removeFromTop(80).reduced(8));
        recordButton.setBounds       (area.removeFromTop(36).removeFromLeft(140).reduced(8));
        explanationLabel.setBounds   (area.reduced(8));
    }

private:
    // if this PIP is running inside the demo runner, we'll use the shared device manager instead
#ifndef JUCE_DEMO_RUNNER
    AudioDeviceManager audioDeviceManager;
#else
    AudioDeviceManager& audioDeviceManager { getSharedAudioDeviceManager(1, 0) };
#endif

    LiveScrollingAudioDisplay liveAudioScroller;
    RecordingThumbnail recordingThumbnail;
    AudioRecorder recorder { recordingThumbnail.getAudioThumbnail() };

    Label explanationLabel { {}, "This page demonstrates how to record a wave file from the live audio input..\n\n"
                                 #if (JUCE_ANDROID || JUCE_IOS)
                                  "After you are done with your recording you can share with other apps."
                                 #else
                                  "Pressing record will start recording a file in your \"Documents\" folder."
                                 #endif
    };
    TextButton recordButton { "Record" };
    File recordingVoiceOnly;
    File recordingVoiceAndMusic;
    AudioBuffer<float> wavBuffer;
    juce::File outputWavFile;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioRecordingDemo)
};
