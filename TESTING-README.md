# Subtask 5-1: Comprehensive Save/Load Workflow Test

## Status: Ready for Manual Testing ⏸️

All code implementation from phases 1-4 has been completed and verified. The application is built and ready for end-to-end integration testing.

## What Has Been Prepared

### 1. Test Documentation
- **📋 E2E Test Plan** (`.auto-claude/specs/005-project-save-load/e2e-test-plan.md`)
  - 8 major test sections
  - 40+ detailed test steps
  - Pass/fail criteria
  - Results documentation template

- **📖 Test Execution Guide** (`.auto-claude/specs/005-project-save-load/TEST-EXECUTION-GUIDE.md`)
  - Quick start instructions
  - Critical test scenarios
  - Success/failure indicators
  - Expected test duration: 30-45 minutes

- **✅ Automated Verification** (`.auto-claude/specs/005-project-save-load/automated-verification.md`)
  - Verified all implementations from phases 1-4
  - Confirmed 100% code completeness
  - Build verification passed

### 2. Application Build
- **Location**: `build/MidiSing_artefacts/Debug/MidiSing.exe`
- **Size**: 18.4 MB
- **Status**: Built successfully, ready to run

## How to Execute the Test

### Quick Start

1. **Launch the Application**
   ```bash
   cd C:\Users\iszun\Downloads\MidiSing\midising\.auto-claude\worktrees\tasks\005-project-save-load
   .\build\MidiSing_artefacts\Debug\MidiSing.exe
   ```

2. **Open the Test Plan**
   - File: `.auto-claude/specs/005-project-save-load/e2e-test-plan.md`
   - Follow all steps in sequential order
   - Check off each completed step

3. **Execute All Test Scenarios**
   - Part 1: Project Creation and Data Entry
   - Part 2: Save and Verify Recent Files
   - Part 3: Close and Reopen Project
   - Part 4: Unsaved Changes Indicator
   - Part 5: Save As Functionality
   - Part 6: Open Recent Menu
   - Part 7: New Project with Unsaved Changes Prompt
   - Part 8: Close with Unsaved Changes Prompt

4. **Document Results**
   - Mark each test as passed/failed
   - Document any issues found
   - Complete the test results summary

## What Features Are Being Tested

### Core Save/Load Functionality
✅ Track configurations (type, name, color)
✅ MIDI note data (pitch, position, duration)
✅ Audio clip references
✅ Mixer settings (volume, pan, mute, solo, arm)
✅ Effect chains and parameters
✅ Playhead position
✅ Loop settings (enabled, start, end)
✅ BPM

### User Experience Features
✅ Recent files list
✅ Unsaved changes indicator (asterisk in title)
✅ File menu with keyboard shortcuts
✅ Save As creates independent copy
✅ New Project command
✅ Unsaved changes prompts (3 options: Save, Don't Save, Cancel)

## Acceptance Criteria Validation

This test validates ALL 10 acceptance criteria from `spec.md`:
- [ ] Users can save a project to a .midising file
- [ ] Users can open/load a previously saved project
- [ ] All track configurations are preserved (type, name, color)
- [ ] All MIDI note data is preserved exactly
- [ ] Audio clip references and positions are preserved
- [ ] Mixer settings (volume, pan, mute, solo) are preserved
- [ ] Effect chains and parameters are preserved
- [ ] BPM and time position are preserved
- [ ] Recent projects list is maintained for quick access
- [ ] Unsaved changes prompt appears before closing

## After Testing

### If All Tests Pass ✅
1. Update test plan with results
2. Mark subtask-5-1 as completed in implementation_plan.json
3. Commit with message: `auto-claude: subtask-5-1 - Comprehensive save/load workflow test`
4. Proceed to subtask-5-2 (edge cases testing)

### If Tests Fail ❌
1. Document failures in test plan "Issues Found" section
2. Investigate root cause in source code
3. Fix the issues
4. Re-run tests
5. Only mark complete when all tests pass

## Key Files Modified (Previous Subtasks)

All implementation is complete from subtasks 1-1 through 4-4:
- `Source/Serialization/ProjectSerializer.cpp/h` - Enhanced serialization
- `Source/Utils/RecentFilesManager.cpp/h` - Recent files tracking
- `Source/MainComponent.cpp/h` - Unsaved changes, menu bar, commands
- `Source/Main.cpp` - Close prompt
- `Source/UI/CommandIDs.h` - New command IDs
- `CMakeLists.txt` - Build configuration

## Notes

- This is a **manual test** - it cannot be automated via command line
- **GUI interaction required** - file dialogs, menu clicks, application lifecycle
- **Estimated time**: 30-45 minutes for complete testing
- **Use clean test files** - don't overwrite important projects
- **Test in a temporary directory** to avoid cluttering user documents

## Questions or Issues?

If you encounter any problems during testing:
1. Document in the test plan
2. Check the source code for the relevant feature
3. Verify the .midising XML file structure
4. Take screenshots if helpful
5. Review automated-verification.md for implementation details

---

**Ready to begin testing!** Launch the application and follow the test plan. 🚀
