void DeepSynthPanel::showApiSettings()
{
    // Create larger dialog to accommodate 3 API key inputs
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
        editor->closeOverlay(SurgeGUIEditor::STORE_PATCH);
    };

    editor->showOverlay(SurgeGUIEditor::STORE_PATCH, std::move(apiKeyDialog));
}
