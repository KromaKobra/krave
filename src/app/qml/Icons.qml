pragma Singleton

import QtQuick

// The icon set, as SVG path data on a 24x24 grid.
//
// Drawing icons as paths rather than shipping raster assets keeps them crisp at
// any DPI and lets every one of them take the colour of whatever it sits in --
// which is how the accent gets used here: as a line, not a fill.
//
// Paths under `filled` are painted; everything else is stroked with round caps.
QtObject {
    // -------------------------------------------------------------- transport
    readonly property string play: "M8 5.2 L18.5 12 L8 18.8 Z"
    readonly property string pause: "M8.4 5.5 H11 V18.5 H8.4 Z M13 5.5 H15.6 V18.5 H13 Z"
    readonly property string stop: "M7 7 H17 V17 H7 Z"
    readonly property string skipBack: "M7 6 H8.8 V18 H7 Z M17.5 6 L10 12 L17.5 18 Z"

    // ------------------------------------------------------------------ files
    readonly property string filePlus: "M6.2 3.8 H13.6 L18 8.2 V20.2 H6.2 Z M12 11.6 V17 M9.3 14.3 H14.7"
    readonly property string folderOpen: "M3.5 6.6 A1.6 1.6 0 0 1 5.1 5 H9.2 L11.3 7.3 H18.9 A1.6 1.6 0 0 1 20.5 8.9 V17.4 A1.6 1.6 0 0 1 18.9 19 H5.1 A1.6 1.6 0 0 1 3.5 17.4 Z"
    readonly property string save: "M5 5.6 A1.1 1.1 0 0 1 6.1 4.5 H16.2 L19.5 7.8 V18.4 A1.1 1.1 0 0 1 18.4 19.5 H6.1 A1.1 1.1 0 0 1 5 18.4 Z M8.6 4.5 V9.4 H15.2 V4.5 M8.6 19.5 V14.2 H15.4 V19.5"
    readonly property string importArrow: "M12 15.4 V4.6 M8.2 8.4 L12 4.6 L15.8 8.4 M5 15.6 V18.4 A2 2 0 0 0 7 20.4 H17 A2 2 0 0 0 19 18.4 V15.6"
    readonly property string exportArrow: "M12 4.6 V15.4 M8.2 11.6 L12 15.4 L15.8 11.6 M5 15.6 V18.4 A2 2 0 0 0 7 20.4 H17 A2 2 0 0 0 19 18.4 V15.6"

    // ------------------------------------------------------------ layer types
    readonly property string stack: "M12 3.8 L21 8.4 L12 13 L3 8.4 Z M3 12.3 L12 16.9 L21 12.3 M3 15.9 L12 20.5 L21 15.9"
    readonly property string waveform: "M4 10 V14 M8 6.6 V17.4 M12 4 V20 M16 6.6 V17.4 M20 10 V14"
    readonly property string bars: "M5.5 13 V19 M9.5 8.4 V19 M13.5 5 V19 M17.5 10.6 V19"
    readonly property string textT: "M5.6 6.6 V4.8 H18.4 V6.6 M12 4.8 V19.2 M9 19.2 H15"
    readonly property string sparkle: "M12 3.8 L13.7 10.3 L20.2 12 L13.7 13.7 L12 20.2 L10.3 13.7 L3.8 12 L10.3 10.3 Z"
    readonly property string image: "M3.8 5.6 H20.2 V18.4 H3.8 Z M3.8 15.6 L9 10.6 L12.8 14 M12.8 14 L15.7 11.4 L20.2 15.4 M16.9 8.7 A1.3 1.3 0 1 1 14.3 8.7 A1.3 1.3 0 1 1 16.9 8.7 Z"

    // ------------------------------------------------------------- interface
    readonly property string plus: "M12 5.4 V18.6 M5.4 12 H18.6"
    readonly property string close: "M6.6 6.6 L17.4 17.4 M17.4 6.6 L6.6 17.4"
    readonly property string check: "M5.6 12.6 L10 17 L18.4 7.6"
    readonly property string chevronDown: "M6.6 9.6 L12 15 L17.4 9.6"
    readonly property string arrowUp: "M12 18.6 V5.6 M6.6 11 L12 5.6 L17.4 11"
    readonly property string arrowDown: "M12 5.4 V18.4 M6.6 13 L12 18.4 L17.4 13"
    readonly property string trash: "M4.6 7 H19.4 M9.5 7 V4.8 H14.5 V7 M6.7 7 L7.7 19.2 H16.3 L17.3 7 M10.3 10.3 V16.2 M13.7 10.3 V16.2"
    readonly property string eye: "M2.6 12 C5.6 7.1 8.8 5.3 12 5.3 C15.2 5.3 18.4 7.1 21.4 12 C18.4 16.9 15.2 18.7 12 18.7 C8.8 18.7 5.6 16.9 2.6 12 Z M14.8 12 A2.8 2.8 0 1 1 9.2 12 A2.8 2.8 0 1 1 14.8 12 Z"
    readonly property string eyeOff: "M4.2 4.6 L19.8 19.4 M9.7 5.9 C10.4 5.7 11.2 5.6 12 5.6 C15.2 5.6 18.4 7.4 21.4 12 C20.3 13.8 19.1 15.2 17.8 16.2 M14.4 17.9 C13.6 18.1 12.8 18.2 12 18.2 C8.8 18.2 5.6 16.4 2.6 12 C3.9 10 5.3 8.5 6.8 7.5"
    readonly property string speaker: "M4.6 9.4 H8.2 L12.6 5.4 V18.6 L8.2 14.6 H4.6 Z M15.6 9.6 A4.2 4.2 0 0 1 15.6 14.4 M18.2 7.3 A7.8 7.8 0 0 1 18.2 16.7"
    readonly property string speakerMute: "M4.6 9.4 H8.2 L12.6 5.4 V18.6 L8.2 14.6 H4.6 Z M16 9.8 L20.6 14.2 M20.6 9.8 L16 14.2"
    readonly property string sliders: "M4.6 7.6 H12.6 M17.4 7.6 H19.4 M4.6 16.4 H7.4 M12.2 16.4 H19.4 M17 7.6 A2.2 2.2 0 1 1 12.6 7.6 A2.2 2.2 0 1 1 17 7.6 Z M11.8 16.4 A2.2 2.2 0 1 1 7.4 16.4 A2.2 2.2 0 1 1 11.8 16.4 Z"
    readonly property string frameCorners: "M4.4 8.6 V4.4 H8.6 M15.4 4.4 H19.6 V8.6 M19.6 15.4 V19.6 H15.4 M8.6 19.6 H4.4 V15.4"
    readonly property string warning: "M12 4.4 L21.4 20 H2.6 Z M12 10 V14.4 M12 16.8 V17.2"
    readonly property string info: "M20.2 12 A8.2 8.2 0 1 1 3.8 12 A8.2 8.2 0 1 1 20.2 12 Z M12 11.2 V16.4 M12 7.8 V8.2"
    readonly property string spinner: "M12 3.4 A8.6 8.6 0 0 1 20.6 12"
    readonly property string link: "M10.2 13.8 A3.6 3.6 0 0 0 15.3 13.8 L18.1 11 A3.6 3.6 0 0 0 13 5.9 L11.6 7.3 M13.8 10.2 A3.6 3.6 0 0 0 8.7 10.2 L5.9 13 A3.6 3.6 0 0 0 11 18.1 L12.4 16.7"
}
