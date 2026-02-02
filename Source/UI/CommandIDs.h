#pragma once

namespace CommandIDs
{
    static const int playStop    = 0x1001;
    static const int record      = 0x1002;
    static const int save        = 0x1003;
    static const int open        = 0x1004;
    static const int undo        = 0x1005;
    static const int redo        = 0x1006;
    static const int projectNew  = 0x1007;
    static const int saveAs      = 0x1008;
    static const int openRecent  = 0x1009;
    static const int audioSettings = 0x100A;

    // Export
    static const int exportAudio   = 0x100B;

    // Track menu commands
    static const int addAudioTrack = 0x2001;
    static const int addMidiTrack  = 0x2002;
}
