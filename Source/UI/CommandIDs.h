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

    // Project management
    static const int collectAllAndSave = 0x100C;

    // Track menu commands
    static const int addAudioTrack  = 0x2001;
    static const int addMidiTrack   = 0x2002;
    static const int deleteTrack    = 0x2003;
    static const int createGroupBus = 0x2004;
    static const int createAuxTrack = 0x2005;

    // View menu commands - Panel toggles
    static const int toggleBrowser   = 0x3001;
    static const int toggleMixer     = 0x3002;
    static const int togglePianoRoll = 0x3003;
    static const int zoomIn          = 0x3004;
    static const int zoomOut         = 0x3005;
    static const int zoomToFit       = 0x3006;

    // Tool mode commands
    static const int toolSelect      = 0x4001;
    static const int toolDraw        = 0x4002;
    static const int toolSplit       = 0x4003;
    static const int toolErase       = 0x4004;
    static const int toolMute        = 0x4005;

    // Edit commands
    static const int selectAll       = 0x5001;
    static const int deleteSelection = 0x5002;
    static const int splitAtPlayhead = 0x5003;
    static const int cut             = 0x5004;
    static const int copy            = 0x5005;
    static const int paste           = 0x5006;
    static const int duplicate       = 0x5007;
    static const int nudgeLeft       = 0x5008;
    static const int nudgeRight      = 0x5009;

    // Help menu commands
    static const int aboutMidiSing     = 0x6001;
    static const int keyboardShortcuts = 0x6002;

    // Export commands
    static const int exportMidi        = 0x7001;

    // Marker commands
    static const int addMarker         = 0x8001;
    static const int nextMarker        = 0x8002;
    static const int prevMarker        = 0x8003;
    static const int clearAllMarkers   = 0x8004;
    static const int renameMarker      = 0x8005;
    static const int deleteMarker      = 0x8006;

    // Punch-in/out commands
    static const int togglePunchIn     = 0x9001;
    static const int togglePunchOut    = 0x9002;

    // Audio region operations
    static const int reverseRegion     = 0xA001;
    static const int normalizeRegion   = 0xA002;
    static const int consolidateRegions = 0xA003;
    static const int editCrossfade     = 0xA004;

    // MIDI improvements
    static const int toggleArpeggiator = 0xB001;
    static const int scaleQuantize     = 0xB002;
    static const int extractGroove     = 0xB003;
    static const int applyGroove       = 0xB004;

    // Step sequencer
    static const int openStepSequencer = 0xC001;

    // Tempo track commands
    static const int addTempoEvent     = 0xD001;
    static const int editTempoEvent    = 0xD002;
    static const int deleteTempoEvent  = 0xD003;

    // Time signature track commands
    static const int addTimeSigEvent   = 0xD011;
    static const int editTimeSigEvent  = 0xD012;
    static const int deleteTimeSigEvent = 0xD013;

    // Track color and folder commands
    static const int setTrackColour    = 0xE001;
    static const int createTrackFolder = 0xE002;
    static const int removeTrackFolder = 0xE003;

    // Loudness meter
    static const int toggleLoudnessMeter = 0xF001;
}
