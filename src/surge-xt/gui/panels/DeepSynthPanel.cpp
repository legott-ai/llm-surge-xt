/*
 * Surge XT - a free and open source hybrid synthesizer,
 * built by Surge Synth Team
 *
 * Learn more at https://surge-synthesizer.github.io/
 *
 * Copyright 2018-2024, various authors, as described in the GitHub
 * transaction log.
 *
 * Surge XT is released under the GNU General Public Licence v3
 * or later (GPL-3.0-or-later). The license is found in the "LICENSE"
 * file in the root of this repository, or at
 * https://www.gnu.org/licenses/gpl-3.0.en.html
 */

#include "AIDebug.h"

#include <iostream>
#include "DeepSynthPanel.h"
#include "SurgeGUIEditor.h"
#include "SurgeStorage.h"
#include "UserDefaults.h"
#include "SurgeSynthesizer.h"
#include "RuntimeFont.h"
#include "SurgeImageStore.h"
#include "ClaudeParameterMapper.h"
#include "OpenAIAPIClient.h"
#include "ClaudeAPIClient.h"
#include "GeminiAPIClient.h"
#include "PatchVectorDB.h"
#include "widgets/MainFrame.h"
#include "SkinColors.h"

using namespace Surge::Skin;

namespace Surge
{
namespace GUI
{
namespace Panels
{

DeepSynthPanel::DeepSynthPanel(SurgeGUIEditor *editor, SurgeStorage *storage)
    : editor(editor), storage(storage)
{
    DSTRACE << "DEBUG: DeepSynthPanel constructor called" << std::endl;

    // Initialize vector database for RAG functionality
    vectorDatabase = std::make_shared<Surge::PatchDB::VectorDatabase>(storage);
    DSTRACE << "DEBUG: Building vector database from factory patches..." << std::endl;

    try
    {
        vectorDatabase->buildFromFactoryPatches();
        DSTRACE << "DEBUG: Vector database built with " << vectorDatabase->patches.size()
                  << " patches" << std::endl;
    }
    catch (const std::exception &e)
    {
        DSTRACE << "WARNING: Vector database build failed: " << e.what() << std::endl;
    }

    // IMPORTANT: Setup UI components FIRST before switching models
    setupComponents();

    // Load saved model preference (default to Gemini 3.0 Flash)
    int savedModel =
        Surge::Storage::getUserDefaultValue(storage, Surge::Storage::DeepSynthSelectedModel,
                                            static_cast<int>(Surge::AI::ModelType::GEMINI_3_FLASH));
    currentModelType = static_cast<Surge::AI::ModelType>(savedModel);

    // Initialize the first provider
    switchToModel(currentModelType);

    // Initial API key check (silent)
    if (currentProvider && !currentProvider->isAPIKeyValid())
    {
        updateStatus("API key not configured", true);
    }
}

DeepSynthPanel::~DeepSynthPanel()
{
    DSTRACE << "DEBUG: DeepSynthPanel destructor called" << std::endl;
}

void DeepSynthPanel::switchToModel(Surge::AI::ModelType modelType)
{
    currentModelType = modelType;

    // Save preference
    Surge::Storage::updateUserDefaultValue(storage, Surge::Storage::DeepSynthSelectedModel,
                                           static_cast<int>(modelType));

    // Create appropriate provider
    if (Surge::AI::isGeminiModel(modelType))
    {
        currentProvider = std::make_unique<Surge::Gemini::APIClient>(storage, modelType);
    }
    else if (Surge::AI::isClaudeModel(modelType))
    {
        currentProvider = std::make_unique<Surge::Claude::APIClient>(storage, modelType);
    }
    else
    {
        currentProvider = std::make_unique<Surge::OpenAI::APIClient>(storage, modelType);
    }

    // Connect vector database
    if (currentProvider && vectorDatabase)
    {
        currentProvider->setVectorDatabase(vectorDatabase);
    }

    // Update title label
    if (titleLabel)
    {
        titleLabel->setText("DeepSynth (" + Surge::AI::getModelName(modelType) + ")",
                            juce::dontSendNotification);
    }

    // Update status
    if (currentProvider && !currentProvider->isAPIKeyValid())
    {
        updateStatus("API key not configured for this provider", true);
    }
    else
    {
        updateStatus("Ready");
    }
}

void DeepSynthPanel::setupComponents()
{
    DSTRACE << "DEBUG: setupComponents() called" << std::endl;

    // Title
    titleLabel = std::make_unique<juce::Label>(
        "title", "DeepSynth (" + Surge::AI::getModelName(currentModelType) + ")");
    titleLabel->setFont(juce::Font(14.0f, juce::Font::bold));
    titleLabel->setJustificationType(juce::Justification::centred);
    addAndMakeVisible(*titleLabel);

    // Model selector dropdown
    modelSelector = std::make_unique<juce::ComboBox>("modelSelector");
    // Populate every model in enum order; combo id == enum index + 1 so the
    // dropdown and ModelType stay in lockstep (see comboBoxChanged).
    for (int i = 0; i < static_cast<int>(Surge::AI::ModelType::NUM_MODELS); ++i)
    {
        modelSelector->addItem(Surge::AI::getModelName(static_cast<Surge::AI::ModelType>(i)), i + 1);
    }
    modelSelector->setSelectedId(static_cast<int>(currentModelType) + 1);
    modelSelector->addListener(this);
    addAndMakeVisible(*modelSelector);

    // Prompt editor
    promptEditor = std::make_unique<juce::TextEditor>("promptEditor");
    promptEditor->setMultiLine(true);
    promptEditor->setReturnKeyStartsNewLine(true);
    promptEditor->setPopupMenuEnabled(true);
    promptEditor->setScrollbarsShown(true);
    promptEditor->addListener(this);
    promptEditor->setTextToShowWhenEmpty("Describe sound...", juce::Colours::grey);
    addAndMakeVisible(*promptEditor);

    // Settings Button (Small icon/text button)
    settingsButton = std::make_unique<juce::TextButton>("Settings");
    settingsButton->addListener(this);
    settingsButton->setTooltip("Configure API Keys");
    addAndMakeVisible(*settingsButton);

    // Buttons
    generateButton = std::make_unique<juce::TextButton>("Generate");
    generateButton->addListener(this);
    generateButton->setTooltip("Generate a new patch from your description");
    addAndMakeVisible(*generateButton);

    modifyButton = std::make_unique<juce::TextButton>("Modify");
    modifyButton->addListener(this);
    modifyButton->setTooltip("Modify the current patch based on your description");
    addAndMakeVisible(*modifyButton);

    // Response display
    responseDisplay = std::make_unique<juce::TextEditor>("responseDisplay");
    responseDisplay->setMultiLine(true);
    responseDisplay->setReadOnly(true);
    responseDisplay->setPopupMenuEnabled(true);
    responseDisplay->setScrollbarsShown(true);
    responseDisplay->setFont(juce::Font(11.0f));
    responseDisplay->setText("Response will appear here...");
    responseDisplay->setColour(juce::TextEditor::textColourId, juce::Colours::lightgrey);

    auto skinCtrl = editor->currentSkin;
    responseDisplay->setColour(juce::TextEditor::backgroundColourId,
                               skinCtrl->getColor(Colors::Dialog::Entry::Background));
    responseDisplay->setColour(juce::TextEditor::outlineColourId,
                               skinCtrl->getColor(Colors::Dialog::Entry::Border));
    addAndMakeVisible(*responseDisplay);

    // Close Button (X)
    closeButton = std::make_unique<juce::TextButton>("X");
    closeButton->setButtonText("X");
    closeButton->addListener(this);
    closeButton->setTooltip("Close DeepSynth Panel");
    addAndMakeVisible(*closeButton);

    // Status label
    statusLabel = std::make_unique<juce::Label>("status", "Ready");
    statusLabel->setFont(juce::Font(10.0f));
    addAndMakeVisible(*statusLabel);
}

void DeepSynthPanel::paint(juce::Graphics &g)
{
    auto skinCtrl = editor->currentSkin;
    auto bgColor = skinCtrl->getColor(Colors::Dialog::Background);
    g.fillAll(bgColor.darker(0.1f));

    auto borderColor = skinCtrl->getColor(Colors::Dialog::Border);
    g.setColour(borderColor);
    g.drawLine(0, 0, 0, getHeight(), 2);

    g.setColour(borderColor.withAlpha(0.3f));
    g.drawRoundedRectangle(getLocalBounds().toFloat(), 2.0f, 1.0f);
}

void DeepSynthPanel::resized()
{
    auto bounds = getLocalBounds().reduced(10);

    // Close button (Top Right)
    closeButton->setBounds(getWidth() - 25, 5, 20, 20);

    // Title
    titleLabel->setFont(juce::Font(14.0f, juce::Font::bold));
    titleLabel->setBounds(bounds.removeFromTop(20));
    bounds.removeFromTop(5);

    // Model selector dropdown
    modelSelector->setBounds(bounds.removeFromTop(25));
    bounds.removeFromTop(8);

    // Prompt editor
    promptEditor->setBounds(bounds.removeFromTop(80));
    bounds.removeFromTop(8);

    // Buttons row - stacked vertically
    generateButton->setBounds(bounds.removeFromTop(28));
    bounds.removeFromTop(4);
    modifyButton->setBounds(bounds.removeFromTop(28));
    bounds.removeFromTop(8);

    // Settings button at the bottom right before status
    auto settingsArea = bounds.removeFromBottom(24);
    settingsButton->setBounds(settingsArea.removeFromRight(60));
    bounds.removeFromBottom(4);

    // Status
    statusLabel->setBounds(bounds.removeFromBottom(18));
    bounds.removeFromBottom(4);

    // Response display - takes remaining space
    responseDisplay->setBounds(bounds);
}

void DeepSynthPanel::mouseDown(const juce::MouseEvent &event) {}
void DeepSynthPanel::mouseDrag(const juce::MouseEvent &event) {}

void DeepSynthPanel::buttonClicked(juce::Button *button)
{
    if (button == generateButton.get())
    {
        processPrompt(false);
    }
    else if (button == modifyButton.get())
    {
        processPrompt(true);
    }
    else if (button == settingsButton.get())
    {
        showApiSettings();
    }
    else if (button == closeButton.get())
    {
        editor->toggleDeepSynthSidebar();
    }
}

void DeepSynthPanel::comboBoxChanged(juce::ComboBox *comboBoxThatHasChanged)
{
    if (comboBoxThatHasChanged == modelSelector.get())
    {
        int selectedId = modelSelector->getSelectedId();
        auto newModelType = static_cast<Surge::AI::ModelType>(selectedId - 1);
        switchToModel(newModelType);
    }
}

void DeepSynthPanel::textEditorReturnKeyPressed(juce::TextEditor &ed)
{
    if (&ed == promptEditor.get() && !promptEditor->isMultiLine())
    {
        processPrompt(false);
    }
}

void DeepSynthPanel::textEditorEscapeKeyPressed(juce::TextEditor &ed) { ed.unfocusAllComponents(); }

void DeepSynthPanel::setVisible(bool shouldBeVisible)
{
    Component::setVisible(shouldBeVisible);

    if (shouldBeVisible)
    {
        promptEditor->grabKeyboardFocus();
        if (currentProvider && !currentProvider->isAPIKeyValid())
        {
            updateStatus("API key not configured", true);
        }
    }
}

void DeepSynthPanel::processPrompt(bool isModification)
{
    if (isProcessing)
        return;

    auto prompt = promptEditor->getText().toStdString();
    if (prompt.empty())
    {
        updateStatus("Please enter a prompt", true);
        return;
    }

    if (!currentProvider || !currentProvider->isAPIKeyValid())
    {
        updateStatus("Please configure API key", true);
        showApiSettings();
        return;
    }

    isProcessing = true;
    generateButton->setEnabled(false);
    modifyButton->setEnabled(false);
    updateStatus("Processing with " + currentProvider->getDisplayName() + "...");
    responseDisplay->setText("Generating response...");
    responseDisplay->setColour(juce::TextEditor::textColourId, juce::Colours::yellow);

    auto callback = [this](const Surge::AI::AIResponse &response) {
        juce::MessageManager::callAsync([this, response]() {
            handleAIResponse(response);
            isProcessing = false;
            generateButton->setEnabled(true);
            modifyButton->setEnabled(true);
        });
    };

    if (isModification)
    {
        Surge::Claude::ParameterMapper mapper(editor->synth);
        std::string patchInfo = mapper.exportCurrentPatchInfo();
        currentProvider->modifyPatch(prompt, patchInfo, callback);
    }
    else
    {
        currentProvider->generatePatch(prompt, callback);
    }
}

void DeepSynthPanel::handleAIResponse(const Surge::AI::AIResponse &response)
{
    hasResponse = true;

    if (response.success)
    {
        std::string displayText;
        std::string fullResponse = response.responseText;
        size_t descStart = fullResponse.find("\n\n");
        if (descStart != std::string::npos)
        {
            std::string description = fullResponse.substr(descStart + 2);
            description.erase(0, description.find_first_not_of(" \n\r\t"));
            description.erase(description.find_last_not_of(" \n\r\t") + 1);
            displayText = description;
        }
        else if (response.modifications.empty())
        {
            displayText = "No parameters were modified.";
        }
        else
        {
            displayText = "Applied " + std::to_string(response.modifications.size()) +
                          " parameter changes to create your sound.";
        }

        lastResponseText = displayText;
        responseDisplay->setText(displayText);
        responseDisplay->setColour(juce::TextEditor::textColourId, juce::Colours::white);

        if (!response.modifications.empty())
        {
            Surge::Claude::ParameterMapper mapper(editor->synth);
            std::vector<Surge::AI::PatchModification> claudeMods;
            for (const auto &mod : response.modifications)
            {
                Surge::AI::PatchModification cm;
                cm.parameterName = mod.parameterName;
                cm.value = mod.value;
                cm.description = mod.description;
                claudeMods.push_back(cm);
            }

            mapper.applyModifications(claudeMods);
            editor->synth->refresh_editor = true;
            editor->refresh_mod();
            if (editor->frame)
            {
                editor->frame->repaint();
            }

            updateStatus("Applied " + std::to_string(response.modifications.size()) + " changes");
        }
        else
        {
            updateStatus("No parameters to modify");
        }
    }
    else
    {
        lastResponseText = "Error: " + response.errorMessage;
        responseDisplay->setText(lastResponseText);
        responseDisplay->setColour(juce::TextEditor::textColourId, juce::Colours::red);
        updateStatus("Error", true);
    }
}

void DeepSynthPanel::updateStatus(const std::string &status, bool isError)
{
    statusLabel->setText(status, juce::dontSendNotification);
    auto skinCtrl = editor->currentSkin;
    if (isError)
    {
        statusLabel->setColour(juce::Label::textColourId, juce::Colours::red);
    }
    else
    {
        statusLabel->setColour(juce::Label::textColourId,
                               skinCtrl->getColor(Colors::Dialog::Label::Text));
    }
}

void DeepSynthPanel::showApiSettings()
{
    auto apiKeyDialog = std::make_unique<juce::Component>();
    apiKeyDialog->setSize(500, 420);

    // Title
    auto *titleLabel = new juce::Label({}, "DeepSynth API Settings");
    titleLabel->setBounds(10, 10, 480, 30);
    titleLabel->setFont(juce::Font(16.0f, juce::Font::bold));
    titleLabel->setJustificationType(juce::Justification::centred);
    apiKeyDialog->addAndMakeVisible(titleLabel);

    // Load current API keys
    std::string geminiKey =
        Surge::Storage::getUserDefaultValue(storage, Surge::Storage::GeminiAPIKey, "");
    std::string claudeKey =
        Surge::Storage::getUserDefaultValue(storage, Surge::Storage::ClaudeAPIKey, "");
    std::string openaiKey =
        Surge::Storage::getUserDefaultValue(storage, Surge::Storage::OpenAIAPIKey, "");

    int yPos = 50;
    const int spacing = 100;

    // === Gemini Section ===
    auto *geminiLabel = new juce::Label({}, "Gemini API Key:");
    geminiLabel->setBounds(10, yPos, 480, 20);
    geminiLabel->setFont(juce::Font(12.0f, juce::Font::bold));
    apiKeyDialog->addAndMakeVisible(geminiLabel);

    auto *geminiEditor = new juce::TextEditor();
    geminiEditor->setText(geminiKey);
    geminiEditor->setPasswordCharacter('*');
    geminiEditor->setMultiLine(false);
    geminiEditor->setBounds(10, yPos + 25, 400, 25);
    apiKeyDialog->addAndMakeVisible(geminiEditor);

    auto *geminiHint = new juce::Label({}, "Get key from: https://aistudio.google.com/");
    geminiHint->setBounds(10, yPos + 55, 480, 15);
    geminiHint->setFont(juce::Font(9.0f));
    geminiHint->setColour(juce::Label::textColourId, juce::Colours::grey);
    apiKeyDialog->addAndMakeVisible(geminiHint);

    yPos += spacing;

    // === Claude Section ===
    auto *claudeLabel = new juce::Label({}, "Claude API Key:");
    claudeLabel->setBounds(10, yPos, 480, 20);
    claudeLabel->setFont(juce::Font(12.0f, juce::Font::bold));
    apiKeyDialog->addAndMakeVisible(claudeLabel);

    auto *claudeEditor = new juce::TextEditor();
    claudeEditor->setText(claudeKey);
    claudeEditor->setPasswordCharacter('*');
    claudeEditor->setMultiLine(false);
    claudeEditor->setBounds(10, yPos + 25, 400, 25);
    apiKeyDialog->addAndMakeVisible(claudeEditor);

    auto *claudeHint = new juce::Label({}, "Get key from: https://console.anthropic.com/");
    claudeHint->setBounds(10, yPos + 55, 480, 15);
    claudeHint->setFont(juce::Font(9.0f));
    claudeHint->setColour(juce::Label::textColourId, juce::Colours::grey);
    apiKeyDialog->addAndMakeVisible(claudeHint);

    yPos += spacing;

    // === OpenAI Section ===
    auto *openaiLabel = new juce::Label({}, "OpenAI API Key:");
    openaiLabel->setBounds(10, yPos, 480, 20);
    openaiLabel->setFont(juce::Font(12.0f, juce::Font::bold));
    apiKeyDialog->addAndMakeVisible(openaiLabel);

    auto *openaiEditor = new juce::TextEditor();
    openaiEditor->setText(openaiKey);
    openaiEditor->setPasswordCharacter('*');
    openaiEditor->setMultiLine(false);
    openaiEditor->setBounds(10, yPos + 25, 400, 25);
    apiKeyDialog->addAndMakeVisible(openaiEditor);

    auto *openaiHint = new juce::Label({}, "Get key from: https://platform.openai.com/");
    openaiHint->setBounds(10, yPos + 55, 480, 15);
    openaiHint->setFont(juce::Font(9.0f));
    openaiHint->setColour(juce::Label::textColourId, juce::Colours::grey);
    apiKeyDialog->addAndMakeVisible(openaiHint);

    // Save button
    auto *saveButton = new juce::TextButton("Save All");
    saveButton->setBounds(180, 370, 140, 35);
    saveButton->setColour(juce::TextButton::buttonColourId, juce::Colours::darkblue);
    apiKeyDialog->addAndMakeVisible(saveButton);

    saveButton->onClick = [this, geminiEditor, claudeEditor, openaiEditor]() {
        // Save all three API keys
        std::string newGeminiKey = geminiEditor->getText().toStdString();
        std::string newClaudeKey = claudeEditor->getText().toStdString();
        std::string newOpenAIKey = openaiEditor->getText().toStdString();

        Surge::Storage::updateUserDefaultValue(storage, Surge::Storage::GeminiAPIKey, newGeminiKey);
        Surge::Storage::updateUserDefaultValue(storage, Surge::Storage::ClaudeAPIKey, newClaudeKey);
        Surge::Storage::updateUserDefaultValue(storage, Surge::Storage::OpenAIAPIKey, newOpenAIKey);

        // Reload current provider's key
        if (currentProvider)
        {
            currentProvider->setAPIKey(currentProvider->getAPIKey());
        }

        updateStatus("API keys saved");

        // Close dialog
        if (auto *dw = openaiEditor->findParentComponentOfClass<juce::DialogWindow>())
        {
            dw->exitModalState(1);
        }
    };

    // Note: apiKeyEditor here refers to the last created editor (OpenAI)
    // We should probably capture saveButton in the lambda below instead
    openaiEditor->onReturnKey = [saveButton]() { saveButton->triggerClick(); };

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(apiKeyDialog.release());
    options.dialogTitle = "DeepSynth API Settings";
    options.componentToCentreAround = this;
    options.dialogBackgroundColour = editor->currentSkin->getColor(Colors::Dialog::Background);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = false;
    options.resizable = false;

    options.launchAsync();
}

DeepSynthPanel::State DeepSynthPanel::getState() const
{
    State state;
    state.hasResponse = hasResponse;
    state.lastResponseText = lastResponseText;
    state.lastPromptText = promptEditor ? promptEditor->getText().toStdString() : "";
    state.selectedModelIndex = static_cast<int>(currentModelType);
    return state;
}

void DeepSynthPanel::setState(const State &state)
{
    hasResponse = state.hasResponse;
    lastResponseText = state.lastResponseText;

    if (promptEditor && !state.lastPromptText.empty())
    {
        promptEditor->setText(state.lastPromptText);
    }

    if (responseDisplay && hasResponse && !lastResponseText.empty())
    {
        responseDisplay->setText(lastResponseText);
        responseDisplay->setColour(juce::TextEditor::textColourId, juce::Colours::white);
    }

    if (modelSelector && state.selectedModelIndex >= 0 &&
        state.selectedModelIndex < static_cast<int>(Surge::AI::ModelType::NUM_MODELS))
    {
        modelSelector->setSelectedId(state.selectedModelIndex + 1);
    }
}

} // namespace Panels
} // namespace GUI
} // namespace Surge