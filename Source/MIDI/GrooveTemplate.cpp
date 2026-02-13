#include "GrooveTemplate.h"
#include <cmath>
#include <algorithm>

GrooveTemplate::GrooveTemplate()
{
}

void GrooveTemplate::extractFromRegion(const MidiRegion& region, double quantizeGrid,
                                        double bpm, double sampleRate)
{
    juce::ignoreUnused(bpm, sampleRate);

    points.clear();
    gridSize = quantizeGrid;

    if (quantizeGrid <= 0.0)
        return;

    const auto& seq = region.getMidiSequence();

    for (int i = 0; i < seq.getNumEvents(); ++i)
    {
        auto* event = seq.getEventPointer(i);
        if (event == nullptr)
            continue;

        const auto& msg = event->message;

        if (!msg.isNoteOn())
            continue;

        double noteTime = msg.getTimeStamp();
        float velocity = msg.getFloatVelocity();

        // Find the nearest grid position
        double nearestGrid = std::round(noteTime / quantizeGrid) * quantizeGrid;

        // Calculate the timing offset from the grid
        double timingOffset = noteTime - nearestGrid;

        // Only capture points that are reasonably close to a grid line
        // (within half a grid unit)
        if (std::abs(timingOffset) <= quantizeGrid * 0.5)
        {
            GroovePoint point;
            point.gridPosition = nearestGrid;
            point.offset = timingOffset;
            point.velocityScale = velocity;  // Store absolute velocity; will be normalized on apply

            points.push_back(point);
        }
    }

    // Normalize velocity values relative to the average
    if (!points.empty())
    {
        float avgVelocity = 0.0f;
        for (const auto& p : points)
            avgVelocity += p.velocityScale;
        avgVelocity /= static_cast<float>(points.size());

        if (avgVelocity > 0.001f)
        {
            for (auto& p : points)
                p.velocityScale = p.velocityScale / avgVelocity;
        }
    }

    // Sort points by grid position
    std::sort(points.begin(), points.end(),
              [](const GroovePoint& a, const GroovePoint& b)
              {
                  return a.gridPosition < b.gridPosition;
              });
}

void GrooveTemplate::applyToRegion(MidiRegion& region, float strength,
                                    double bpm, double sampleRate) const
{
    juce::ignoreUnused(bpm, sampleRate);

    if (points.empty() || strength <= 0.0f || gridSize <= 0.0)
        return;

    strength = juce::jlimit(0.0f, 1.0f, strength);

    auto& seq = region.getMidiSequence();

    // Determine the groove pattern length (to allow looping the groove)
    double grooveLength = 0.0;
    if (!points.empty())
    {
        grooveLength = points.back().gridPosition + gridSize;
    }

    for (int i = 0; i < seq.getNumEvents(); ++i)
    {
        auto* event = seq.getEventPointer(i);
        if (event == nullptr)
            continue;

        auto& msg = event->message;

        if (!msg.isNoteOn())
            continue;

        double noteTime = msg.getTimeStamp();

        // Find the nearest grid position for this note
        double nearestGrid = std::round(noteTime / gridSize) * gridSize;

        // Map to the groove template's range (loop if necessary)
        double grooveGridPos = nearestGrid;
        if (grooveLength > 0.0)
        {
            grooveGridPos = std::fmod(nearestGrid, grooveLength);
            if (grooveGridPos < 0.0)
                grooveGridPos += grooveLength;
        }

        // Find the nearest groove point
        const GroovePoint* groovePoint = findNearestPoint(grooveGridPos);

        if (groovePoint != nullptr)
        {
            // Apply timing offset with strength
            double targetTime = nearestGrid + groovePoint->offset;
            double newTime = noteTime + (targetTime - noteTime) * strength;
            newTime = juce::jmax(0.0, newTime);
            msg.setTimeStamp(newTime);

            // Apply velocity scaling with strength
            float originalVelocity = msg.getFloatVelocity();
            float scaledVelocity = originalVelocity * groovePoint->velocityScale;
            float newVelocity = originalVelocity + (scaledVelocity - originalVelocity) * strength;
            msg.setVelocity(juce::jlimit(0.01f, 1.0f, newVelocity));

            // Also shift the corresponding note-off by the same amount
            if (event->noteOffObject != nullptr)
            {
                double noteOffTime = event->noteOffObject->message.getTimeStamp();
                double duration = noteOffTime - noteTime;
                event->noteOffObject->message.setTimeStamp(juce::jmax(0.0, newTime + duration));
            }
        }
    }

    seq.sort();
    seq.updateMatchedPairs();
}

const GroovePoint* GrooveTemplate::findNearestPoint(double gridPosition) const
{
    if (points.empty())
        return nullptr;

    const GroovePoint* nearest = nullptr;
    double bestDistance = std::numeric_limits<double>::max();

    for (const auto& point : points)
    {
        double distance = std::abs(point.gridPosition - gridPosition);

        if (distance < bestDistance)
        {
            bestDistance = distance;
            nearest = &point;
        }
    }

    // Only return if within half a grid unit
    if (nearest != nullptr && bestDistance <= gridSize * 0.5)
        return nearest;

    return nullptr;
}

std::unique_ptr<juce::XmlElement> GrooveTemplate::createXml() const
{
    auto xml = std::make_unique<juce::XmlElement>("GROOVE_TEMPLATE");
    xml->setAttribute("name", templateName);
    xml->setAttribute("gridSize", gridSize);

    for (const auto& point : points)
    {
        auto* pointXml = xml->createNewChildElement("POINT");
        pointXml->setAttribute("gridPosition", point.gridPosition);
        pointXml->setAttribute("offset", point.offset);
        pointXml->setAttribute("velocityScale", static_cast<double>(point.velocityScale));
    }

    return xml;
}

void GrooveTemplate::loadFromXml(const juce::XmlElement& xml)
{
    templateName = xml.getStringAttribute("name", "Untitled Groove");
    gridSize = xml.getDoubleAttribute("gridSize", 480.0);

    points.clear();

    for (auto* pointXml : xml.getChildWithTagNameIterator("POINT"))
    {
        GroovePoint point;
        point.gridPosition = pointXml->getDoubleAttribute("gridPosition", 0.0);
        point.offset = pointXml->getDoubleAttribute("offset", 0.0);
        point.velocityScale = static_cast<float>(
            pointXml->getDoubleAttribute("velocityScale", 1.0));
        points.push_back(point);
    }
}
