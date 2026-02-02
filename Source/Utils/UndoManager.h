#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <functional>

/**
 * Simple UndoManager wrapper using JUCE's built-in UndoManager.
 */
class DAWUndoManager
{
public:
    DAWUndoManager() = default;
    ~DAWUndoManager() = default;

    juce::UndoManager& getUndoManager() { return undoManager; }

    // Perform an undoable action
    bool perform(juce::UndoableAction* action, const juce::String& actionName = {})
    {
        return undoManager.perform(action, actionName);
    }

    // Undo/Redo
    bool undo() { return undoManager.undo(); }
    bool redo() { return undoManager.redo(); }

    bool canUndo() const { return undoManager.canUndo(); }
    bool canRedo() const { return undoManager.canRedo(); }

    juce::String getUndoDescription() const { return undoManager.getUndoDescription(); }
    juce::String getRedoDescription() const { return undoManager.getRedoDescription(); }

    void clearUndoHistory() { undoManager.clearUndoHistory(); }

    // Begin/end transaction for grouping multiple actions
    void beginNewTransaction(const juce::String& name = {})
    {
        undoManager.beginNewTransaction(name);
    }

private:
    juce::UndoManager undoManager;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DAWUndoManager)
};
