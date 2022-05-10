/*
  ==============================================================================

    FileOperations.h
    Created: 8 May 2022 8:16:25am
    Author:  abhis

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>

bool CheckIsPermissionGranted (juce::RuntimePermissions::PermissionID id)
{
    if (!RuntimePermissions::isGranted (id))
    {
        RuntimePermissions::request (id,
        [&](bool granted) mutable
        {
            if (granted) return true;
            else return false;
        });
        return false;
    }
    else return true;
}

bool CheckIsPermissionRequired (juce::RuntimePermissions::PermissionID id)
{
    if (RuntimePermissions::isRequired (id))
    {
        return true;
    }
    return false;
}

void loadFileIntoBuffer (juce::File file, juce::AudioBuffer<float>& wavBuffer, bool isStereo)
{
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    if (file != File{})
    {
        auto* reader = formatManager.createReaderFor (file);

        if (reader != nullptr)
        {
            auto backingSource = std::make_unique<juce::AudioFormatReaderSource> (reader, true);
            wavBuffer = juce::AudioBuffer<float>((int) reader->numChannels, (int) reader->lengthInSamples);

            if (isStereo)
                reader->read (&wavBuffer, 0, (int) reader->lengthInSamples, 0, true, true);
            else
                reader->read (&wavBuffer, 0, (int) reader->lengthInSamples, 0, true, false);
        }
    }
}

void loadFileIntoBuffer (juce::String filePath, juce::AudioBuffer<float>& wavBuffer, bool isStereo)
{
    auto file = juce::File (filePath);

    loadFileIntoBuffer (file, wavBuffer, isStereo);
}

//You need to pass in a persistent reader source
void loadFileIntoTransportSource (juce::File file, juce::AudioTransportSource& source, std::unique_ptr<juce::AudioFormatReaderSource>& readerSource, bool isStereo)
{
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    if (file != File {})
    {
        auto* reader = formatManager.createReaderFor (file);

        if (reader != nullptr)
        {
            auto fileReaderSource = std::make_unique<juce::AudioFormatReaderSource> (reader, true);

            source.setSource (fileReaderSource.get(), 0, nullptr, reader->sampleRate, 2);

            readerSource.reset (fileReaderSource.release());
        }
    }
}

//This function must be OS and device independent
juce::File getParentDir()
{
    #if (JUCE_ANDROID || JUCE_IOS)
    {
        return File::getSpecialLocation (File::tempDirectory);
    }
    #else
        return File::getSpecialLocation (File::userDocumentsDirectory);
    #endif
}

//ideally overload this function for buffer, audiosamplebuffer, etc.
//This was a nightmare to set up
void WriteBufferToFile (juce::String fileName, AudioSampleBuffer& wavBuffer, bool isStereo)
{
    AudioSampleBuffer bufferToWrite (1, wavBuffer.getNumSamples());
    bufferToWrite.clear();

    
    bufferToWrite.copyFrom (0, 0, wavBuffer, 0, 0, wavBuffer.getNumSamples());

    if (isStereo)
        bufferToWrite.copyFrom (1, 0, wavBuffer, 0, 1, wavBuffer.getNumSamples());

    juce::File outputFile (getParentDir().getNonexistentChildFile (fileName, ".wav"));
    auto outputStream = outputFile.createOutputStream();
    juce::WavAudioFormat format;
    std::unique_ptr<AudioFormatWriter> streamWriter (format.createWriterFor (outputStream.get(), 44100, 1, 16, StringPairArray(), 0));
    streamWriter->writeFromAudioSampleBuffer (bufferToWrite, 0, bufferToWrite.getNumSamples());
    outputStream.release();
}




        

        //Remnant of code to load from binary data
        
        /*
        if (true)
        {
            MemoryInputStream inputStream (BinaryData::Solvage_wav, BinaryData::Solvage_wavSize, false);
            AudioFormatReader* mFormatReader = wavFormat.createReaderFor (&inputStream, true);
            backingTrackBuffer = AudioBuffer<float> (mFormatReader->numChannels, mFormatReader->lengthInSamples);
            mFormatReader->read (&backingTrackBuffer, 0, backingTrackBuffer.getNumSamples(), 0, true, true);


            if (mFormatReader != nullptr)
            {
                std::unique_ptr<AudioFormatReaderSource> backingSource = std::make_unique<juce::AudioFormatReaderSource> (mFormatReader, true);


                //Copy the backing track to its own buffer here
                backingTrackBuffer = AudioBuffer<float> (mFormatReader->numChannels, mFormatReader->lengthInSamples);
                mFormatReader->read (&backingTrackBuffer, 0, backingTrackBuffer.getNumSamples(), 0, true, true);

                transportSource.setSource (backingSource.get(), 0, nullptr, mFormatReader->sampleRate);
                readerSource.reset (backingSource.release());
            }
        }
       */ 

        //This chooser starts up at the very beginning
