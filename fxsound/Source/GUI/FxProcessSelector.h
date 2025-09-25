/*
FxSound
Copyright (C) 2024  FxSound LLC

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

#pragma once

#include <JuceHeader.h>
#include "FxTheme.h"
#include "FxModel.h"

// This component will display a list of applications playing audio
// and allow the user to select which ones to process.
class FxProcessSelector : public juce::Component,
                          public juce::Timer,
                          public juce::TableListBoxModel,
                          public juce::Button::Listener
{
public:
    FxProcessSelector();
    ~FxProcessSelector() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void timerCallback() override;

    // TableListBoxModel overrides
    int getNumRows() override;
    void paintRowBackground(juce::Graphics& g, int rowNumber, int width, int height, bool rowIsSelected) override;
    void paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected) override;
    juce::Component* refreshComponentForCell(int rowNumber, int columnId, bool isRowSelected, juce::Component* existingComponentToUpdate) override;

    // Button::Listener override
    void buttonClicked(juce::Button* button) override;

private:
    void refreshProcessList();

    juce::TableListBox m_processTable;
    juce::OwnedArray<juce::ToggleButton> m_toggleButtons;
    juce::Array<FxModel::ProcessInfo> m_processList;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FxProcessSelector)
};
