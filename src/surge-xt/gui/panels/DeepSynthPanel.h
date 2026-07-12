#ifndef SURGE_SRC_SURGE_XT_GUI_PANELS_DEEPSYNTHPANEL_H
#define SURGE_SRC_SURGE_XT_GUI_PANELS_DEEPSYNTHPANEL_H

#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <functional>
#include "AIModelProvider.h"

class SurgeGUIEditor;
class SurgeStorage;

namespace Surge
{
namespace Claude
{
class ParameterMapper;
}
namespace PatchDB
{
class VectorDatabase;
}
} // namespace Surge

namespace Surge
{
namespace GUI
{
namespace Panels
{

class DeepSynthPanel : public juce::Component,
                       public juce::Button::Listener,
                       public juce::TextEditor::Listener,
                       public juce::ComboBox::Listener
{
  public:
    DeepSynthPanel(SurgeGUIEditor *editor, SurgeStorage *storage);
    ~DeepSynthPanel() override;

    void paint(juce::Graphics &g) override;
    void resized() override;

    // Mouse handling for dragging
    void mouseDown(const juce::MouseEvent &event) override;
    void mouseDrag(const juce::MouseEvent &event) override;

    void buttonClicked(juce::Button *button) override;
    void textEditorReturnKeyPressed(juce::TextEditor &editor) override;
    void textEditorEscapeKeyPressed(juce::TextEditor &editor) override;
    void comboBoxChanged(juce::ComboBox *comboBoxThatHasChanged) override;

    void setVisible(bool shouldBeVisible) override;
    void showApiSettings();

    // State management
    struct State
    {
        bool hasResponse = false;
        std::string lastResponseText = "";
        std::string lastPromptText = "";
        int selectedModelIndex = 0;
    };

    State getState() const;
    void setState(const State &state);

  private:
    void setupComponents();
    void processPrompt(bool isModification);
    void handleAIResponse(const Surge::AI::AIResponse &response);
    void updateStatus(const std::string &status, bool isError = false);
    void switchToModel(Surge::AI::ModelType modelType);

    SurgeGUIEditor *editor;
    SurgeStorage *storage;

    // Multi-model support
    std::unique_ptr<Surge::AI::AIModelProvider> currentProvider;
    Surge::AI::ModelType currentModelType{Surge::AI::ModelType::GEMINI_3_FLASH};

    std::shared_ptr<Surge::PatchDB::VectorDatabase> vectorDatabase;

    // UI Components
    std::unique_ptr<juce::Label> titleLabel;
    std::unique_ptr<juce::ComboBox> modelSelector;
    std::unique_ptr<juce::TextEditor> promptEditor;
    std::unique_ptr<juce::TextButton> generateButton;
    std::unique_ptr<juce::TextButton> modifyButton;
    std::unique_ptr<juce::TextButton> settingsButton;
    std::unique_ptr<juce::Label> statusLabel;
    std::unique_ptr<juce::TextEditor> responseDisplay;
    std::unique_ptr<juce::TextButton> closeButton;

    bool isProcessing = false;
    bool hasResponse = false;
    std::string lastResponseText = "";

    // For dragging (kept but handled by parent now)
    juce::Point<int> dragStartPosition;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeepSynthPanel)
};

} // namespace Panels
} // namespace GUI
} // namespace Surge

#endif // SURGE_SRC_SURGE_XT_GUI_PANELS_DEEPSYNTHPANEL_H