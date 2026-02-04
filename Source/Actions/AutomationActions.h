#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include "../Timeline/AutomationLane.h"
#include <vector>

/**
 * AddAutomationPointAction - undoable action for adding an automation point
 */
class AddAutomationPointAction : public juce::UndoableAction
{
public:
    AddAutomationPointAction(AutomationLane& lane, const AutomationPoint& point)
        : laneRef(lane), addedPoint(point)
    {
    }

    AddAutomationPointAction(AutomationLane& lane, int64_t position, float value,
                              AutomationCurveType curve = AutomationCurveType::Linear)
        : laneRef(lane), addedPoint(position, value, curve)
    {
    }

    bool perform() override
    {
        if (!wasPerformed)
        {
            laneRef.addPoint(addedPoint);
            wasPerformed = true;
        }
        return true;
    }

    bool undo() override
    {
        if (wasPerformed)
        {
            // Find and remove the point by position
            int index = laneRef.findPointAtPosition(addedPoint.position, 0);
            if (index >= 0)
            {
                laneRef.removePoint(static_cast<size_t>(index));
                wasPerformed = false;
                return true;
            }
        }
        return false;
    }

    int getSizeInUnits() override { return 5; }

private:
    AutomationLane& laneRef;
    AutomationPoint addedPoint;
    bool wasPerformed = false;
};

/**
 * DeleteAutomationPointAction - undoable action for deleting an automation point
 */
class DeleteAutomationPointAction : public juce::UndoableAction
{
public:
    DeleteAutomationPointAction(AutomationLane& lane, size_t pointIndex)
        : laneRef(lane), originalIndex(pointIndex)
    {
        if (pointIndex < lane.getNumPoints())
        {
            // Store the point data for restoration
            const AutomationPoint& point = lane.getPoint(pointIndex);
            savedPosition = point.position;
            savedValue = point.value;
            savedCurve = point.curve;
            hasValidData = true;
        }
    }

    bool perform() override
    {
        if (hasValidData && originalIndex < laneRef.getNumPoints())
        {
            laneRef.removePoint(originalIndex);
            wasDeleted = true;
        }
        return true;
    }

    bool undo() override
    {
        if (wasDeleted && hasValidData)
        {
            // Restore the deleted point
            laneRef.addPoint(AutomationPoint(savedPosition, savedValue, savedCurve));
            wasDeleted = false;
            return true;
        }
        return false;
    }

    int getSizeInUnits() override { return 5; }

private:
    AutomationLane& laneRef;
    size_t originalIndex;
    bool wasDeleted = false;
    bool hasValidData = false;

    // Saved point data for restoration
    int64_t savedPosition = 0;
    float savedValue = 0.0f;
    AutomationCurveType savedCurve = AutomationCurveType::Linear;
};

/**
 * MoveAutomationPointAction - undoable action for moving an automation point
 * to a new position and/or value
 */
class MoveAutomationPointAction : public juce::UndoableAction
{
public:
    MoveAutomationPointAction(AutomationLane& lane, size_t pointIndex,
                               int64_t newPosition, float newValue)
        : laneRef(lane), targetIndex(pointIndex),
          newPos(newPosition), newVal(newValue)
    {
        if (pointIndex < lane.getNumPoints())
        {
            const AutomationPoint& point = lane.getPoint(pointIndex);
            oldPos = point.position;
            oldVal = point.value;
            hasValidData = true;
        }
    }

    bool perform() override
    {
        if (hasValidData)
        {
            // Find the point by its original position (it may have moved in the sorted list)
            int foundIndex = laneRef.findPointAtPosition(oldPos, 0);
            if (foundIndex >= 0)
            {
                laneRef.movePoint(static_cast<size_t>(foundIndex), newPos, newVal);
                wasPerformed = true;
            }
            else if (!wasPerformed && targetIndex < laneRef.getNumPoints())
            {
                // Fallback: use original index if position search fails
                laneRef.movePoint(targetIndex, newPos, newVal);
                wasPerformed = true;
            }
        }
        return true;
    }

