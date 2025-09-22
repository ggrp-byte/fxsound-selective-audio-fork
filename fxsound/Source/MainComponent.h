/*
  ==============================================================================

    This file was auto-generated!

  ==============================================================================
*/

#pragma once

#include "../JuceLibraryCode/JuceHeader.h"
#include "GUI/AnnouncementComponent.h"
#include "GUI/UITheme.h"
#include "GUI/FxMinView.h"
#include "GUI/FxExpandedView.h"
#include "Utils/Settings/Settings.h"

// --- START OF SIMULATED MODIFICATIONS ---
#include <vector>
#include <string>
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audiopolicy.h>
#include <psapi.h>

// Forward declarations for WASAPI COM interfaces
struct IMMDeviceEnumerator;
struct IMMDevice;
struct IAudioSessionManager2;
struct IAudioSessionEnumerator;
struct IAudioSessionControl;
struct IAudioSessionControl2;

// Structure to hold audio session information
struct AudioSessionInfo
{
    DWORD pid;
    juce::String processName;
    juce::String sessionId;
    // Add more properties as needed, e.g., volume, mute state
};

// --- END OF SIMULATED MODIFICATIONS ---

//==============================================================================
/*
    This component lives inside our window, and this is where you should put all
    your controls and content.
*/
constexpr wchar_t CHECK_ANNOUNCEMENT_URL[] = L"https://www.fxsound.com/tmp/check_announcement.json";
constexpr char ATTRIB_URL[] = "url";
constexpr char ATTRIB_WIDTH[] = "width";
constexpr char ATTRIB_HEIGHT[] = "height";

class MainComponent : public Component, public FxView::ResizeViewListener
{
public:
    //==============================================================================
    MainComponent(const String& name);
    ~MainComponent();

    //==============================================================================
	void paint(Graphics& g) override;
	void resized() override;

	void mouseDown(const MouseEvent& e) override;
	void mouseDrag(const MouseEvent& e) override;
	void mouseDoubleClick(const MouseEvent& e) override;

	bool hitTest(int x, int y) override;

	void userTriedToCloseWindow() override;

	void resizeView() override;

private:
	void showAnnouncement();
    //==============================================================================
    // Your private member variables go here...
	std::unique_ptr<UITheme> look_and_feel_;
	std::unique_ptr<AnnouncementComponent> announcement_;
	std::unique_ptr<FxMinView> min_view_;
	std::unique_ptr<FxExpandedView> expanded_view_;
	ComponentDragger dragger_;
	ComponentAnimator animator_;

	FxSound::Settings settings_;
	
	bool show_caption_;
	String caption_;
	double font_size_;
	uint32 font_color_;

	int colour_scheme_;

	enum FxViewType {MinView, ExpandedView} fx_view_;
	bool resize_view_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)

    // --- START OF SIMULATED MODIFICATIONS ---
    juce::Timer audioSessionTimer; // Timer to periodically update audio sessions
    juce::TextEditor sessionDisplayBox; // A simple text box to display session info

    void enumerateAudioSessions(); // New method to enumerate WASAPI sessions
    juce::String getProcessName(DWORD pid); // Helper to get process name from PID
    // --- END OF SIMULATED MODIFICATIONS ---
};


