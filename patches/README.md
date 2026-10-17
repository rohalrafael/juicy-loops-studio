# Patches to dependencies

Applied automatically by CMake when the dependency is fetched (`cmake/apply_patch.cmake`).
Each one should be reported upstream and removed once a release contains the fix.

| Patch | Fixes |
|---|---|
| `juce-xembed-use-after-free.patch` | JUCE 9.0.4 `XEmbedComponent` (Linux): a resize notification from the embedded window queues a callback holding a raw `this`; if the component is deleted before it runs (closing an LV2 plugin window right after a resize), it runs on freed memory and crashes. The callback now checks a `SafePointer` first. |
