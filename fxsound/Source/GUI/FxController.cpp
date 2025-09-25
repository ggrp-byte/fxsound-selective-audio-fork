/*
FxSound
Copyright (C) 2025  FxSound LLC

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "FxController.h"
#include "FxMainWindow.h"
#include "FxSystemTrayView.h"
#include <atlbase.h>
#include <audiopolicy.h>
#include <Psapi.h>
#include "FxMessage.h"
#include "FxEffects.h"
#include "FxPresetSaveDialog.h"
#include "../Utils/SysInfo/SysInfo.h"

class FxDeviceErrorMessage : public FxWindow
{
public:
	FxDeviceErrorMessage()
	{
		setContent(&message_content_);
		centreWithSize(getWidth(), getHeight());
		addToDesktop(0);
		setAlwaysOnTop(true);
	}
	~FxDeviceErrorMessage() = default;

	void closeButtonPressed() override
	{
		exitModalState(0);
		removeFromDesktop();
	}

private:
	class MessageComponent : public Component
	{
	public:
		MessageComponent()
		{
			auto& theme = dynamic_cast<FxTheme&>(LookAndFeel::getDefaultLookAndFeel());

			error_message_.setFont(theme.getNormalFont());
			contact_message_.setFont(theme.getNormalFont());

			error_message_.setText(TRANS("Oops! There's an issue with your playback device settings.\r\nBefore we can get started, please go through the "), NotificationType::dontSendNotification);
			error_message_.setJustificationType(Justification::topLeft);
			addAndMakeVisible(error_message_);

			error_link_.setButtonText(TRANS("troubleshooting steps here."));
			error_link_.setURL(URL(L"https://www.fxsound.com/learning-center/installation-troubleshooting"));
			error_link_.setJustificationType(Justification::topLeft);
			addAndMakeVisible(error_link_);

			contact_message_.setText(TRANS(" if you're still having problems."), NotificationType::dontSendNotification);
			contact_message_.setJustificationType(Justification::topLeft);
			addAndMakeVisible(contact_message_);

			contact_link_.setButtonText(TRANS("Contact us"));
			contact_link_.setURL(URL(L"https://www.fxsound.com/support"));
			contact_link_.setJustificationType(Justification::topLeft);
			addAndMakeVisible(contact_link_);

			setSize(WIDTH, HEIGHT);
		}
		~MessageComponent() = default;

	private:
		static constexpr int WIDTH = 400;
		static constexpr int HEIGHT = 142;
		static constexpr int MESSAGE_HEIGHT = (24 + 2) * 2;
		static constexpr int HYPERLINK_HEIGHT = 24;

		void resized() override
		{
			auto bounds = getLocalBounds();
			bounds.setTop(10);
			bounds.reduce(20, 0);

			RectanglePlacement placement(RectanglePlacement::xLeft
										| RectanglePlacement::yTop
										| RectanglePlacement::doNotResize);

			auto component_area = juce::Rectangle<int>(0, 0, bounds.getWidth(), MESSAGE_HEIGHT);
			error_message_.setBounds(placement.appliedTo(component_area, bounds));
			auto x = error_message_.getX() + error_message_.getBorderSize().getLeft();

			bounds.setTop(error_message_.getBottom() + 2);
			component_area = juce::Rectangle<int>(0, 0, bounds.getWidth(), HYPERLINK_HEIGHT);
			error_link_.setBounds(placement.appliedTo(component_area, bounds).withX(x));

			bounds.setTop(error_link_.getBottom() + 20);
			component_area = juce::Rectangle<int>(0, 0, contact_link_.getTextWidth(), HYPERLINK_HEIGHT);
			contact_link_.setBounds(placement.appliedTo(component_area, bounds).withX(x));

			bounds = juce::Rectangle<int>(contact_link_.getRight(), contact_link_.getY(), error_message_.getWidth() - contact_link_.getWidth(), MESSAGE_HEIGHT / 2);
			contact_message_.setBounds(bounds);
		}

		Label error_message_;
		Label contact_message_;
		FxHyperlink error_link_;
		FxHyperlink contact_link_;

		JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MessageComponent)
	};

	MessageComponent message_content_;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FxDeviceErrorMessage)
};

FxController::FxController() : message_window_(L"FxSoundHotkeys", (WNDPROC) eventCallback)
{
	dfx_enabled_ = true;
	authenticated_ = true;
	minimize_tip_ = true;
	processing_time_over_tip_ = true;
	subscription_validity_tip_ = true;
    subscription_unverified_tip_ = true;

	hotkeys_registered_ = false;
	output_changed_ = false;
    playback_device_available_ = true;

	device_count_ = 0;
	output_device_id_ = L"";
	
	audio_process_time_ = 0;
	audio_process_on_counter_ = 0;
	audio_process_off_counter_ = 0;
	audio_process_on_ = false;

    audio_process_start_time_ = -1LL;

    main_window_ = nullptr;
    audio_passthru_ = nullptr;

	file_logger_.reset(FileLogger::createDefaultAppLogger(L"FxSound", L"fxsound.log", L"FxSound logs"));
    logMessage("v" + JUCEApplication::getInstance()->getApplicationVersion());
	logMessage(SystemStats::getOperatingSystemName());

	SYSTEM_INFO sys_info;
	GetNativeSystemInfo(&sys_info);
	if (sys_info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL)
	{
		logMessage(String("x86"));
	}
	else if (sys_info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64)
	{
		logMessage(String("x64"));
	}
	else if (sys_info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ARM64)
	{
		logMessage(String("ARM64"));
	}

	auto view = settings_.getInt("view");
	if (view <= 0 || view > 2)
	{
		view_ = ViewType::Pro;
	}
	else
	{
		view_ = static_cast<ViewType>(view);
	}
	auto hotkeys_support = settings_.getBool("hotkeys") && SysInfo::canSupportHotkeys();
	FxModel::getModel().setHotkeySupport(hotkeys_support);
	if (hotkeys_support)
	{
		registerHotkeys();
	}
	FxModel::getModel().setMenuClicked(settings_.getBool("menu_clicked"));

    free_plan_ = settings_.getBool("free_plan");
    hide_help_tooltips_ = settings_.getBool("hide_help_tooltips");
	hide_notifications_ = settings_.getBool("hide_notifications");
    output_device_id_ = settings_.getString("output_device_id");
    output_device_name_ = settings_.getString("output_device_name");
	max_user_presets_ = settings_.getInt("max_user_presets");
	if (max_user_presets_ < 10 || max_user_presets_ > 100)
	{
		settings_.setInt("max_user_presets", 20);
		max_user_presets_ = 20;		
	}

	volume_normalization_enabled_ = settings_.getBool("volume_normalization_enabled");
	volume_normalization_rms_ = checkRMSValue((float)settings_.getDouble("volume_normalization_rms"));
	
	SetWindowLongPtr(message_window_.getHandle(), GWLP_USERDATA, (LONG_PTR)this);

	session_id_ = 0;
	ProcessIdToSessionId(GetCurrentProcessId(), &session_id_);
	WTSRegisterSessionNotification(message_window_.getHandle(), NOTIFY_FOR_ALL_SESSIONS);
}

FxController::~FxController()
{
	WTSUnRegisterSessionNotification(message_window_.getHandle());
	unregisterHotkeys();
	stopTimer();
}


void FxController::config(const String& commandline)
{
    auto arg_list = ArgumentList(File::getSpecialLocation(File::SpecialLocationType::invokedExecutableFile).getFileName(), commandline);

    auto email = arg_list.getValueForOption("--email");
    auto preset = arg_list.getValueForOption("--preset");
    auto view = arg_list.getValueForOption("--view");
    auto output_device = arg_list.getValueForOption("--output").unquoted();
    auto language = arg_list.getValueForOption("--language");

    if (email.isNotEmpty())
    {
        settings_.setSecure("email", email);
    }
    
    if (preset.isNotEmpty())
    {
        settings_.setString("preset", preset);
    }

    if (view.isNotEmpty())
    {
        auto value = view.getIntValue();
        if (value == ViewType::Lite || value == ViewType::Pro)
        {
            settings_.setInt("view", value);
            view_ = static_cast<ViewType>(value);
        }
    }

    if (output_device.isNotEmpty())
    {
		setPreferredOutput("", output_device);
    }

    if (language.isEmpty())
    {
        language = settings_.getString("language");
        if (language.isEmpty())
        {
            language = SystemStats::getDisplayLanguage();
        }        
    }

    setLanguage(language);
}

void FxController::init(FxMainWindow* main_window, FxSystemTrayView* system_tray_view, ProcessCaptureManager* capture_manager)
{
	if (!isTimerRunning())
	{
		main_window_ = main_window;
		capture_manager_ = capture_manager;
		system_tray_view_ = system_tray_view;

        // Connect the DSP module to the new capture manager
        capture_manager_->SetDspModule(&dfx_dsp_);

        // The new model doesn't rely on the old device list or init process.
        // We will add a new way to get process lists later.
        // For now, we just start the UI timer.
		startTimer(100);
	
		auto path = File(File::addTrailingSeparator(File::getSpecialLocation(File::SpecialLocationType::userApplicationDataDirectory).getFullPathName()) + L"FxSound\\Presets");
		if (!path.exists())
		{
			path.createDirectory();
		}

		FxModel::getModel().setPowerState(settings_.getBool("power"));
		dfx_dsp_.powerOn(FxModel::getModel().getPowerState());
		if (volume_normalization_enabled_)
		{
			dfx_dsp_.setVolumeNormalization(volume_normalization_rms_);
		}
		
		FxModel::getModel().setEmail(settings_.getSecure("email").toLowerCase());

        try 
        {
            auto audio_processed = settings_.getSecure("audio_processed_per_day");
            auto audio_processed_start = settings_.getSecure("audio_process_start_time");
            if (audio_processed.isEmpty())
            {
                audio_processed_per_day_ = 0;
            }
            else
            {
                audio_processed_per_day_ = std::stoul(audio_processed.toStdString());
            }
            if (audio_processed_start.isEmpty())
            {
                audio_process_start_time_ = std::time(nullptr);
                String s;
                s << audio_process_start_time_;
                settings_.setSecure("audio_process_start_time", s);
            }
            else
            {
                audio_process_start_time_ = (std::time_t) std::stoll(audio_processed_start.toStdString());
            }
        }
        catch (...)
        {
        }

		initPresets();
		
		auto preset_name = settings_.getString("preset");
        if (!authenticated_)
        {
            preset_name = "General";
        }
		auto selected_preset = FxModel::getModel().selectPreset(preset_name, false);
		setPreset(selected_preset);

		auto app_version = JUCEApplication::getInstance()->getApplicationVersion();
		auto prev_version = settings_.getString("version");
        if (prev_version != app_version)
        {
			RegDeleteTree(HKEY_CURRENT_USER, L"Software\\DFX");

            FxModel::getModel().pushMessage(" ", { TRANS("Click here to see what's new on this version!"), "https://www.fxsound.com/changelog" });			
            settings_.setString("version", app_version);
			if (!prev_version.startsWith("1.1.2") && app_version.startsWith("1.1.2"))
			{
				FxMessage::showMessage(TRANS("FxSound is now open-source"), { TRANS("GitHub"), "https://github.com/fxsound2/fxsound-app" });
			}
            view_ = ViewType::Pro;
            settings_.setBool("run_minimized", false);
        }

		showView();
		main_window_->centreWithSize(main_window_->getWidth(), main_window_->getHeight());

        survey_tip_ = !settings_.getBool("survey_displayed");
        
        if (!settings_.getBool("run_minimized"))
        {
            showMainWindow();
        }
        else
        {
            hideMainWindow();
        }

		auto power = FxModel::getModel().getPowerState();
		main_window_->setIcon(power, false);
		system_tray_view_->setStatus(power, false);
	}
}

void FxController::initPresets()
{
	Array<FxModel::Preset> presets;
	auto working_dir = File::getCurrentWorkingDirectory();
	FileSearchPath preset_search_path(File::addTrailingSeparator(working_dir.getFullPathName()) + L"Factsoft");
	auto app_preset_paths = preset_search_path.findChildFiles(File::findFiles, false, "*.fac");
	for (auto path : app_preset_paths)
	{
		auto preset_info = dfx_dsp_.getPresetInfo(path.getFullPathName().toWideCharPointer());
		if (!preset_info.name.empty())
		{
			presets.add({ preset_info.name.c_str(), path.getFullPathName(), FxModel::PresetType::AppPreset });
		}
	}

	auto data_dir = File::addTrailingSeparator(File::getSpecialLocation(File::SpecialLocationType::userApplicationDataDirectory).getFullPathName());
	FileSearchPath user_preset_search_path(data_dir + L"FxSound\\Presets");
	auto user_preset_paths = user_preset_search_path.findChildFiles(File::findFiles, false, "*.fac");
	for (auto path : user_preset_paths)
	{
		auto preset_info = dfx_dsp_.getPresetInfo(path.getFullPathName().toWideCharPointer());
		if (!preset_info.name.empty())
		{
			presets.add({ preset_info.name.c_str(), path.getFullPathName(), FxModel::PresetType::UserPreset });
		}
	}

	FxModel::getModel().initPresets(presets);
}

void FxController::showView()
{
	if (view_ == ViewType::Pro)
	{
		main_window_->showProView();
	}
	else
	{
		main_window_->showLiteView();
	}
}

void FxController::switchView()
{
	if (view_ == ViewType::Pro)
	{
		view_ = ViewType::Lite;
		main_window_->showLiteView();
		settings_.setInt("view", static_cast<int>(view_));
	}
	else
	{
		view_ = ViewType::Pro;
		main_window_->showProView();
		settings_.setInt("view", static_cast<int>(view_));
	}
}

ViewType FxController::getCurrentView()
{
	return view_;
}

void FxController::hideMainWindow()
{
	if (main_window_->isOnDesktop())
	{
		main_window_->removeFromDesktop();
		main_window_->setVisible(false);
        settings_.setBool("run_minimized", true);
	}
	
	if (minimize_tip_)
	{
        Thread::sleep(2000);
		FxModel::getModel().pushMessage(TRANS("FxSound in system tray\r\nClick FxSound icon to reopen"));
		minimize_tip_ = false;
	}
}

void FxController::showMainWindow()
{
	if (main_window_ != nullptr)
	{
		settings_.setBool("run_minimized", false);
		main_window_->show();

		auto power = FxModel::getModel().getPowerState();
		main_window_->setIcon(power, audio_process_on_);

        if (survey_tip_)
        {
            uint32_t survey_timer = settings_.getInt("survey_timer");
            if (survey_timer == 0)
            {
                survey_timer = std::time(nullptr) + (7 * (24 * 60 * 60));
                settings_.setInt("survey_timer", survey_timer);
            }
            else
            {
                uint32_t current_time = std::time(nullptr);
                if (current_time > survey_timer)
                {
                    survey_tip_ = false;
                    settings_.setBool("survey_displayed", true);
                    String message = TRANS("Thanks for using FxSound! Would you be\r\ninterested in helping us by taking a quick 4 minute\r\nsurvey so we can make FxSound better?");
                    if (authenticated_)
                    {
                        FxModel::getModel().pushMessage(message, { TRANS("Take the survey."), "https://forms.gle/ATx1ayXDWRaMdiR59" });
                    }                    
                }
            }
        }
	}
}

bool FxController::isMainWindowVisible()
{
    if (main_window_ != nullptr)
    {
        return main_window_->isOnDesktop() && main_window_->isVisible();
    }
    
    return false;
}

void FxController::setMenuClicked(bool clicked)
{
	settings_.setBool("menu_clicked", clicked);
	FxModel::getModel().setMenuClicked(clicked);
}

FxWindow* FxController::getMainWindow()
{
	return main_window_;
}

Point<int> FxController::getSystemTrayWindowPosition(int width, int height)
{
	return system_tray_view_->getSystemTrayWindowPosition(width, height);
}

bool FxController::exit()
{
	if (FxModel::getModel().isPresetModified())
	{
		FxPresetSaveDialog preset_save_dialog;
		preset_save_dialog.runModalLoop();
	}

    audio_processed_per_day_ += dfx_dsp_.getTotalAudioProcessedTime() / 1000;
    String s;
    s << audio_processed_per_day_;
    settings_.setSecure("audio_processed_per_day", s);
	
	JUCEApplication::getInstance()->systemRequestedQuit();

	return true;
}

void FxController::setPowerState(bool power_state)
{	
	FxModel::getModel().setPowerState(power_state);
	dfx_dsp_.powerOn(power_state && !FxModel::getModel().isMonoOutputSelected());
	settings_.setBool("power", power_state);

	system_tray_view_->setStatus(power_state, audio_process_on_);
	main_window_->setIcon(power_state, audio_process_on_);
}

bool FxController::setPreset(int selected_preset)
{
	auto& model = FxModel::getModel();

	if (selected_preset < 0 || selected_preset >= model.getPresetCount())
	{
		return false;
	}

    auto preset = model.getPreset(selected_preset);

	if (model.isPresetModified() && selected_preset != model.getSelectedPreset())
	{
		FxPresetSaveDialog preset_save_dialog;
		preset_save_dialog.runModalLoop();

		model.setPresetModified(false);
	}

	if (preset.path.isNotEmpty())
	{
		dfx_dsp_.loadPreset(preset.path.toWideCharPointer());

		settings_.setString("preset", preset.name);
		model.selectPreset(selected_preset, true);
		model.setPresetModified(false);

        for (auto e=0; e<FxEffects::EffectType::NumEffects; e++)
        {
            auto value = dfx_dsp_.getEffectValue(static_cast<DfxDsp::Effect>(e));
            dfx_dsp_.setEffectValue(static_cast<DfxDsp::Effect>(e), value*10);
        }

        auto num_bands = getNumEqBands();
        for (auto b=1; b<num_bands; b++)
        {
            dfx_dsp_.setEqBandFrequency(b, dfx_dsp_.getEqBandFrequency(b));
            dfx_dsp_.setEqBandBoostCut(b, dfx_dsp_.getEqBandBoostCut(b));
        }
	}

	model.pushMessage(TRANS("Preset: ") + model.getPreset(selected_preset).name);

	return true;
}

void FxController::setOutput(int output, bool notify)
{
    // This logic is deprecated in the new model.
    // The output is always the system's default device.
    // The UI for this will be removed/repurposed in Phase 3.
}

void FxController::selectOutput()
{
    // This logic is deprecated in the new model.
}

bool FxController::isPlaybackDeviceAvailable()
{
    // This logic is deprecated in the new model. Always assume available.
    return true;
}

void FxController::savePreset(const String& preset_name)
{
	auto& model = FxModel::getModel();

	auto preset_index = model.getSelectedPreset();
	auto preset = model.getPreset(preset_index);

	if (preset_name.isEmpty())
	{
		auto path = File::addTrailingSeparator(File::getSpecialLocation(File::SpecialLocationType::userApplicationDataDirectory).getFullPathName()) + L"FxSound\\Presets";
		dfx_dsp_.savePreset(preset.name.toWideCharPointer(), path.toWideCharPointer());

		model.pushMessage(FormatString(TRANS(L"Changes to preset %s are saved."), preset.name));
	}
	else
	{
		auto path = File::addTrailingSeparator(File::getSpecialLocation(File::SpecialLocationType::userApplicationDataDirectory).getFullPathName()) + L"FxSound\\Presets";
		dfx_dsp_.savePreset(preset_name.toWideCharPointer(), path.toWideCharPointer());

		initPresets();
		
		auto selected_preset = model.selectPreset(preset_name, false);
		setPreset(selected_preset);

		model.pushMessage(FormatString(TRANS("New preset %s is saved."), preset_name));

		if (model.getUserPresetCount() == FxController::getInstance().getMaxUserPresets())
		{
			Thread::sleep(2000);
			model.pushMessage(TRANS("Reached the limit on new presets."));
		}
	}

	model.setPresetModified(false);
}

void FxController::renamePreset(const String& new_name)
{
	auto& model = FxModel::getModel();

	auto preset_index = model.getSelectedPreset();
	auto preset = model.getPreset(preset_index);

	if (preset.type == FxModel::PresetType::UserPreset)
	{
		auto path = File::addTrailingSeparator(File::getSpecialLocation(File::SpecialLocationType::userApplicationDataDirectory).getFullPathName()) + L"FxSound\\Presets";
		dfx_dsp_.savePreset(new_name.toWideCharPointer(), path.toWideCharPointer());

		wchar_t old_path[MAX_PATH] = {};
		wcscpy_s(old_path, preset.path.toWideCharPointer());
		if (preset.path.length()+1 < MAX_PATH)
		{
			old_path[preset.path.length() + 1] = L'\0';
		}
		SHFILEOPSTRUCT file_op = { NULL, FO_DELETE, old_path, L"",
								   FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT, FALSE, 0, L"" };
		SHFileOperation(&file_op);

		initPresets();

		auto selected_preset = model.selectPreset(new_name, false);
		setPreset(selected_preset);

		model.setPresetModified(false);
	}
}

void FxController::deletePreset()
{
	auto& model = FxModel::getModel();

	auto preset_index = model.getSelectedPreset();
	auto preset = model.getPreset(preset_index);

	if (preset.type == FxModel::PresetType::UserPreset)
	{
		wchar_t path[MAX_PATH] = {};
		
		wcscpy_s(path, preset.path.toWideCharPointer());
		if (preset.path.length() + 1 < MAX_PATH)
		{
			path[preset.path.length() + 1] = L'\0';
		}

		SHFILEOPSTRUCT file_op = { NULL, FO_DELETE, path, L"",
								   FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT, FALSE, 0, L"" };
		SHFileOperation(&file_op);

		initPresets();

		setPreset(0);

		FxModel::getModel().pushMessage(FormatString(TRANS("Preset %s is deleted."), preset.name));
	}
}

void FxController::undoPreset()
{
	auto& model = FxModel::getModel();

	auto preset_index = model.getSelectedPreset();

	setPreset(preset_index);

	model.setPresetModified(false);
}

void FxController::resetPresets()
{
	auto& model = FxModel::getModel();

	auto count = model.getPresetCount();
	for (auto i=0; i<count; i++)
	{
		auto preset = model.getPreset(i);
		if (preset.type == FxModel::PresetType::UserPreset)
		{
			wchar_t path[MAX_PATH] = {};

			wcscpy_s(path, preset.path.toWideCharPointer());
			if (preset.path.length() + 1 < MAX_PATH)
			{
				path[preset.path.length() + 1] = L'\0';
			}

			SHFILEOPSTRUCT file_op = { NULL, FO_DELETE, path, L"",
									   FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT, FALSE, 0, L"" };
			SHFileOperation(&file_op);
		}
	}
	
	initPresets();

	setPreset(0);

	FxModel::getModel().pushMessage(TRANS("Presets are restored to factory defaults"));
}

bool FxController::exportPresets(const Array< FxModel::Preset>& presets)
{
    auto path_name = File::addTrailingSeparator(File::getSpecialLocation(File::SpecialLocationType::userDocumentsDirectory).getFullPathName()) + L"FxSound\\Presets\\Export\\";

    File path(path_name);
    if (!path.exists())
    {
        path.createDirectory();
    }

    bool exported = false;

    for (auto preset : presets)
    {
        bool skip = false;

        auto preset_file = File(path_name + preset.name + ".fac");
        if (preset_file.exists())
        {
            if (!FxConfirmationMessage::showMessage(String::formatted(TRANS("Preset file %s already exists in the export path, do you want to overwrite the preset file?"), preset.name.toWideCharPointer())))
            {
                skip = true;
            }
        }

        if (!skip)
        {
            dfx_dsp_.exportPreset(preset.path.toWideCharPointer(), preset.name.toWideCharPointer(), path.getFullPathName().toWideCharPointer());
            exported = true;
        }
    }
    
    return exported;
}

bool FxController::importPresets(const Array<File>& preset_files, StringArray& imported_presets, StringArray& skipped_presets)
{
    auto path_name = File::addTrailingSeparator(File::getSpecialLocation(File::SpecialLocationType::userApplicationDataDirectory).getFullPathName()) + L"FxSound\\Presets";
    File path(path_name);
    if (!path.exists())
    {
        path.createDirectory();
    }

    auto& model = FxModel::getModel();

    for (auto preset_file : preset_files)
    {
        auto preset_info = dfx_dsp_.getPresetInfo(preset_file.getFullPathName().toWideCharPointer());
        if (model.isPresetNameValid(preset_info.name.c_str()))
        {
            File import_file(path_name + "\\" + preset_info.name.c_str() + ".fac");
            if (preset_file.copyFileTo(import_file))
            {
                imported_presets.add(preset_info.name.c_str());
            }
        }
        else
        {
            skipped_presets.add(preset_info.name.c_str());
        }
    }

    if (!imported_presets.isEmpty())
    {
        initPresets();

        auto preset_name = settings_.getString("preset");
        auto selected_preset = FxModel::getModel().selectPreset(preset_name, false);
        setPreset(selected_preset);

        return true;
    }

    return false;
}

void FxController::initOutputs(std::vector<SoundDevice>& sound_devices)
{
    // This logic is deprecated in the new model.
}

void FxController::addPreferredOutput(std::vector<SoundDevice>& sound_devices)
{
	// This logic is deprecated in the new model.
}

void FxController::updateOutputs(std::vector<SoundDevice>& sound_devices)
{
    // This logic is deprecated in the new model.
}

void FxController::setSelectedOutput(String id, String name)
{
	auto pref_device_id = getPreferredOutputId();

	// If preferred output is set, then do not change the output device in settings
	if (pref_device_id.isNotEmpty() && id != pref_device_id)
	{
		return;
	}

	if (id.isNotEmpty() || name.isNotEmpty())
	{
		settings_.setString("output_device_id", id);
		settings_.setString("output_device_name", name);
	}
}

float FxController::getEffectValue(FxEffects::EffectType effect)
{
	return dfx_dsp_.getEffectValue(static_cast<DfxDsp::Effect>(effect));
}

void FxController::setEffectValue(FxEffects::EffectType effect, float value)
{
	dfx_dsp_.setEffectValue(static_cast<DfxDsp::Effect>(effect), value);

	if (!FxModel::getModel().isPresetModified())
	{
		FxModel::getModel().setPresetModified(true);
	}
}

bool FxController::isVolumeNormalizationEnbabled() const
{
	return volume_normalization_enabled_;
}

void FxController::setVolumeNormalizationEnabled(bool enabled)
{
	volume_normalization_enabled_ = enabled;
	settings_.setBool("volume_normalization_enabled", enabled);

	if (volume_normalization_enabled_)
	{
		dfx_dsp_.setVolumeNormalization(volume_normalization_rms_);
	}
	else
	{
		dfx_dsp_.setVolumeNormalization(0.0f);
	}
}

float FxController::getVolumeNormalization() const
{
	return volume_normalization_rms_;
}

void FxController::setVolumeNormalization(float target_rms)
{
	volume_normalization_rms_ = checkRMSValue(target_rms);
	settings_.setDouble("volume_normalization_rms", target_rms);
	if (volume_normalization_enabled_)
	{
		dfx_dsp_.setVolumeNormalization(target_rms);
	}
}

float FxController::checkRMSValue(float target_rms)
{
	if (target_rms < 0.125f)
	{
		return 0.125f;
	}
	else if (target_rms > 0.5f)
	{
		return 0.5f;
	}

	return target_rms;
}

bool FxController::isAudioProcessing()
{
    return audio_process_on_;
}

int FxController::getNumEqBands()
{
	return dfx_dsp_.getNumEqBands();
}

float FxController::getEqBandFrequency(int band_num)
{
	return dfx_dsp_.getEqBandFrequency(band_num);
}

void FxController::setEqBandFrequency(int band_num, float freq)
{
    dfx_dsp_.setEqBandFrequency(band_num, freq);

    if (!FxModel::getModel().isPresetModified())
    {
        FxModel::getModel().setPresetModified(true);
    }
}

void FxController::getEqBandFrequencyRange(int band_num, float* min_freq, float* max_freq)
{
    dfx_dsp_.getEqBandFrequencyRange(band_num, min_freq, max_freq);
}

float FxController::getEqBandBoostCut(int band_num)
{
	return dfx_dsp_.getEqBandBoostCut(band_num);
}

void FxController::setEqBandBoostCut(int band_num, float boost)
{
	dfx_dsp_.setEqBandBoostCut(band_num, boost);

	if (!FxModel::getModel().isPresetModified())
	{
		FxModel::getModel().setPresetModified(true);
	}
}

LRESULT CALLBACK FxController::eventCallback(HWND hwnd, const UINT message, const WPARAM w_param, const LPARAM l_param)
{
	FxController* controller = (FxController*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

	switch (message)
	{
		case WM_HOTKEY:
		{
			if (w_param == CMD_ON_OFF)
			{
				auto power_state = !FxModel::getModel().getPowerState();
				controller->setPowerState(power_state);

                String param = FxModel::getModel().getPowerState() ? TRANS(L"on") : TRANS(L"off");
                FxModel::getModel().pushMessage(controller->FormatString(TRANS("FxSound is %s."), param));
			}
			if (w_param == CMD_OPEN_CLOSE)
			{
				if (controller->main_window_->isOnDesktop())
				{
					controller->hideMainWindow();
				}
				else
				{
					controller->showMainWindow();
				}
			}
			if (w_param == CMD_NEXT_PRESET && FxModel::getModel().getPowerState())
			{
				auto preset_index = FxModel::getModel().getSelectedPreset();
				auto preset_count = FxModel::getModel().getPresetCount();
				if (preset_count > 1)
				{
					if (preset_index < preset_count - 1)
					{
						preset_index++;
					}
					else
					{
						preset_index = 0;
					}

					controller->setPreset(preset_index);
				}
			}
			if (w_param == CMD_PREVIOUS_PRESET && FxModel::getModel().getPowerState())
			{
				auto preset_index = FxModel::getModel().getSelectedPreset();
				auto preset_count = FxModel::getModel().getPresetCount();
				if (preset_count > 1)
				{
					if (preset_index != 0)
					{
						preset_index--;
					}
					else
					{
						preset_index = preset_count - 1;
					}

					controller->setPreset(preset_index);
				}
			}
			if (w_param == CMD_NEXT_OUTPUT)
			{
				auto output_index = FxModel::getModel().getSelectedOutput();
				int count = 0;
				while (count < controller->output_devices_.size())
				{
					count++;
					if (output_index < controller->output_devices_.size() - 1)
					{
						output_index++;
					}
					else
					{
						output_index = 0;
					}

					if (controller->output_devices_[output_index].deviceNumChannel >= 2)
					{
						controller->setOutput(output_index);
						break;
					}
				}
			}
		}
		break;

		case WM_WTSSESSION_CHANGE:
		{
			if (w_param == WTS_CONSOLE_CONNECT || w_param == WTS_SESSION_UNLOCK)
			{
				if ((DWORD)l_param == controller->session_id_)
				{
					controller->setPowerState(FxModel::getModel().getPowerState());
					if (!controller->isTimerRunning())
					{
						controller->startTimer(100);
					}
				}
				else
				{
					controller->dfx_dsp_.powerOn(false);
					if (controller->isTimerRunning())
					{
						controller->stopTimer();
					}
				}				
			}
		}
	}

	return DefWindowProc(hwnd, message, w_param, l_param);
}

void FxController::timerCallback()
{
	// This timer is now only for UI updates.
    // The audio processing is self-driven in ProcessCaptureManager.
    // We can add logic here later to update the visualizer based on
    // data from the capture manager.
}

void FxController::enableHotkeys(bool enable)
{
	settings_.setBool("hotkeys", enable);
	FxModel::getModel().setHotkeySupport(enable);
	if (enable)
	{
		registerHotkeys();
	}
	else
	{
		unregisterHotkeys();
	}
}

bool FxController::getHotkey(String cmdKey, int& mod, int& vk)
{
	int cmd = settings_.getInt(cmdKey);
	mod = (cmd >> 16) & 0x7;
	vk = cmd & 0xff;
	if ((mod == (MOD_CONTROL|MOD_ALT) || mod == (MOD_CONTROL|MOD_SHIFT)) && (vk >= 0x30 && vk <= 0x39) || (vk >= 'A' && vk <= 'Z'))
	{
		return true;
	}

	return false;
}

bool FxController::setHotkey(const String& command, int new_mod, int new_vk)
{
	StringArray hotkey_cmds = { HK_CMD_ON_OFF, HK_CMD_OPEN_CLOSE, HK_CMD_NEXT_PRESET, HK_CMD_PREVIOUS_PRESET, HK_CMD_NEXT_OUTPUT };

	for (int i = 0; i < hotkey_cmds.size(); i++)
	{
		if (command == hotkey_cmds[i])
		{
			continue;
		}

		int mod, vk;
		if (getHotkey(hotkey_cmds[i], mod, vk))
		{
			if (mod == new_mod && vk == new_vk)
			{
				return false;
			}
		}
	}

	unsigned int code = (new_mod << 16) | new_vk;
	settings_.setInt(command, code);

	if (command == HK_CMD_ON_OFF)
	{
		::UnregisterHotKey(message_window_.getHandle(), CMD_ON_OFF);
		if (isValidHotkey(new_mod, new_vk))
		{
			::RegisterHotKey(message_window_.getHandle(), CMD_ON_OFF, new_mod, new_vk);
		}		
		return true;
	}

	if (command == HK_CMD_OPEN_CLOSE)
	{
		::UnregisterHotKey(message_window_.getHandle(), CMD_OPEN_CLOSE);
		if (isValidHotkey(new_mod, new_vk))
		{
			::RegisterHotKey(message_window_.getHandle(), CMD_OPEN_CLOSE, new_mod, new_vk);
		}		
		return true;
	}

	if (command == HK_CMD_NEXT_PRESET)
	{
		::UnregisterHotKey(message_window_.getHandle(), CMD_NEXT_PRESET);
		if (isValidHotkey(new_mod, new_vk))
		{
			::RegisterHotKey(message_window_.getHandle(), CMD_NEXT_PRESET, new_mod, new_vk);
		}		
		return true;
	}

	if (command == HK_CMD_PREVIOUS_PRESET)
	{
		::UnregisterHotKey(message_window_.getHandle(), CMD_PREVIOUS_PRESET);
		if (isValidHotkey(new_mod, new_vk))
		{
			::RegisterHotKey(message_window_.getHandle(), CMD_PREVIOUS_PRESET, new_mod, new_vk);
		}		
		return true;
	}

	if (command == HK_CMD_NEXT_OUTPUT)
	{
		::UnregisterHotKey(message_window_.getHandle(), CMD_NEXT_OUTPUT);
		if (isValidHotkey(new_mod, new_vk))
		{
			::RegisterHotKey(message_window_.getHandle(), CMD_NEXT_OUTPUT, new_mod, new_vk);
		}		
		return true;
	}

	return false;
}

bool FxController::isValidHotkey(int mod, int vk)
{
	if ((mod & MOD_CONTROL) == 0)
	{
		return false;
	}
		
	HKL hkl = GetKeyboardLayout(0);
	BYTE kbd_state[256] = { 0 };
	kbd_state[VK_CONTROL] = 0x80;

	if ((mod & MOD_ALT) != 0)
	{
		kbd_state[VK_MENU] = 0x80;
	}
	if ((mod & MOD_SHIFT) != 0)
	{
		kbd_state[VK_SHIFT] = 0x80;
	}
	
	WCHAR output[3] = { 0 };

	int len = ToUnicodeEx(vk, 0, kbd_state, output, 3, 0, hkl);
	if (len > 0 && output[0] >= 0x20) // The hotkey combination generates a non-control character
	{
		return false;
	}

	return true;
}

bool FxController::isHelpTooltipsHidden()
{
    return hide_help_tooltips_;
}

void FxController::setHelpTooltipsHidden(bool status)
{
    hide_help_tooltips_ = status;
    settings_.setBool("hide_help_tooltips", status);
    main_window_->repaint();
}

bool FxController::isNotificationsHidden()
{
	return hide_notifications_;
}

void FxController::setNotificationsHidden(bool status)
{
	hide_notifications_ = status;
	settings_.setBool("hide_notifications", status);
}

String FxController::getLanguage() const
{
    return language_;
}

void FxController::setLanguage(String language_code)
{
	if (language_code.isEmpty())
	{
		language_code = "en";
	}

    language_ = language_code;
    settings_.setString("language", language_);

    LocalisedStrings::setCurrentMappings(nullptr);

    if (language_.startsWithIgnoreCase("ko"))
    {
        LocalisedStrings::setCurrentMappings(new LocalisedStrings(String::createStringFromData(BinaryData::FxSound_ko_txt, BinaryData::FxSound_ko_txtSize), false));
    }
    else if (language_.startsWithIgnoreCase("vi"))
    {
        LocalisedStrings::setCurrentMappings(new LocalisedStrings(String::createStringFromData(BinaryData::FxSound_vi_txt, BinaryData::FxSound_vi_txtSize), false));
    }
    else if (language_.startsWithIgnoreCase("id"))
    {
        LocalisedStrings::setCurrentMappings(new LocalisedStrings(String::createStringFromData(BinaryData::FxSound_id_txt, BinaryData::FxSound_id_txtSize), false));
    }
    else if (language_.startsWithIgnoreCase("pt-br"))
    {
        LocalisedStrings::setCurrentMappings(new LocalisedStrings(String::createStringFromData(BinaryData::FxSound_ptbr_txt, BinaryData::FxSound_ptbr_txtSize), false));
    }
    else if (language_.startsWithIgnoreCase("pt"))
    {
        LocalisedStrings::setCurrentMappings(new LocalisedStrings(String::createStringFromData(BinaryData::FxSound_pt_txt, BinaryData::FxSound_pt_txtSize), false));
    }
    else if (language_.startsWithIgnoreCase("es"))
    {
        LocalisedStrings::setCurrentMappings(new LocalisedStrings(String::createStringFromData(BinaryData::FxSound_es_txt, BinaryData::FxSound_es_txtSize), false));
    }
    else if (language_.startsWithIgnoreCase("zh-CN"))
    {
        LocalisedStrings::setCurrentMappings(new LocalisedStrings(String::createStringFromData(BinaryData::FxSound_zhCN_txt, BinaryData::FxSound_zhCN_txtSize), false));
    }
	else if (language_.startsWithIgnoreCase("zh-TW"))
	{
		LocalisedStrings::setCurrentMappings(new LocalisedStrings(String::createStringFromData(BinaryData::FxSound_zhTW_txt, BinaryData::FxSound_zhTW_txtSize), false));
	}
    else if (language_.startsWithIgnoreCase("fr"))
    {
        LocalisedStrings::setCurrentMappings(new LocalisedStrings(String::createStringFromData(BinaryData::FxSound_fr_txt, BinaryData::FxSound_fr_txtSize), false));
    } 
    else if (language_.startsWithIgnoreCase("sv"))
    {
        LocalisedStrings::setCurrentMappings(new LocalisedStrings(String::createStringFromData(BinaryData::FxSound_sv_txt, BinaryData::FxSound_sv_txtSize), false));
    }
    else if (language_.startsWithIgnoreCase("it"))
    {
        LocalisedStrings::setCurrentMappings(new LocalisedStrings(String::createStringFromData(BinaryData::FxSound_it_txt, BinaryData::FxSound_it_txtSize), false));
    }
    else if (language_.startsWithIgnoreCase("ru"))
    {
        LocalisedStrings::setCurrentMappings(new LocalisedStrings(String::createStringFromData(BinaryData::FxSound_ru_txt, BinaryData::FxSound_ru_txtSize), false));
    }
    else if (language_.startsWithIgnoreCase("ro"))
    {
        LocalisedStrings::setCurrentMappings(new LocalisedStrings(String::createStringFromData(BinaryData::FxSound_ro_txt, BinaryData::FxSound_ro_txtSize), false));
    }
    else if (language_.startsWithIgnoreCase("tr"))
    {
        LocalisedStrings::setCurrentMappings(new LocalisedStrings(String::createStringFromData(BinaryData::FxSound_tr_txt, BinaryData::FxSound_tr_txtSize), false));
    }
    else if (language_.startsWithIgnoreCase("pl"))
    {
        LocalisedStrings::setCurrentMappings(new LocalisedStrings(String::createStringFromData(BinaryData::FxSound_pl_txt, BinaryData::FxSound_pl_txtSize), false));
    }
    else if (language_.startsWithIgnoreCase("de"))
    {
        LocalisedStrings::setCurrentMappings(new LocalisedStrings(String::createStringFromData(BinaryData::FxSound_de_txt, BinaryData::FxSound_de_txtSize), false));
    }
	else if (language_.startsWithIgnoreCase("hu"))
	{
		LocalisedStrings::setCurrentMappings(new LocalisedStrings(String::createStringFromData(BinaryData::fxsound_hu_txt, BinaryData::fxsound_hu_txtSize), false));
	}
	else if (language_.startsWithIgnoreCase("th"))
	{
		LocalisedStrings::setCurrentMappings(new LocalisedStrings(String::createStringFromData(BinaryData::FxSound_th_txt, BinaryData::FxSound_th_txtSize), false));
	}
	else if (language_.startsWithIgnoreCase("nl"))
	{
		LocalisedStrings::setCurrentMappings(new LocalisedStrings(String::createStringFromData(BinaryData::FxSound_nl_txt, BinaryData::FxSound_nl_txtSize), false));
	}
	else if (language_.startsWithIgnoreCase("ja"))
	{
		LocalisedStrings::setCurrentMappings(new LocalisedStrings(String::createStringFromData(BinaryData::FxSound_ja_txt, BinaryData::FxSound_ja_txtSize), false));
	}
	else if (language_.startsWithIgnoreCase("ar"))
	{
		LocalisedStrings::setCurrentMappings(new LocalisedStrings(String::createStringFromData(BinaryData::FxSound_ar_txt, BinaryData::FxSound_ar_txtSize), false));
	}
	else if (language_.startsWithIgnoreCase("hr"))
	{
		LocalisedStrings::setCurrentMappings(new LocalisedStrings(String::createStringFromData(BinaryData::FxSound_hr_txt, BinaryData::FxSound_hr_txtSize), false));
	}
	else if (language_.startsWithIgnoreCase("ba"))
	{
		LocalisedStrings::setCurrentMappings(new LocalisedStrings(String::createStringFromData(BinaryData::FxSound_ba_txt, BinaryData::FxSound_ba_txtSize), false));
	}
	else if (language_.startsWithIgnoreCase("fa"))
	{
		LocalisedStrings::setCurrentMappings(new LocalisedStrings(String::createStringFromData(BinaryData::FxSound_ir_txt, BinaryData::FxSound_ir_txtSize), false));
	}
	else if (language_.startsWithIgnoreCase("ua"))
	{
		LocalisedStrings::setCurrentMappings(new LocalisedStrings(String::createStringFromData(BinaryData::FxSound_ua_txt, BinaryData::FxSound_ua_txtSize), false));
	}
  
	auto* theme = dynamic_cast<FxTheme*>(&LookAndFeel::getDefaultLookAndFeel());
	if (theme != nullptr)
	{
		theme->loadFont(language_);
	}

    if (main_window_ != nullptr)
    {
        main_window_->sendLookAndFeelChange();
    }
}

String FxController::getLanguageName(String language_code) const
{
	if (language_code.startsWithIgnoreCase("en"))
	{
		return "English";
	}
	else if (language_code.startsWithIgnoreCase("ko"))
	{
		return L"\ud55c\uad6d\uc5b4";
	}
	else if (language_code.startsWithIgnoreCase("vi"))
	{
		return L"Ti\u1ebfng Vi\u1ec7t";
	}
	else if (language_code.startsWithIgnoreCase("id"))
	{
		return L"bahasa Indonesia";
	}
	else if (language_code.startsWithIgnoreCase("pt-br"))
	{
		return L"portugu\u00eas brasileiro";
	}
	else if (language_code.startsWithIgnoreCase("pt"))
	{
		return L"Portugu\u00eas";
	}
	else if (language_code.startsWithIgnoreCase("es"))
	{
		return L"Espa\u00f1ol";
	}
	else if (language_code.startsWithIgnoreCase("zh-CN"))
	{
		return L"\u7b80\u4f53\u4e2d\u6587";
	}
	else if (language_code.startsWithIgnoreCase("zh-TW"))
	{
		return L"\u7e41\u9ad4\u4e2d\u6587";
	}
	else if (language_code.startsWithIgnoreCase("sv"))
	{
		return L"svenska";
	}
	else if (language_code.startsWithIgnoreCase("fr"))
	{
		return L"fran\u00e7ais";
	}
	else if (language_code.startsWithIgnoreCase("it"))
	{
		return L"Italiano";
	}
	else if (language_code.startsWithIgnoreCase("ru"))
	{
		return L"\u0440\u0443\u0441\u0441\u043a\u0438\u0439";
	}
	else if (language_code.startsWithIgnoreCase("ro"))
	{
		return L"Rom\u00e2n\u0103";
	}
	else if (language_code.startsWithIgnoreCase("tr")) {
		return L"T\u00fcrk";
	}
	else if (language_code.startsWithIgnoreCase("pl"))
	{
		return L"Polski";
	}
	else if (language_code.startsWithIgnoreCase("de"))
	{
		return L"Deutsch";
	}
	else if (language_code.startsWithIgnoreCase("hu"))
	{
		return L"Magyar";
	}
	else if (language_code.startsWithIgnoreCase("th"))
	{
		return L"\u0e41\u0e1a\u0e1a\u0e44\u0e17\u0e22";
	}
	else if (language_code.startsWithIgnoreCase("nl"))
	{
		return L"Nederlands";
	}
	else if (language_code.startsWithIgnoreCase("ja"))
	{
		return L"\u65e5\u672c\u8a9e";
	}
	else if (language_code.startsWithIgnoreCase("ar"))
	{
		return L"\u0627\u0644\u0639\u0631\u0628\u064a\u0629";
	}
	else if (language_code.startsWithIgnoreCase("hr"))
	{
		return L"hrvatski";
	}
	else if (language_code.startsWithIgnoreCase("ba"))
	{
		return L"bosanski";
	}
	else if (language_code.startsWithIgnoreCase("fa"))
	{
		return L"\u0641\u0627\u0631\u0633\u06cc";
	}
	else if (language_code.startsWithIgnoreCase("ua"))
	{
		return L"\u0443\u043a\u0440\u0430\u0457\u043d\u0441\u044c\u043a\u0430";
	}

    return "English";
}

int FxController::getMaxUserPresets() const
{
	return max_user_presets_;
}

std::tuple<String, String> FxController::getPreferredOutput()
{
	auto pref_device_id = settings_.getString("preferred_device_id");
	auto pref_device_name = settings_.getString("preferred_device_name");

	return {pref_device_id, pref_device_name};
}

String FxController::getPreferredOutputId()
{
	auto pref_device_id = settings_.getString("preferred_device_id");
	return pref_device_id;
}

String FxController::getPreferredOutputName()
{
	auto pref_device_name = settings_.getString("preferred_device_name");
	return pref_device_name;
}

void FxController::setPreferredOutput(String id, String name)
{
	settings_.setString("preferred_device_id", id);
	settings_.setString("preferred_device_name", name);

	setSelectedOutput(id, name);
}

bool FxController::isLaunchOnStartup()
{
	HKEY hkey;
	RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_QUERY_VALUE, &hkey);
	DWORD type;
	DWORD size = 0;
	RegQueryValueEx(hkey, L"FxSound", NULL, &type, NULL, &size);
	RegCloseKey(hkey);

	return size > 0;
}

void FxController::setLaunchOnStartup(bool launch_on_startup)
{
	wchar_t szPath[MAX_PATH];
	GetModuleFileName(NULL, szPath, MAX_PATH);
	HKEY hkey;
	RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hkey);

	if (launch_on_startup)
	{
		RegSetValueEx(hkey, L"FxSound", 0, REG_SZ, (BYTE*)szPath, sizeof(szPath));
	}
	else
	{
		RegDeleteValue(hkey, L"FxSound");
	}

	RegCloseKey(hkey);
}

void FxController::registerHotkeys()
{
	if (!hotkeys_registered_)
	{
		int mod;
		int vk;

		if (getHotkey(HK_CMD_ON_OFF, mod, vk))
		{
			if (isValidHotkey(mod, vk))
			{
				::RegisterHotKey(message_window_.getHandle(), CMD_ON_OFF, mod, vk);
			}			
		}
		
		if (getHotkey(HK_CMD_OPEN_CLOSE, mod, vk))
		{
			if (isValidHotkey(mod, vk))
			{
				::RegisterHotKey(message_window_.getHandle(), CMD_OPEN_CLOSE, mod, vk);
			}
			
		}

		if (getHotkey(HK_CMD_NEXT_PRESET, mod, vk))
		{
			if (isValidHotkey(mod, vk))
			{
				::RegisterHotKey(message_window_.getHandle(), CMD_NEXT_PRESET, mod, vk);
			}
			
		}

		if (getHotkey(HK_CMD_PREVIOUS_PRESET, mod, vk))
		{
			if (isValidHotkey(mod, vk))
			{
				::RegisterHotKey(message_window_.getHandle(), CMD_PREVIOUS_PRESET, mod, vk);
			}
			
		}

		if (getHotkey(HK_CMD_NEXT_OUTPUT, mod, vk))
		{
			if (isValidHotkey(mod, vk))
			{
				::RegisterHotKey(message_window_.getHandle(), CMD_NEXT_OUTPUT, mod, vk);
			}			
		}
		
		hotkeys_registered_ = true;
	}	
}

void FxController::unregisterHotkeys()
{
	if (hotkeys_registered_)
	{
		::UnregisterHotKey(message_window_.getHandle(), CMD_ON_OFF);
		::UnregisterHotKey(message_window_.getHandle(), CMD_OPEN_CLOSE);
		::UnregisterHotKey(message_window_.getHandle(), CMD_NEXT_PRESET);
		::UnregisterHotKey(message_window_.getHandle(), CMD_PREVIOUS_PRESET);
		::UnregisterHotKey(message_window_.getHandle(), CMD_NEXT_OUTPUT);
		hotkeys_registered_ = false;
	}
}

void FxController::getSpectrumBandValues(Array<float>& band_values)
{
    float values[NUM_SPECTRUM_BANDS] = { 0 };

    dfx_dsp_.getSpectrumBandValues(values, NUM_SPECTRUM_BANDS);

    band_values.clearQuick();
    for (auto i = 0; i < NUM_SPECTRUM_BANDS; i++)
    {
		if (audio_process_on_)
		{
			band_values.set(i, values[i]);
		}
		else
		{
			band_values.set(i, 0.01);
		}
    }
}

String FxController::FormatString(const String& format, const String& arg)
{
    wchar_t buffer[1024];

    swprintf_s(buffer, format.toWideCharPointer(), arg.toWideCharPointer());

    return String(buffer);
}

juce::Array<FxModel::ProcessInfo> FxController::getAudioProcesses()
{
    juce::Array<FxModel::ProcessInfo> processes;

    HRESULT hr = S_OK;
    CComPtr<IMMDeviceEnumerator> pEnumerator;
    CComPtr<IMMDeviceCollection> pDeviceCollection;

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    if (FAILED(hr)) return processes;

    hr = pEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pDeviceCollection);
    if (FAILED(hr)) return processes;

    UINT deviceCount;
    hr = pDeviceCollection->GetCount(&deviceCount);
    if (FAILED(hr)) return processes;

    for (UINT i = 0; i < deviceCount; i++)
    {
        CComPtr<IMMDevice> pDevice;
        hr = pDeviceCollection->Item(i, &pDevice);
        if (FAILED(hr)) continue;

        CComPtr<IAudioSessionManager2> pSessionManager;
        hr = pDevice->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, NULL, (void**)&pSessionManager);
        if (FAILED(hr)) continue;

        CComPtr<IAudioSessionEnumerator> pSessionEnumerator;
        hr = pSessionManager->GetSessionEnumerator(&pSessionEnumerator);
        if (FAILED(hr)) continue;

        int sessionCount;
        hr = pSessionEnumerator->GetCount(&sessionCount);
        if (FAILED(hr)) continue;

        for (int j = 0; j < sessionCount; j++)
        {
            CComPtr<IAudioSessionControl> pSessionControl;
            CComPtr<IAudioSessionControl2> pSessionControl2;
            hr = pSessionEnumerator->GetSession(j, &pSessionControl);
            if (FAILED(hr)) continue;

            hr = pSessionControl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&pSessionControl2);
            if (FAILED(hr)) continue;

            if (pSessionControl2->IsSystemSoundsSession() == S_OK) continue;

            DWORD sessionProcessId = 0;
            hr = pSessionControl2->GetProcessId(&sessionProcessId);
            if (FAILED(hr) || sessionProcessId == 0) continue;

            LPWSTR displayName = NULL;
            pSessionControl->GetDisplayName(&displayName);

            FxModel::ProcessInfo info;
            info.pid = sessionProcessId;
            info.name = juce::String(displayName ? displayName : L"");
            if (displayName) CoTaskMemFree(displayName);

            if (info.name.isEmpty())
            {
				HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, sessionProcessId);
				if (hProcess) {
					WCHAR processName[MAX_PATH] = L"";
					if (GetModuleBaseName(hProcess, NULL, processName, MAX_PATH)) {
						info.name = processName;
					}
					CloseHandle(hProcess);
				}
            }
            if (info.name.isEmpty()) info.name = "Unknown Process";

            bool found = false;
            for(const auto& p : processes) { if(p.pid == info.pid) { found = true; break; } }
            if(!found)
            {
                info.selected = capture_manager_ ? capture_manager_->IsProcessCapturing(info.pid) : false;
                processes.add(info);
            }
        }
    }

    return processes;
}

void FxController::setProcessCaptureState(DWORD pid, bool shouldCapture)
{
    if (capture_manager_)
    {
        if (shouldCapture)
        {
            capture_manager_->StartCaptureForProcess(pid);
        }
        else
        {
            capture_manager_->StopCaptureForProcess(pid);
        }
    }
}
