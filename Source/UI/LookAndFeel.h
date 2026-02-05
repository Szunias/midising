#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/**
 * Custom LookAndFeel for MidiSing DAW.
 * Dark theme with modern styling.
 */
class MidiSingLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MidiSingLookAndFeel()
    {
        // Set colour scheme
        setColour(juce::ResizableWindow::backgroundColourId, backgroundDark);
        setColour(juce::TextButton::buttonColourId, buttonBackground);
        setColour(juce::TextButton::buttonOnColourId, accentColour);
        setColour(juce::TextButton::textColourOffId, textColour);
        setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        setColour(juce::Slider::backgroundColourId, sliderBackground);
        setColour(juce::Slider::thumbColourId, accentColour);
        setColour(juce::Slider::trackColourId, accentColour.darker(0.3f));
        setColour(juce::Label::textColourId, textColour);
        setColour(juce::ComboBox::backgroundColourId, buttonBackground);
        setColour(juce::ComboBox::textColourId, textColour);
        setColour(juce::ComboBox::outlineColourId, borderColour);
        setColour(juce::ComboBox::arrowColourId, textColour);
        setColour(juce::ComboBox::focusedOutlineColourId, accentColour);

        // ScrollBar colours
        setColour(juce::ScrollBar::backgroundColourId, backgroundDark);
        setColour(juce::ScrollBar::thumbColourId, backgroundLight);
        setColour(juce::ScrollBar::trackColourId, backgroundMid);

        // PopupMenu colours
        setColour(juce::PopupMenu::backgroundColourId, backgroundMid);
        setColour(juce::PopupMenu::textColourId, textColour);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, accentColour.withAlpha(0.3f));
        setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour(juce::PopupMenu::headerTextColourId, accentColour);

        // TextEditor colours
        setColour(juce::TextEditor::backgroundColourId, sliderBackground);
        setColour(juce::TextEditor::textColourId, textColour);
        setColour(juce::TextEditor::highlightColourId, accentColour.withAlpha(0.4f));
        setColour(juce::TextEditor::highlightedTextColourId, juce::Colours::white);
        setColour(juce::TextEditor::outlineColourId, borderColour);
        setColour(juce::TextEditor::focusedOutlineColourId, accentColour);
        setColour(juce::CaretComponent::caretColourId, accentColour);

        // ToggleButton colours
        setColour(juce::ToggleButton::textColourId, textColour);
        setColour(juce::ToggleButton::tickColourId, accentColour);
        setColour(juce::ToggleButton::tickDisabledColourId, textDimColour);

        // ProgressBar colours
        setColour(juce::ProgressBar::backgroundColourId, sliderBackground);
        setColour(juce::ProgressBar::foregroundColourId, accentColour);

        // GroupComponent colours
        setColour(juce::GroupComponent::outlineColourId, borderColour);
        setColour(juce::GroupComponent::textColourId, textColour);

        // TabbedComponent colours
        setColour(juce::TabbedComponent::backgroundColourId, backgroundDark);
        setColour(juce::TabbedComponent::outlineColourId, borderColour);
        setColour(juce::TabbedButtonBar::tabOutlineColourId, borderColour);
        setColour(juce::TabbedButtonBar::tabTextColourId, textDimColour);
        setColour(juce::TabbedButtonBar::frontOutlineColourId, accentColour);
        setColour(juce::TabbedButtonBar::frontTextColourId, textColour);

        // AlertWindow colours
        setColour(juce::AlertWindow::backgroundColourId, backgroundMid);
        setColour(juce::AlertWindow::textColourId, textColour);
        setColour(juce::AlertWindow::outlineColourId, borderColour);

        // TreeView colours
        setColour(juce::TreeView::backgroundColourId, backgroundDark);
        setColour(juce::TreeView::linesColourId, borderColour);
        setColour(juce::TreeView::selectedItemBackgroundColourId, accentColour.withAlpha(0.3f));
        setColour(juce::TreeView::dragAndDropIndicatorColourId, accentColour);
        setColour(juce::TreeView::evenItemsColourId, backgroundDark);
        setColour(juce::TreeView::oddItemsColourId, backgroundMid.withAlpha(0.5f));

        // ListBox colours
        setColour(juce::ListBox::backgroundColourId, backgroundDark);
        setColour(juce::ListBox::outlineColourId, borderColour);
        setColour(juce::ListBox::textColourId, textColour);

        // TableHeaderComponent colours
        setColour(juce::TableHeaderComponent::backgroundColourId, backgroundMid);
        setColour(juce::TableHeaderComponent::textColourId, textColour);
        setColour(juce::TableHeaderComponent::outlineColourId, borderColour);
        setColour(juce::TableHeaderComponent::highlightColourId, accentColour.withAlpha(0.2f));

        // Tooltip colours
        setColour(juce::TooltipWindow::backgroundColourId, backgroundLight);
        setColour(juce::TooltipWindow::textColourId, textColour);
        setColour(juce::TooltipWindow::outlineColourId, borderColour);

        // DirectoryContentsDisplayComponent colours (file browser)
        setColour(juce::DirectoryContentsDisplayComponent::highlightColourId, accentColour.withAlpha(0.3f));
        setColour(juce::DirectoryContentsDisplayComponent::textColourId, textColour);
        setColour(juce::DirectoryContentsDisplayComponent::highlightedTextColourId, juce::Colours::white);

        // FileBrowserComponent colours
        setColour(juce::FileBrowserComponent::currentPathBoxBackgroundColourId, buttonBackground);
        setColour(juce::FileBrowserComponent::currentPathBoxTextColourId, textColour);
        setColour(juce::FileBrowserComponent::currentPathBoxArrowColourId, textColour);
        setColour(juce::FileBrowserComponent::filenameBoxBackgroundColourId, sliderBackground);
        setColour(juce::FileBrowserComponent::filenameBoxTextColourId, textColour);

        // CodeEditorComponent colours (for potential scripting features)
        // Note: Commented out - requires juce_gui_extra module
        // setColour(juce::CodeEditorComponent::backgroundColourId, backgroundDark);
        // setColour(juce::CodeEditorComponent::highlightColourId, accentColour.withAlpha(0.3f));
        // setColour(juce::CodeEditorComponent::defaultTextColourId, textColour);
        // setColour(juce::CodeEditorComponent::lineNumberBackgroundId, backgroundMid);
        // setColour(juce::CodeEditorComponent::lineNumberTextId, textDimColour);

        // PropertyComponent colours (for settings panels)
        setColour(juce::PropertyComponent::backgroundColourId, backgroundDark);
        setColour(juce::PropertyComponent::labelTextColourId, textColour);

        // SidePanel colours
        setColour(juce::SidePanel::backgroundColour, backgroundMid);
        setColour(juce::SidePanel::titleTextColour, textColour);
        setColour(juce::SidePanel::shadowBaseColour, juce::Colours::black);
        setColour(juce::SidePanel::dismissButtonNormalColour, textDimColour);
        setColour(juce::SidePanel::dismissButtonOverColour, textColour);
        setColour(juce::SidePanel::dismissButtonDownColour, accentColour);

        // Toolbar colours
        setColour(juce::Toolbar::backgroundColourId, backgroundMid);
        setColour(juce::Toolbar::separatorColourId, borderColour);
        setColour(juce::Toolbar::buttonMouseOverBackgroundColourId, backgroundLight);
        setColour(juce::Toolbar::buttonMouseDownBackgroundColourId, accentColour.withAlpha(0.3f));
        setColour(juce::Toolbar::labelTextColourId, textColour);
        setColour(juce::Toolbar::editingModeOutlineColourId, accentColour);

        // Keyboard focus indicator
        // Note: Commented out - KeyboardFocusIndicator not available in all JUCE versions
        // setColour(juce::KeyboardFocusIndicator::focusColourId, accentColour);

        // Slider text box colours
        setColour(juce::Slider::textBoxTextColourId, textColour);
        setColour(juce::Slider::textBoxBackgroundColourId, sliderBackground);
        setColour(juce::Slider::textBoxHighlightColourId, accentColour.withAlpha(0.4f));
        setColour(juce::Slider::textBoxOutlineColourId, borderColour);

        // Hyperlink button colours
        setColour(juce::HyperlinkButton::textColourId, accentColour);
    }

    // Colour palette
    static inline juce::Colour backgroundDark   { 0xff1a1a1a };
    static inline juce::Colour backgroundMid    { 0xff252525 };
    static inline juce::Colour backgroundLight  { 0xff303030 };
    static inline juce::Colour borderColour     { 0xff404040 };
    static inline juce::Colour textColour       { 0xffe0e0e0 };
    static inline juce::Colour textDimColour    { 0xff808080 };
    static inline juce::Colour accentColour     { 0xff5a9fd4 };  // Blue
    static inline juce::Colour recordColour     { 0xffd4605a };  // Red
    static inline juce::Colour playColour       { 0xff6bba75 };  // Green
    static inline juce::Colour buttonBackground { 0xff3a3a3a };
    static inline juce::Colour sliderBackground { 0xff2a2a2a };
    static inline juce::Colour regionColour     { 0xff4a8db2 };  // Soft blue for regions

    // Additional DAW-specific colours
    static inline juce::Colour midiNoteColour   { 0xff5abed4 };  // Cyan for MIDI notes
    static inline juce::Colour automationColour { 0xffd4a65a };  // Orange for automation
    static inline juce::Colour selectionColour  { 0xff5a9fd4 };  // Blue for selections
    static inline juce::Colour gridColour       { 0xff353535 };  // Subtle grid lines
    static inline juce::Colour waveformColour   { 0xff7ab8d6 };  // Light blue for waveforms
    static inline juce::Colour warningColour    { 0xffd4a65a };  // Orange for warnings
    static inline juce::Colour errorColour      { 0xffd45a5a };  // Red for errors

    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool isMouseOverButton, bool isButtonDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
        auto baseColour = backgroundColour;

        if (isButtonDown)
            baseColour = baseColour.brighter(0.2f);
        else if (isMouseOverButton)
            baseColour = baseColour.brighter(0.1f);

        g.setColour(baseColour);
        g.fillRoundedRectangle(bounds, 4.0f);

        g.setColour(borderColour);
        g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
    }

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        juce::ignoreUnused(minSliderPos, maxSliderPos);

        auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                             static_cast<float>(width), static_cast<float>(height));

        // Draw track background
        g.setColour(sliderBackground);

        if (style == juce::Slider::LinearHorizontal)
        {
            auto trackBounds = bounds.reduced(0, bounds.getHeight() * 0.35f);
            g.fillRoundedRectangle(trackBounds, 2.0f);

            // Draw filled portion
            g.setColour(accentColour);
            auto filledBounds = trackBounds.removeFromLeft(sliderPos - static_cast<float>(x));
            g.fillRoundedRectangle(filledBounds, 2.0f);

            // Draw thumb
            float thumbX = sliderPos - 6.0f;
            auto thumbBounds = juce::Rectangle<float>(thumbX, bounds.getY(), 12.0f, bounds.getHeight());
            g.setColour(slider.findColour(juce::Slider::thumbColourId));
            g.fillRoundedRectangle(thumbBounds.reduced(0, 2), 3.0f);
        }
        else if (style == juce::Slider::LinearVertical)
        {
            auto trackBounds = bounds.reduced(bounds.getWidth() * 0.35f, 0);
            g.fillRoundedRectangle(trackBounds, 2.0f);

            // Draw filled portion (from bottom)
            g.setColour(accentColour);
            float filledHeight = bounds.getBottom() - sliderPos;
            auto filledBounds = trackBounds.removeFromBottom(filledHeight);
            g.fillRoundedRectangle(filledBounds, 2.0f);

            // Draw thumb
            float thumbY = sliderPos - 6.0f;
            auto thumbBounds = juce::Rectangle<float>(bounds.getX(), thumbY, bounds.getWidth(), 12.0f);
            g.setColour(slider.findColour(juce::Slider::thumbColourId));
            g.fillRoundedRectangle(thumbBounds.reduced(2, 0), 3.0f);
        }
        else if (style == juce::Slider::LinearBar || style == juce::Slider::LinearBarVertical)
        {
            // Draw bar-style slider (useful for meters)
            g.setColour(sliderBackground);
            g.fillRoundedRectangle(bounds, 2.0f);

            g.setColour(accentColour);
            if (style == juce::Slider::LinearBar)
            {
                auto filledWidth = sliderPos - static_cast<float>(x);
                auto filledBounds = bounds.withWidth(filledWidth);
                g.fillRoundedRectangle(filledBounds, 2.0f);
            }
            else
            {
                auto filledHeight = bounds.getBottom() - sliderPos;
                auto filledBounds = bounds.removeFromBottom(filledHeight);
                g.fillRoundedRectangle(filledBounds, 2.0f);
            }
        }
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override
    {
        juce::ignoreUnused(slider);

        auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                             static_cast<float>(width), static_cast<float>(height));
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f - 4.0f;
        auto centreX = bounds.getCentreX();
        auto centreY = bounds.getCentreY();
        auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        // Draw background arc
        juce::Path backgroundArc;
        backgroundArc.addCentredArc(centreX, centreY, radius, radius, 0.0f,
                                     rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(sliderBackground);
        g.strokePath(backgroundArc, juce::PathStrokeType(4.0f));

        // Draw value arc
        juce::Path valueArc;
        valueArc.addCentredArc(centreX, centreY, radius, radius, 0.0f,
                               rotaryStartAngle, angle, true);
        g.setColour(accentColour);
        g.strokePath(valueArc, juce::PathStrokeType(4.0f));

        // Draw pointer
        juce::Path pointer;
        auto pointerLength = radius * 0.6f;
        pointer.addRectangle(-2.0f, -pointerLength, 4.0f, pointerLength);
        g.setColour(textColour);
        g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(centreX, centreY));
    }

    //==========================================================================
    // ComboBox styling
    //==========================================================================

    void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox& box) override
    {
        juce::ignoreUnused(buttonX, buttonY, buttonW, buttonH);

        auto bounds = juce::Rectangle<float>(0.0f, 0.0f,
                                             static_cast<float>(width),
                                             static_cast<float>(height));

        auto baseColour = box.findColour(juce::ComboBox::backgroundColourId);
        if (isButtonDown)
            baseColour = baseColour.brighter(0.1f);

        g.setColour(baseColour);
        g.fillRoundedRectangle(bounds.reduced(1.0f), 4.0f);

        g.setColour(box.findColour(box.hasKeyboardFocus(true) ?
                                   juce::ComboBox::focusedOutlineColourId :
                                   juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle(bounds.reduced(1.0f), 4.0f, 1.0f);

        // Draw arrow
        auto arrowZone = bounds.removeFromRight(static_cast<float>(height)).reduced(8.0f);
        juce::Path arrow;
        arrow.addTriangle(arrowZone.getX(), arrowZone.getCentreY() - 3.0f,
                          arrowZone.getRight(), arrowZone.getCentreY() - 3.0f,
                          arrowZone.getCentreX(), arrowZone.getCentreY() + 3.0f);
        g.setColour(box.findColour(juce::ComboBox::arrowColourId));
        g.fillPath(arrow);
    }

    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds(8, 0, box.getWidth() - box.getHeight() - 8, box.getHeight());
        label.setFont(getComboBoxFont(box));
    }

    //==========================================================================
    // ScrollBar styling
    //==========================================================================

    void drawScrollbar(juce::Graphics& g, juce::ScrollBar& scrollbar,
                       int x, int y, int width, int height,
                       bool isScrollbarVertical, int thumbStartPosition, int thumbSize,
                       bool isMouseOver, bool isMouseDown) override
    {
        juce::ignoreUnused(scrollbar);

        auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                             static_cast<float>(width), static_cast<float>(height));

        // Draw track
        g.setColour(backgroundMid);
        g.fillRoundedRectangle(bounds, 3.0f);

        // Draw thumb
        juce::Rectangle<float> thumbBounds;
        if (isScrollbarVertical)
        {
            thumbBounds = juce::Rectangle<float>(
                static_cast<float>(x) + 2.0f,
                static_cast<float>(thumbStartPosition),
                static_cast<float>(width) - 4.0f,
                static_cast<float>(thumbSize));
        }
        else
        {
            thumbBounds = juce::Rectangle<float>(
                static_cast<float>(thumbStartPosition),
                static_cast<float>(y) + 2.0f,
                static_cast<float>(thumbSize),
                static_cast<float>(height) - 4.0f);
        }

        auto thumbColour = backgroundLight;
        if (isMouseDown)
            thumbColour = thumbColour.brighter(0.2f);
        else if (isMouseOver)
            thumbColour = thumbColour.brighter(0.1f);

        g.setColour(thumbColour);
        g.fillRoundedRectangle(thumbBounds, 3.0f);
    }

    int getDefaultScrollbarWidth() override
    {
        return 10;
    }

    //==========================================================================
    // PopupMenu styling
    //==========================================================================

    void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override
    {
        auto bounds = juce::Rectangle<float>(0.0f, 0.0f,
                                             static_cast<float>(width),
                                             static_cast<float>(height));

        g.setColour(backgroundMid);
        g.fillRoundedRectangle(bounds, 4.0f);

        g.setColour(borderColour);
        g.drawRoundedRectangle(bounds.reduced(0.5f), 4.0f, 1.0f);
    }

    void drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                           bool isSeparator, bool isActive, bool isHighlighted,
                           bool isTicked, bool hasSubMenu,
                           const juce::String& text, const juce::String& shortcutKeyText,
                           const juce::Drawable* icon, const juce::Colour* textColourToUse) override
    {
        if (isSeparator)
        {
            auto r = area.reduced(5, 0);
            r.removeFromTop(r.getHeight() / 2 - 1);
            g.setColour(borderColour);
            g.fillRect(r.removeFromTop(1));
            return;
        }

        auto r = area.reduced(2);

        if (isHighlighted && isActive)
        {
            g.setColour(accentColour.withAlpha(0.3f));
            g.fillRoundedRectangle(r.toFloat(), 3.0f);
        }

        auto textColor = textColourToUse != nullptr ? *textColourToUse :
                         (isActive ? textColour : textDimColour);

        r.reduce(juce::jmin(5, area.getWidth() / 20), 0);

        if (icon != nullptr)
        {
            auto iconArea = r.removeFromLeft(r.getHeight()).toFloat();
            icon->drawWithin(g, iconArea, juce::RectanglePlacement::centred, 1.0f);
            r.removeFromLeft(4);
        }

        if (isTicked)
        {
            auto tickArea = r.removeFromLeft(r.getHeight());
            juce::Path tick;
            tick.startNewSubPath(tickArea.getX() + tickArea.getWidth() * 0.2f,
                                 tickArea.getCentreY());
            tick.lineTo(static_cast<float>(tickArea.getX()) + static_cast<float>(tickArea.getWidth()) * 0.4f,
                       static_cast<float>(tickArea.getY()) + static_cast<float>(tickArea.getHeight()) * 0.7f);
            tick.lineTo(static_cast<float>(tickArea.getX()) + static_cast<float>(tickArea.getWidth()) * 0.8f,
                       static_cast<float>(tickArea.getY()) + static_cast<float>(tickArea.getHeight()) * 0.3f);
            g.setColour(accentColour);
            g.strokePath(tick, juce::PathStrokeType(2.0f));
            r.removeFromLeft(4);
        }

        if (hasSubMenu)
        {
            auto arrowH = 0.6f * static_cast<float>(r.getHeight());
            auto x = static_cast<float>(r.getRight()) - arrowH * 0.6f;
            auto y = static_cast<float>(r.getCentreY());

            juce::Path arrow;
            arrow.addTriangle(x, y - arrowH * 0.5f, x, y + arrowH * 0.5f, x + arrowH * 0.5f, y);
            g.setColour(textColor);
            g.fillPath(arrow);
            r.removeFromRight(static_cast<int>(arrowH));
        }

        g.setColour(textColor);
        g.setFont(getPopupMenuFont());
        g.drawFittedText(text, r, juce::Justification::centredLeft, 1);

        if (shortcutKeyText.isNotEmpty())
        {
            g.setColour(textDimColour);
            g.drawText(shortcutKeyText, r, juce::Justification::centredRight, true);
        }
    }

    //==========================================================================
    // TextEditor styling
    //==========================================================================

    void fillTextEditorBackground(juce::Graphics& g, int width, int height,
                                  juce::TextEditor& textEditor) override
    {
        auto bounds = juce::Rectangle<float>(0.0f, 0.0f,
                                             static_cast<float>(width),
                                             static_cast<float>(height));

        g.setColour(textEditor.findColour(juce::TextEditor::backgroundColourId));
        g.fillRoundedRectangle(bounds, 4.0f);
    }

    void drawTextEditorOutline(juce::Graphics& g, int width, int height,
                               juce::TextEditor& textEditor) override
    {
        auto bounds = juce::Rectangle<float>(0.0f, 0.0f,
                                             static_cast<float>(width),
                                             static_cast<float>(height));

        auto outlineColour = textEditor.findColour(
            textEditor.hasKeyboardFocus(true) ? juce::TextEditor::focusedOutlineColourId
                                              : juce::TextEditor::outlineColourId);
        g.setColour(outlineColour);
        g.drawRoundedRectangle(bounds.reduced(0.5f), 4.0f, 1.0f);
    }

    //==========================================================================
    // ToggleButton styling
    //==========================================================================

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                          bool shouldDrawButtonAsHighlighted,
                          bool shouldDrawButtonAsDown) override
    {
        auto fontSize = juce::jmin(15.0f, static_cast<float>(button.getHeight()) * 0.75f);
        auto tickWidth = fontSize * 1.1f;

        drawTickBox(g, button, 4.0f, (static_cast<float>(button.getHeight()) - tickWidth) * 0.5f,
                    tickWidth, tickWidth,
                    button.getToggleState(),
                    button.isEnabled(),
                    shouldDrawButtonAsHighlighted,
                    shouldDrawButtonAsDown);

        g.setColour(button.findColour(juce::ToggleButton::textColourId));
        g.setFont(fontSize);

        auto textBounds = button.getLocalBounds()
            .withTrimmedLeft(static_cast<int>(tickWidth) + 10)
            .withTrimmedRight(2);

        g.drawFittedText(button.getButtonText(), textBounds,
                         juce::Justification::centredLeft, 10);
    }

    void drawTickBox(juce::Graphics& g, juce::Component& component,
                     float x, float y, float w, float h,
                     bool ticked, bool isEnabled,
                     bool shouldDrawButtonAsHighlighted,
                     bool shouldDrawButtonAsDown) override
    {
        juce::ignoreUnused(component, shouldDrawButtonAsDown);

        auto bounds = juce::Rectangle<float>(x, y, w, h);

        // Draw box background
        auto baseColour = sliderBackground;
        if (shouldDrawButtonAsHighlighted)
            baseColour = baseColour.brighter(0.1f);

        g.setColour(baseColour);
        g.fillRoundedRectangle(bounds, 3.0f);

        g.setColour(borderColour);
        g.drawRoundedRectangle(bounds, 3.0f, 1.0f);

        // Draw tick
        if (ticked)
        {
            auto tickColour = isEnabled ? accentColour : textDimColour;
            g.setColour(tickColour);

            juce::Path tick;
            auto pad = w * 0.2f;
            tick.startNewSubPath(x + pad, y + h * 0.5f);
            tick.lineTo(x + w * 0.4f, y + h - pad);
            tick.lineTo(x + w - pad, y + pad);

            g.strokePath(tick, juce::PathStrokeType(2.0f));
        }
    }

    //==========================================================================
    // ProgressBar styling
    //==========================================================================

    void drawProgressBar(juce::Graphics& g, juce::ProgressBar& progressBar,
                         int width, int height, double progress,
                         const juce::String& textToShow) override
    {
        auto bounds = juce::Rectangle<float>(0.0f, 0.0f,
                                             static_cast<float>(width),
                                             static_cast<float>(height));

        // Draw background
        g.setColour(progressBar.findColour(juce::ProgressBar::backgroundColourId));
        g.fillRoundedRectangle(bounds, 4.0f);

        // Draw progress
        if (progress >= 0.0 && progress <= 1.0)
        {
            auto progressBounds = bounds.withWidth(static_cast<float>(progress) * bounds.getWidth());
            g.setColour(progressBar.findColour(juce::ProgressBar::foregroundColourId));
            g.fillRoundedRectangle(progressBounds, 4.0f);
        }
        else
        {
            // Indeterminate progress - animated
            auto barWidth = bounds.getWidth() * 0.3f;
            auto time = static_cast<float>(juce::Time::getMillisecondCounter() / 1000.0);
            auto offset = std::fmod(time, 1.0f) * (bounds.getWidth() + barWidth) - barWidth;

            auto barBounds = juce::Rectangle<float>(offset, 0.0f, barWidth, bounds.getHeight());
            g.setColour(progressBar.findColour(juce::ProgressBar::foregroundColourId));
            g.fillRoundedRectangle(barBounds.getIntersection(bounds), 4.0f);
        }

        // Draw text
        if (textToShow.isNotEmpty())
        {
            g.setColour(textColour);
            g.setFont(static_cast<float>(height) * 0.6f);
            g.drawText(textToShow, bounds, juce::Justification::centred, false);
        }
    }

    //==========================================================================
    // GroupComponent styling
    //==========================================================================

    void drawGroupComponentOutline(juce::Graphics& g, int width, int height,
                                   const juce::String& text,
                                   const juce::Justification& position,
                                   juce::GroupComponent& group) override
    {
        auto textH = 15.0f;
        auto indent = 3.0f;
        auto textEdgeGap = 4.0f;

        juce::Font f(textH);
        auto textW = text.isEmpty() ? 0.0f : juce::jlimit(0.0f, static_cast<float>(width) - 12.0f,
                                                          static_cast<float>(f.getStringWidth(text)) + textEdgeGap * 2.0f);

        auto x = indent;
        auto y = f.getAscent() - 3.0f;
        auto w = juce::jmax(0.0f, static_cast<float>(width) - x * 2.0f);
        auto h = juce::jmax(0.0f, static_cast<float>(height) - y - indent);

        float textX = position.testFlags(juce::Justification::left) ? x + textEdgeGap
                    : position.testFlags(juce::Justification::right) ? x + w - textW
                    : x + (w - textW) * 0.5f;

        // Draw rounded rectangle outline
        juce::Path outline;
        outline.addRoundedRectangle(x, y, w, h, 6.0f);

        g.setColour(group.findColour(juce::GroupComponent::outlineColourId));
        g.strokePath(outline, juce::PathStrokeType(1.0f));

        // Draw text background and text
        if (text.isNotEmpty())
        {
            g.setColour(backgroundDark);
            g.fillRect(textX, y - 2.0f, textW, textH + 2.0f);

            g.setColour(group.findColour(juce::GroupComponent::textColourId));
            g.setFont(f);
            g.drawText(text, static_cast<int>(textX + textEdgeGap), static_cast<int>(y - 3.0f),
                       static_cast<int>(textW - textEdgeGap * 2.0f), static_cast<int>(textH),
                       juce::Justification::centred, true);
        }
    }

    //==========================================================================
    // TabbedComponent styling
    //==========================================================================

    void drawTabButton(juce::TabBarButton& button, juce::Graphics& g, bool isMouseOver, bool isMouseDown) override
    {
        auto activeArea = button.getActiveArea();
        auto bounds = activeArea.toFloat();
        auto isFrontTab = button.isFrontTab();

        auto baseColour = isFrontTab ? backgroundMid : backgroundDark;
        if (isMouseDown)
            baseColour = baseColour.brighter(0.1f);
        else if (isMouseOver)
            baseColour = baseColour.brighter(0.05f);

        g.setColour(baseColour);
        g.fillRoundedRectangle(bounds.withTrimmedBottom(isFrontTab ? 0.0f : 2.0f), 4.0f);

        if (isFrontTab)
        {
            g.setColour(accentColour);
            g.fillRect(bounds.removeFromBottom(2.0f));
        }

        g.setColour(button.findColour(isFrontTab ? juce::TabbedButtonBar::frontTextColourId
                                                  : juce::TabbedButtonBar::tabTextColourId));

        auto textArea = button.getTextArea().toFloat();
        g.setFont(juce::Font(14.0f));  // Fixed: button.getFont() not available in JUCE
        g.drawFittedText(button.getButtonText(), textArea.toNearestInt(),
                         juce::Justification::centred, 1);
    }

    void drawTabAreaBehindFrontButton(juce::TabbedButtonBar& bar, juce::Graphics& g,
                                      int w, int h) override
    {
        juce::ignoreUnused(bar);

        g.setColour(backgroundDark);
        g.fillRect(0, 0, w, h);

        g.setColour(borderColour);
        g.fillRect(0, h - 1, w, 1);
    }

    //==========================================================================
    // AlertWindow styling
    //==========================================================================

    void drawAlertBox(juce::Graphics& g, juce::AlertWindow& alert,
                      const juce::Rectangle<int>& textArea,
                      juce::TextLayout& textLayout) override
    {
        auto bounds = alert.getLocalBounds().toFloat();

        g.setColour(alert.findColour(juce::AlertWindow::backgroundColourId));
        g.fillRoundedRectangle(bounds, 6.0f);

        g.setColour(alert.findColour(juce::AlertWindow::outlineColourId));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 1.0f);

        // Draw title bar area
        auto titleBar = bounds.removeFromTop(30.0f);
        g.setColour(backgroundLight);
        g.fillRoundedRectangle(titleBar, 6.0f);
        g.fillRect(titleBar.withTrimmedTop(6.0f));

        g.setColour(textColour);
        textLayout.draw(g, textArea.toFloat());
    }

    //==========================================================================
    // Tooltip styling
    //==========================================================================

    void drawTooltip(juce::Graphics& g, const juce::String& text, int width, int height) override
    {
        auto bounds = juce::Rectangle<float>(0.0f, 0.0f,
                                             static_cast<float>(width),
                                             static_cast<float>(height));

        g.setColour(backgroundLight);
        g.fillRoundedRectangle(bounds, 4.0f);

        g.setColour(borderColour);
        g.drawRoundedRectangle(bounds.reduced(0.5f), 4.0f, 1.0f);

        g.setColour(textColour);
        g.setFont(13.0f);
        g.drawFittedText(text, 4, 2, width - 8, height - 4,
                         juce::Justification::centredLeft, 3);
    }

    //==========================================================================
    // TreeView styling
    //==========================================================================

    void drawTreeviewPlusMinusBox(juce::Graphics& g, const juce::Rectangle<float>& area,
                                  juce::Colour backgroundColour, bool isOpen, bool isMouseOver) override
    {
        juce::ignoreUnused(backgroundColour);

        auto boxSize = juce::jmin(area.getWidth(), area.getHeight()) * 0.7f;
        auto box = area.withSizeKeepingCentre(boxSize, boxSize);

        auto baseColour = textDimColour;
        if (isMouseOver)
            baseColour = textColour;

        g.setColour(baseColour);

        if (isOpen)
        {
            // Draw minus
            g.fillRect(box.withSizeKeepingCentre(boxSize * 0.8f, 2.0f));
        }
        else
        {
            // Draw plus
            g.fillRect(box.withSizeKeepingCentre(boxSize * 0.8f, 2.0f));
            g.fillRect(box.withSizeKeepingCentre(2.0f, boxSize * 0.8f));
        }
    }

    //==========================================================================
    // TableHeaderComponent styling
    //==========================================================================

    void drawTableHeaderBackground(juce::Graphics& g, juce::TableHeaderComponent& header) override
    {
        auto bounds = header.getLocalBounds().toFloat();

        g.setColour(header.findColour(juce::TableHeaderComponent::backgroundColourId));
        g.fillRect(bounds);

        g.setColour(header.findColour(juce::TableHeaderComponent::outlineColourId));
        g.fillRect(bounds.removeFromBottom(1.0f));
    }

    void drawTableHeaderColumn(juce::Graphics& g, juce::TableHeaderComponent& header,
                               const juce::String& columnName, int columnId,
                               int width, int height, bool isMouseOver, bool isMouseDown,
                               int columnFlags) override
    {
        juce::ignoreUnused(header, columnId);

        auto bounds = juce::Rectangle<int>(0, 0, width, height);

        if (isMouseDown)
        {
            g.setColour(accentColour.withAlpha(0.2f));
            g.fillRect(bounds);
        }
        else if (isMouseOver)
        {
            g.setColour(backgroundLight);
            g.fillRect(bounds);
        }

        g.setColour(borderColour);
        g.fillRect(bounds.removeFromRight(1));

        g.setColour(textColour);
        g.setFont(14.0f);

        auto textBounds = bounds.reduced(4, 0);

        if ((columnFlags & (juce::TableHeaderComponent::sortedForwards |
                            juce::TableHeaderComponent::sortedBackwards)) != 0)
        {
            auto arrowBounds = textBounds.removeFromRight(height);
            juce::Path arrow;

            auto cx = static_cast<float>(arrowBounds.getCentreX());
            auto cy = static_cast<float>(arrowBounds.getCentreY());
            auto size = 5.0f;

            if (columnFlags & juce::TableHeaderComponent::sortedForwards)
            {
                arrow.addTriangle(cx - size, cy + size * 0.5f,
                                  cx + size, cy + size * 0.5f,
                                  cx, cy - size * 0.5f);
            }
            else
            {
                arrow.addTriangle(cx - size, cy - size * 0.5f,
                                  cx + size, cy - size * 0.5f,
                                  cx, cy + size * 0.5f);
            }

            g.setColour(textDimColour);
            g.fillPath(arrow);
        }

        g.drawFittedText(columnName, textBounds, juce::Justification::centredLeft, 1);
    }

    //==========================================================================
    // ListBox item styling
    //==========================================================================

    void drawListBoxItem(int rowNumber, juce::Graphics& g, int width, int height,
                         bool rowIsSelected, juce::Component& listBox)
    {
        juce::ignoreUnused(listBox);

        auto bounds = juce::Rectangle<int>(0, 0, width, height);

        // Alternate row backgrounds
        if (rowNumber % 2 == 0)
            g.setColour(backgroundDark);
        else
            g.setColour(backgroundMid.withAlpha(0.5f));

        g.fillRect(bounds);

        if (rowIsSelected)
        {
            g.setColour(accentColour.withAlpha(0.3f));
            g.fillRect(bounds);
        }
    }

    //==========================================================================
    // Label styling
    //==========================================================================

    void drawLabel(juce::Graphics& g, juce::Label& label) override
    {
        g.fillAll(label.findColour(juce::Label::backgroundColourId));

        if (! label.isBeingEdited())
        {
            auto alpha = label.isEnabled() ? 1.0f : 0.5f;
            auto font = getLabelFont(label);

            g.setColour(label.findColour(juce::Label::textColourId).withMultipliedAlpha(alpha));
            g.setFont(font);

            auto textArea = getLabelBorderSize(label).subtractedFrom(label.getLocalBounds());

            g.drawFittedText(label.getText(), textArea, label.getJustificationType(),
                             juce::jmax(1, static_cast<int>(static_cast<float>(textArea.getHeight()) / font.getHeight())),
                             label.getMinimumHorizontalScale());

            g.setColour(label.findColour(juce::Label::outlineColourId).withMultipliedAlpha(alpha));
        }
        else if (label.isEnabled())
        {
            g.setColour(label.findColour(juce::Label::outlineColourId));
        }

        g.drawRect(label.getLocalBounds());
    }

    //==========================================================================
    // DocumentWindow styling (title bar)
    //==========================================================================

    void drawDocumentWindowTitleBar(juce::DocumentWindow& window, juce::Graphics& g,
                                    int w, int h, int titleSpaceX, int titleSpaceW,
                                    const juce::Image* icon, bool drawTitleTextOnLeft) override
    {
        juce::ignoreUnused(icon, drawTitleTextOnLeft);

        auto bounds = juce::Rectangle<int>(0, 0, w, h);

        g.setColour(backgroundMid);
        g.fillRect(bounds);

        g.setColour(borderColour);
        g.fillRect(bounds.removeFromBottom(1));

        g.setColour(textColour);
        g.setFont(16.0f);

        auto titleBounds = juce::Rectangle<int>(titleSpaceX, 0, titleSpaceW, h);
        g.drawText(window.getName(), titleBounds, juce::Justification::centredLeft, true);
    }

    juce::Button* createDocumentWindowButton(int buttonType) override
    {
        juce::Path shape;
        juce::Colour normalColour = buttonBackground;
        juce::Colour overColour = backgroundLight;
        juce::Colour downColour = accentColour;

        if (buttonType == juce::DocumentWindow::closeButton)
        {
            shape.addLineSegment({ 0.0f, 0.0f, 1.0f, 1.0f }, 0.1f);
            shape.addLineSegment({ 1.0f, 0.0f, 0.0f, 1.0f }, 0.1f);
            normalColour = recordColour.withAlpha(0.0f);
            overColour = recordColour.withAlpha(0.5f);
            downColour = recordColour;
        }
        else if (buttonType == juce::DocumentWindow::minimiseButton)
        {
            shape.addLineSegment({ 0.0f, 0.5f, 1.0f, 0.5f }, 0.1f);
        }
        else if (buttonType == juce::DocumentWindow::maximiseButton)
        {
            shape.addRectangle(0.0f, 0.0f, 1.0f, 1.0f);
        }

        auto button = new juce::ShapeButton("", normalColour, overColour, downColour);
        button->setShape(shape, true, true, false);

        return button;
    }

    //==========================================================================
    // Concertina Panel styling
    //==========================================================================

    void drawConcertinaPanelHeader(juce::Graphics& g, const juce::Rectangle<int>& area,
                                   bool isMouseOver, bool isMouseDown,
                                   juce::ConcertinaPanel& panel, juce::Component& component) override
    {
        juce::ignoreUnused(panel);

        auto bounds = area.toFloat();

        auto baseColour = backgroundMid;
        if (isMouseDown)
            baseColour = baseColour.brighter(0.1f);
        else if (isMouseOver)
            baseColour = baseColour.brighter(0.05f);

        g.setColour(baseColour);
        g.fillRect(bounds);

        g.setColour(borderColour);
        g.fillRect(bounds.removeFromBottom(1.0f));

        g.setColour(textColour);
        g.setFont(15.0f);
        g.drawText(component.getName(), area.reduced(8, 0), juce::Justification::centredLeft, true);
    }

    //==========================================================================
    // ResizableWindow styling
    //==========================================================================

    void drawResizableWindowBorder(juce::Graphics& g, int w, int h,
                                   const juce::BorderSize<int>& border,
                                   juce::ResizableWindow& window) override
    {
        juce::ignoreUnused(border, window);

        g.setColour(borderColour);
        g.drawRect(0, 0, w, h, 1);
    }

    //==========================================================================
    // Callout Box styling
    //==========================================================================

    void drawCallOutBoxBackground(juce::CallOutBox& box, juce::Graphics& g,
                                  const juce::Path& path, juce::Image& cachedImage) override
    {
        juce::ignoreUnused(box, cachedImage);

        g.setColour(backgroundMid);
        g.fillPath(path);

        g.setColour(borderColour);
        g.strokePath(path, juce::PathStrokeType(1.0f));
    }

    int getCallOutBoxBorderSize(const juce::CallOutBox& box) override
    {
        juce::ignoreUnused(box);
        return 20;
    }

    float getCallOutBoxCornerSize(const juce::CallOutBox& box) override
    {
        juce::ignoreUnused(box);
        return 6.0f;
    }
};
