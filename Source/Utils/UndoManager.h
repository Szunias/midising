#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <functional>
#include <vector>

/**
 * Enhanced UndoManager wrapper using JUCE's built-in UndoManager.
 * Supports unlimited history by default with configurable limits.
 */
class DAWUndoManager
{
public:
    /**
     * Creates an UndoManager with unlimited history by default.
     * @param maxHistorySize Maximum number of transactions to keep (0 = unlimited)
     */
    explicit DAWUndoManager(int maxHistorySize = 0)
        : historyLimit(maxHistorySize)
    {
        // Configure for unlimited history (0 means no limit)
        // The second parameter is minimum transactions to keep regardless of memory
        undoManager.setMaxNumberOfStoredUnits(0, 0);
    }

    ~DAWUndoManager() = default;

    juce::UndoManager& getUndoManager() { return undoManager; }
    const juce::UndoManager& getUndoManager() const { return undoManager; }

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

    //==============================================================================
    // History Management (New in enhanced version)
    //==============================================================================

    /**
     * Get the number of undo transactions available.
     */
    int getNumUndoTransactions() const
    {
        return undoManager.getNumActionsInCurrentTransaction() > 0 ?
               countTransactions(true) : countTransactions(true);
    }

    /**
     * Get the number of redo transactions available.
     */
    int getNumRedoTransactions() const
    {
        return countTransactions(false);
    }

    /**
     * Get a list of undo transaction descriptions.
     * @param maxCount Maximum number of descriptions to return (0 = all)
     */
    std::vector<juce::String> getUndoDescriptions(int maxCount = 0) const
    {
        std::vector<juce::String> descriptions;

        // Save state and traverse undo history
        auto* nonConstThis = const_cast<DAWUndoManager*>(this);
        int count = 0;

        while (nonConstThis->undoManager.canUndo() && (maxCount == 0 || count < maxCount))
        {
            descriptions.push_back(nonConstThis->undoManager.getUndoDescription());
            nonConstThis->undoManager.undo();
            count++;
        }

        // Restore state by redoing everything
        for (int i = 0; i < count; ++i)
            nonConstThis->undoManager.redo();

        return descriptions;
    }

    /**
     * Get a list of redo transaction descriptions.
     * @param maxCount Maximum number of descriptions to return (0 = all)
     */
    std::vector<juce::String> getRedoDescriptions(int maxCount = 0) const
    {
        std::vector<juce::String> descriptions;

        auto* nonConstThis = const_cast<DAWUndoManager*>(this);
        int count = 0;

        while (nonConstThis->undoManager.canRedo() && (maxCount == 0 || count < maxCount))
        {
            descriptions.push_back(nonConstThis->undoManager.getRedoDescription());
            nonConstThis->undoManager.redo();
            count++;
        }

        // Restore state by undoing everything
        for (int i = 0; i < count; ++i)
            nonConstThis->undoManager.undo();

        return descriptions;
    }

    /**
     * Undo multiple transactions at once.
     * @param numTransactions Number of transactions to undo
     * @return Number of transactions actually undone
     */
    int undoMultiple(int numTransactions)
    {
        int undone = 0;
        for (int i = 0; i < numTransactions && canUndo(); ++i)
        {
            if (undo())
                undone++;
            else
                break;
        }
        return undone;
    }

    /**
     * Redo multiple transactions at once.
     * @param numTransactions Number of transactions to redo
     * @return Number of transactions actually redone
     */
    int redoMultiple(int numTransactions)
    {
        int redone = 0;
        for (int i = 0; i < numTransactions && canRedo(); ++i)
        {
            if (redo())
                redone++;
            else
                break;
        }
        return redone;
    }

    /**
     * Undo to a specific point in history.
     * @param transactionIndex Index in the undo history (0 = most recent)
     * @return true if successful
     */
    bool undoToIndex(int transactionIndex)
    {
        if (transactionIndex < 0)
            return false;

        int numUndone = undoMultiple(transactionIndex + 1);
        return numUndone == transactionIndex + 1;
    }

    /**
     * Set the maximum number of transactions to store.
     * @param maxTransactions Maximum transactions (0 = unlimited)
     * @param minToKeep Minimum transactions to keep regardless of limit
     */
    void setHistoryLimit(int maxTransactions, int minToKeep = 10)
    {
        historyLimit = maxTransactions;
        // JUCE uses units (individual actions), not transactions
        // For unlimited, set both to 0
        if (maxTransactions == 0)
        {
            undoManager.setMaxNumberOfStoredUnits(0, 0);
        }
        else
        {
            // Estimate ~5 actions per transaction on average
            int estimatedUnits = maxTransactions * 5;
            undoManager.setMaxNumberOfStoredUnits(estimatedUnits, minToKeep);
        }
    }

    /**
     * Get the current history limit setting.
     * @return Maximum transactions (0 = unlimited)
     */
    int getHistoryLimit() const { return historyLimit; }

    /**
     * Check if history is set to unlimited.
     */
    bool isUnlimitedHistory() const { return historyLimit == 0; }

    /**
     * Get the total number of actions (not transactions) stored.
     */
    int getNumActionsInAllTransactions() const
    {
        // Count by traversing
        auto descriptions = getUndoDescriptions();
        return static_cast<int>(descriptions.size());
    }

    /**
     * Listener interface for undo/redo events.
     */
    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void undoPerformed() {}
        virtual void redoPerformed() {}
        virtual void actionPerformed(const juce::String& /*actionName*/) {}
        virtual void historyCleared() {}
    };

    void addListener(Listener* listener)
    {
        listeners.push_back(listener);
    }

    void removeListener(Listener* listener)
    {
        listeners.erase(
            std::remove(listeners.begin(), listeners.end(), listener),
            listeners.end());
    }

    // Enhanced perform with listener notification
    bool performWithNotification(juce::UndoableAction* action, const juce::String& actionName = {})
    {
        bool result = undoManager.perform(action, actionName);
        if (result)
        {
            for (auto* listener : listeners)
                listener->actionPerformed(actionName);
        }
        return result;
    }

    // Enhanced undo with listener notification
    bool undoWithNotification()
    {
        bool result = undoManager.undo();
        if (result)
        {
            for (auto* listener : listeners)
                listener->undoPerformed();
        }
        return result;
    }

    // Enhanced redo with listener notification
    bool redoWithNotification()
    {
        bool result = undoManager.redo();
        if (result)
        {
            for (auto* listener : listeners)
                listener->redoPerformed();
        }
        return result;
    }

    // Enhanced clear with listener notification
    void clearUndoHistoryWithNotification()
    {
        undoManager.clearUndoHistory();
        for (auto* listener : listeners)
            listener->historyCleared();
    }

private:
    juce::UndoManager undoManager;
    int historyLimit = 0;  // 0 = unlimited
    std::vector<Listener*> listeners;

    // Helper to count transactions by traversing history
    int countTransactions(bool countUndo) const
    {
        auto* nonConstThis = const_cast<DAWUndoManager*>(this);
        int count = 0;

        if (countUndo)
        {
            while (nonConstThis->undoManager.canUndo())
            {
                nonConstThis->undoManager.undo();
                count++;
            }
            // Restore
            for (int i = 0; i < count; ++i)
                nonConstThis->undoManager.redo();
        }
        else
        {
            while (nonConstThis->undoManager.canRedo())
            {
                nonConstThis->undoManager.redo();
                count++;
            }
            // Restore
            for (int i = 0; i < count; ++i)
                nonConstThis->undoManager.undo();
        }

        return count;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DAWUndoManager)
};
