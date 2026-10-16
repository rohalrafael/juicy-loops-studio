#include "model/document.h"

namespace jls::model
{

Document::Document(Project initial) : current(std::move(initial)) {}

void Document::apply(const std::function<void(Project&)>& edit, int gestureId)
{
    const bool continuesGesture = gestureId != 0 && gestureId == lastGestureId;
    if (!continuesGesture)
    {
        undoStack.push_back(current);
        if (undoStack.size() > maxUndoSteps)
            undoStack.erase(undoStack.begin());
        redoStack.clear();
    }
    lastGestureId = gestureId;

    edit(current);
    notify();
}

bool Document::undo()
{
    if (undoStack.empty())
        return false;
    redoStack.push_back(std::move(current));
    current = std::move(undoStack.back());
    undoStack.pop_back();
    lastGestureId = 0;
    notify();
    return true;
}

bool Document::redo()
{
    if (redoStack.empty())
        return false;
    undoStack.push_back(std::move(current));
    current = std::move(redoStack.back());
    redoStack.pop_back();
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
