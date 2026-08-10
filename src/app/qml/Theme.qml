pragma Singleton

import QtQuick

// Nocturne — the single source of visual truth for the editor.
//
// Every colour, size, radius and font in the QML tree comes from here. Nothing
// downstream hard-codes a hex or a magic pixel value, so retuning the app is a
// matter of editing this file.
//
// The palette is a dark, near-neutral blue-grey ground with one blurple accent
// used as a line and a glow rather than a flood. Contrast comes from the tonal
// ramps (100 = lightest … 900 = darkest), not from saturation.
QtObject {
    // ------------------------------------------------------------- utilities

    /// `c` at opacity `a`. Stands in for CSS colour-mix against a transparent
    /// stop, which is how the design system tints everything.
    function withAlpha(c, a) {
        return Qt.rgba(c.r, c.g, c.b, a)
    }

    // --------------------------------------------------------------- grounds

    readonly property color bg: "#161826"          // the page
    readonly property color surface: "#232532"     // docked panels
    readonly property color sunken: "#1b1d2b"      // fields and wells inside panels
    readonly property color stage: "#0d0e17"       // the letterbox around the canvas
    readonly property color scrim: Qt.rgba(0, 0, 0, 0.6)

    // ------------------------------------------------------------------ ink

    readonly property color text: "#e9e9ed"
    readonly property color textMuted: withAlpha(text, 0.72)
    readonly property color textFaint: withAlpha(text, 0.5)
    readonly property color textGhost: withAlpha(text, 0.32)
    readonly property color divider: withAlpha(text, 0.16)
    readonly property color dividerStrong: withAlpha(text, 0.28)

    // --------------------------------------------------------------- accent

    readonly property color accent: "#9184d9"
    readonly property color accent100: "#f5f4ff"
    readonly property color accent200: "#e7e5fe"
    readonly property color accent300: "#d2cefd"
    readonly property color accent400: "#b5abfc"
    readonly property color accent500: "#968ae0"
    readonly property color accent600: "#796cbf"
    readonly property color accent700: "#5d5294"
    readonly property color accent800: "#423a6a"
    readonly property color accent900: "#2b2741"

    readonly property color neutral100: "#f3f5fe"
    readonly property color neutral200: "#e4e7f5"
    readonly property color neutral300: "#cfd3e5"
    readonly property color neutral400: "#b2b6ca"
    readonly property color neutral500: "#9397ab"
    readonly property color neutral600: "#75798c"
    readonly property color neutral700: "#595d6c"
    readonly property color neutral800: "#3f424d"
    readonly property color neutral900: "#292b31"

    // Status hues. Kept low-chroma so they read as information, not decoration.
    readonly property color danger: "#e0899a"
    readonly property color warning: "#d9b184"

    // ------------------------------------------------------- interaction fills
    //
    // Hovers and pressed states are tints, never new colours: neutral tints for
    // secondary chrome, accent tints for anything the accent already owns.

    readonly property color hoverFill: withAlpha(text, 0.07)
    readonly property color pressFill: withAlpha(text, 0.14)
    readonly property color accentHoverFill: withAlpha(accent, 0.12)
    readonly property color accentPressFill: withAlpha(accent, 0.22)
    readonly property color accentSoftFill: withAlpha(accent, 0.14)
    readonly property color selectionFill: withAlpha(accent, 0.12)

    // -------------------------------------------------------------- geometry
    //
    // The compact 0.7x spacing scale from the design system. This interface is
    // dense on purpose: it is a control surface, not a document.

    readonly property int space1: 3
    readonly property int space2: 6
    readonly property int space3: 8
    readonly property int space4: 11
    readonly property int space6: 17
    readonly property int space8: 22

    readonly property int radiusSm: 4
    readonly property int radiusMd: 8
    readonly property int radiusLg: 14

    readonly property int controlHeight: 30
    readonly property int controlHeightSm: 26
    readonly property int rowHeight: 40
    readonly property int iconButtonSize: 28

    readonly property int topBarHeight: 52
    readonly property int panelHeaderHeight: 42
    readonly property int leftPanelWidth: 264
    readonly property int rightPanelWidth: 340
    readonly property int timelineHeight: 148

    // ------------------------------------------------------------------ type
    //
    // Inter if the machine has it, otherwise the closest thing the platform
    // ships. Resolved once here so no call site has to guess.

    readonly property string fontFamily: pickFont(
        ["Inter", "Inter Display", "Segoe UI Variable Text", "Segoe UI",
         "SF Pro Text", "Helvetica Neue"])
    readonly property string monoFamily: pickFont(
        ["JetBrains Mono", "Cascadia Mono", "SF Mono", "Consolas", "Menlo",
         "DejaVu Sans Mono"])

    function pickFont(candidates) {
        const available = Qt.fontFamilies()
        for (let i = 0; i < candidates.length; ++i) {
            if (available.indexOf(candidates[i]) >= 0)
                return candidates[i]
        }
        return Qt.application.font.family
    }

    readonly property int fontSizeTitle: 15
    readonly property int fontSizeBody: 13
    readonly property int fontSizeSmall: 12
    readonly property int fontSizeMicro: 11
    readonly property int fontSizeKicker: 10

    // Hierarchy here is size and space; headings never go past medium weight.
    readonly property int weightMedium: Font.Medium

    // ------------------------------------------------------------- animation

    readonly property int durFast: 90
    readonly property int durNormal: 150
}
