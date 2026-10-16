#pragma once

#include "model/project.h"

#include <cstddef>
#include <functional>
#include <vector>

namespace jls::model
{

// The open project plus undo/redo. UI thread only.
//
// Undo works on whole-project snapshots: Project is a cheap-to-copy value (samples are shared),
// which keeps every edit undoable without writing an inverse for each operation.
class Document
{
  public:
    static constexpr std::size_t maxUndoSteps = 200;

    explicit Document(Project initial = {});

    const Project& project() const { return current; }

    // Applies an edit as one undo step. Consecutive edits with the same nonzero gesture id
    // (e.g. one mouse drag painting steps or turning a knob) merge into a single undo step.
    void apply(const std::function<void(Project&)>& edit, int gestureId = 0);

    bool canUndo() const { return !undoStack.empty(); }
    bool canRedo() const { return !redoStack.empty(); }
    bool undo();
    bool redo();

    // Returns a fresh id for apply(); call once at the start of a gesture.
    int newGestureId() { return ++gestureCounter; }

    // Called after every change to project(), including undo/redo.
    std::function<void()> onChange;

  private:
    void notify();

    Project current;
    std::vector<Project> undoStack;
    std::vector<Project> redoStack;
    int lastGestureId = 0;
    int gestureCounter = 0;
};

} // namespace jls::model
