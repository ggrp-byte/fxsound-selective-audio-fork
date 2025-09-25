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

#include "FxProcessSelector.h"
#include "FxController.h"

FxProcessSelector::FxProcessSelector()
{
    addAndMakeVisible(m_processTable);
    m_processTable.setModel(this);
    m_processTable.getHeader().addColumn("Process Name", 1, 300);
    m_processTable.getHeader().addColumn("Enable FxSound", 2, 150);

    startTimerHz(1); // Refresh once per second
}

FxProcessSelector::~FxProcessSelector()
{
    stopTimer();
}

void FxProcessSelector::paint(juce::Graphics& g)
{
    // You can add background drawing here if needed
}

void FxProcessSelector::resized()
{
    m_processTable.setBounds(getLocalBounds());
}

void FxProcessSelector::timerCallback()
{
    refreshProcessList();
}

int FxProcessSelector::getNumRows()
{
    return m_processList.size();
}

void FxProcessSelector::paintRowBackground(juce::Graphics& g, int rowNumber, int width, int height, bool rowIsSelected)
{
    if (rowIsSelected)
    {
        g.fillAll(juce::Colours::lightblue.withAlpha(0.5f));
    }
}

void FxProcessSelector::paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected)
{
    if (columnId == 1) // Process Name
    {
        g.setColour(juce::Colours::white);
        g.drawText(m_processList[rowNumber].name, 2, 0, width - 4, height, juce::Justification::centredLeft, true);
    }
}

juce::Component* FxProcessSelector::refreshComponentForCell(int rowNumber, int columnId, bool isRowSelected, juce::Component* existingComponentToUpdate)
{
    if (columnId == 2) // Enable FxSound column
    {
        auto* toggleButton = static_cast<juce::ToggleButton*>(existingComponentToUpdate);

        if (toggleButton == nullptr)
            toggleButton = new juce::ToggleButton();

        toggleButton->setButtonText("");
        toggleButton->setComponentID(juce::String(m_processList[rowNumber].pid));
        toggleButton->setToggleState(m_processList[rowNumber].selected, juce::dontSendNotification);
        toggleButton->addListener(this);
        return toggleButton;
    }
    return nullptr;
}

void FxProcessSelector::buttonClicked(juce::Button* button)
{
    auto pid = button->getComponentID().getLargeIntValue();
    bool isSelected = button->getToggleState();

    // Find the process in our list and update its state
    for (auto& process : m_processList)
    {
        if (process.pid == pid)
        {
            process.selected = isSelected;
            break;
        }
    }

    FxController::getInstance().setProcessCaptureState((DWORD)pid, isSelected);
}

void FxProcessSelector::refreshProcessList()
{
    m_processList = FxController::getInstance().getAudioProcesses();
    m_processTable.updateContent();
}
