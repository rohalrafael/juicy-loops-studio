#pragma once

#include "model/project.h"

#include <cstddef>
#include <functional>
#include <vector>

namespace jls::model
{

// The open project plus undo/redo and "unsaved changes" tracking. UI thread only.
//
// Undo works on whole-project snapshots: Project is a cheap-to-copy value (samples are shared),
// which keeps every edit undoable without writing an inverse for each operation. Each state
// carries a revision number, so undoing back to the saved state counts as unmodified.
class Document
{
  public:
    static constexpr std::size_t maxUndoSteps = 200;

    explicit Document(Project initial = {});

    const Project& project() const { return current.project; }

    // Applies an edit as one undo step. Consecutive edits with the same nonzero gesture id
    // (e.g. one mouse drag painting steps or turning a knob) merge into a single undo step.
    void apply(const std::function<void(Project&)>& edit, int gestureId = 0);

    // Changes view state (selected pattern, PAT/SONG mode) without creating an undo step or
    // marking the project modified.
    void setViewState(const std::function<void(Project&)>& change);

    // Replaces the whole project (File > New / Open): clears undo history, counts as saved.
    void reset(Project project);

    bool canUndo() const { return !undoStack.empty(); }
    bool canRedo() const { return !redoStack.empty(); }
    bool undo();
    bool redo();

    // True if the project differs from the last reset() or markSaved().
    bool isModified() const { return current.revision != savedRevision; }
    void markSaved();

    // Returns a fresh id for apply(); call once at the start of a gesture.
    int newGestureId() { return ++gestureCounter; }

    // Called after every change to project(), including undo/redo and reset.
    std::function<void()> onChange;

  private:
    struct State
    {
        Project project;
        int revision = 0;
    };

    void notify();

    State current;
    std::vector<State> undoStack;
    std::vector<State> redoStack;
    int nextRevision = 1;
    int savedRevision = 0;
    int lastGestureId = 0;
    int gestureCounter = 0;
};

} // namespace jls::model