    bool undo() override
    {
        if (wasPerformed && hasValidData)
        {
            // Find the point at the new position
            int foundIndex = laneRef.findPointAtPosition(newPos, 0);
            if (foundIndex >= 0)
            {
                laneRef.movePoint(static_cast<size_t>(foundIndex), oldPos, oldVal);
                wasPerformed = false;
                return true;
            }
        }
        return false;
    }

    int getSizeInUnits() override { return 5; }

private:
    AutomationLane& laneRef;
    size_t targetIndex;

    int64_t oldPos = 0;
    float oldVal = 0.0f;
    int64_t newPos = 0;
    float newVal = 0.0f;

    bool hasValidData = false;
    bool wasPerformed = false;
};

/**
 * SetAutomationPointCurveAction - undoable action for changing the curve type
 * of an automation point
 */
class SetAutomationPointCurveAction : public juce::UndoableAction
{
public:
    SetAutomationPointCurveAction(AutomationLane& lane, size_t pointIndex,
                                   AutomationCurveType newCurve)
        : laneRef(lane), targetIndex(pointIndex), newCurveType(newCurve)
    {
        if (pointIndex < lane.getNumPoints())
        {
            const AutomationPoint& point = lane.getPoint(pointIndex);
            oldCurveType = point.curve;
            pointPosition = point.position;
            hasValidData = true;
        }
    }

    bool perform() override
    {
        if (hasValidData)
        {
            // Find by position for robustness after sorting
            int foundIndex = laneRef.findPointAtPosition(pointPosition, 0);
            if (foundIndex >= 0)
            {
                laneRef.setPointCurve(static_cast<size_t>(foundIndex), newCurveType);
                wasPerformed = true;
            }
            else if (!wasPerformed && targetIndex < laneRef.getNumPoints())
            {
                laneRef.setPointCurve(targetIndex, newCurveType);
                wasPerformed = true;
            }
        }
        return true;
    }

    bool undo() override
    {
        if (wasPerformed && hasValidData)
        {
            int foundIndex = laneRef.findPointAtPosition(pointPosition, 0);
            if (foundIndex >= 0)
            {
                laneRef.setPointCurve(static_cast<size_t>(foundIndex), oldCurveType);
                wasPerformed = false;
                return true;
            }
        }
        return false;
    }

    int getSizeInUnits() override { return 3; }

private:
    AutomationLane& laneRef;
    size_t targetIndex;
    int64_t pointPosition = 0;

    AutomationCurveType oldCurveType = AutomationCurveType::Linear;
    AutomationCurveType newCurveType = AutomationCurveType::Linear;

    bool hasValidData = false;
    bool wasPerformed = false;
};

/**
 * ClearAutomationRangeAction - undoable action for deleting all automation points
 * within a specified range
 */
class ClearAutomationRangeAction : public juce::UndoableAction
{
public:
    ClearAutomationRangeAction(AutomationLane& lane, int64_t startPosition, int64_t endPosition)
        : laneRef(lane), rangeStart(startPosition), rangeEnd(endPosition)
    {
        // Store all points that will be deleted
        const auto& points = lane.getPoints();
        for (const auto& point : points)
        {
            if (point.position >= startPosition && point.position <= endPosition)
            {
                savedPoints.push_back(point);
            }
        }
    }

    bool perform() override
    {
        if (!wasPerformed && !savedPoints.empty())
        {
            laneRef.removePointsInRange(rangeStart, rangeEnd);
            wasPerformed = true;
        }
        return true;
    }

    bool undo() override
    {
        if (wasPerformed)
        {
            // Restore all deleted points
            for (const auto& point : savedPoints)
            {
                laneRef.addPoint(point);
            }
            wasPerformed = false;
            return true;
        }
        return false;
    }

    int getSizeInUnits() override
    {
        return static_cast<int>(5 + savedPoints.size() * 2);
    }

private:
    AutomationLane& laneRef;
    int64_t rangeStart;
    int64_t rangeEnd;

    std::vector<AutomationPoint> savedPoints;
    bool wasPerformed = false;
};

