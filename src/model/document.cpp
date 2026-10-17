#include "model/document.h"

namespace jls::model
{

namespace
{

// Selections and PAT/SONG mode are view state: undo/redo restores the project's content but
// leaves these as the user has them (as long as the selected pattern still exists).
void keepViewState(const Project& from, Project& to)
{
    to.songMode = from.songMode;
    if (to.findPattern(from.currentPattern) != nullptr)
        to.currentPattern = from.currentPattern;
    if (to.findChannel(from.selectedChannel) != nullptr)
        to.selectedChannel = from.selectedChannel;
    to.selectedMixerTrack = from.selectedMixerTrack;
    to.scale = from.scale;
    to.scaleRoot = from.scaleRoot;
}

} // namespace

Document::Document(Project initial) : current{std::move(initial), 0} {}

void Document::setViewState(const std::function<void(Project&)>& change)
{
    change(current.project);
    notify();
}

void Document::reset(Project project)
{
    current = {std::move(project), nextRevision++};
    savedRevision = current.revision;
    undoStack.clear();
    redoStack.clear();
    lastGestureId = 0;
    notify();
}

void Document::markSaved()
{
    savedRevision = current.revision;
    notify();
}

void Document::apply(const std::function<void(Project&)>& edit, int gestureId)
{
    const bool continuesGesture = gestureId != 0 && gestureId == lastGestureId;
    if (!continuesGesture)
    {
        undoStack.push_back(current);
        if (undoStack.size() > maxUndoSteps)
            undoStack.erase(undoStack.begin());
        redoStack.clear();
        current.revision = nextRevision++;
    }
    lastGestureId = gestureId;

    edit(current.project);
    notify();
}

bool Document::undo()
{
    if (undoStack.empty())
        return false;
    auto restored = std::move(undoStack.back());
    undoStack.pop_back();
    keepViewState(current.project, restored.project);
    redoStack.push_back(std::move(current));
    current = std::move(restored);
    lastGestureId = 0;
    notify();
    return true;
}

bool Document::redo()
{
    if (redoStack.empty())
        return false;
    auto restored = std::move(redoStack.back());
    redoStack.pop_back();
    keepViewState(current.project, restored.project);
    undoStack.push_back(std::move(current));
    current = std::move(restored);
    lastGestureId = 0;
    notify();
    return true;
}

void Document::notify()
{
    if (onChange)
        onChange();
}

} // namespace jls::model