/**
 * ModifyMultiplePointsAction - undoable action for batch modifying automation points
 * Useful for operations like "move selected points" or "scale selected points"
 */
class ModifyMultiplePointsAction : public juce::UndoableAction
{
public:
    /**
     * Stores the original and new state for a single point modification
     */
    struct PointModification
    {
        int64_t oldPosition;
        float oldValue;
        AutomationCurveType oldCurve;

        int64_t newPosition;
        float newValue;
        AutomationCurveType newCurve;
    };

    ModifyMultiplePointsAction(AutomationLane& lane,
                                const std::vector<PointModification>& modifications)
        : laneRef(lane), mods(modifications)
    {
    }

    bool perform() override
    {
        if (!wasPerformed && !mods.empty())
        {
            // Apply modifications in reverse order to handle index changes
            // due to position changes and re-sorting
            for (auto it = mods.rbegin(); it != mods.rend(); ++it)
            {
                int foundIndex = laneRef.findPointAtPosition(it->oldPosition, 0);
                if (foundIndex >= 0)
                {
                    auto& point = laneRef.getPoint(static_cast<size_t>(foundIndex));
                    point.position = it->newPosition;
                    point.value = it->newValue;
                    point.curve = it->newCurve;
                }
            }
            // Re-sort after all modifications
            laneRef.sortPoints();
            wasPerformed = true;
        }
        return true;
    }

    bool undo() override
    {
        if (wasPerformed && !mods.empty())
        {
            // Restore original states
            for (const auto& mod : mods)
            {
                int foundIndex = laneRef.findPointAtPosition(mod.newPosition, 0);
                if (foundIndex >= 0)
                {
                    auto& point = laneRef.getPoint(static_cast<size_t>(foundIndex));
                    point.position = mod.oldPosition;
                    point.value = mod.oldValue;
                    point.curve = mod.oldCurve;
                }
            }
            // Re-sort after all restorations
            laneRef.sortPoints();
            wasPerformed = false;
            return true;
        }
        return false;
    }

    int getSizeInUnits() override
    {
        return static_cast<int>(5 + mods.size() * 3);
    }

private:
    AutomationLane& laneRef;
    std::vector<PointModification> mods;
    bool wasPerformed = false;
};

/**
 * DrawAutomationAction - undoable action for drawing automation with pencil tool
 * Clears points in range and adds new points
 */
class DrawAutomationAction : public juce::UndoableAction
{
public:
    DrawAutomationAction(AutomationLane& lane, int64_t startPosition, int64_t endPosition,
                          const std::vector<AutomationPoint>& newPoints)
        : laneRef(lane), rangeStart(startPosition), rangeEnd(endPosition),
          drawnPoints(newPoints)
    {
        // Store existing points that will be replaced
        const auto& points = lane.getPoints();
        for (const auto& point : points)
        {
            if (point.position >= startPosition && point.position <= endPosition)
            {
                originalPoints.push_back(point);
            }
        }
    }

    bool perform() override
    {
        if (!wasPerformed)
        {
            // Remove existing points in range
            laneRef.removePointsInRange(rangeStart, rangeEnd);

            // Add the new drawn points
            for (const auto& point : drawnPoints)
            {
                laneRef.addPoint(point);
            }

            wasPerformed = true;
        }
        return true;
    }

    bool undo() override
    {
        if (wasPerformed)
        {
            // Remove the drawn points
            laneRef.removePointsInRange(rangeStart, rangeEnd);

            // Restore original points
            for (const auto& point : originalPoints)
            {
                laneRef.addPoint(point);
            }

            wasPerformed = false;
            return true;
        }
        return false;
    }

    int getSizeInUnits() override
    {
        return static_cast<int>(10 + originalPoints.size() * 2 + drawnPoints.size() * 2);
    }

private:
    AutomationLane& laneRef;
    int64_t rangeStart;
    int64_t rangeEnd;

    std::vector<AutomationPoint> originalPoints;  // Points that existed before drawing
    std::vector<AutomationPoint> drawnPoints;     // Points created by drawing

    bool wasPerformed = false;
};
