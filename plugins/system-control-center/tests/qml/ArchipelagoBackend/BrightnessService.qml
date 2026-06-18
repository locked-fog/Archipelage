import QtQuick
pragma Singleton

QtObject {
    property real brightness: 0.62
    function requestBrightness() {}
    function setBrightness(value) {
        brightness = value
    }
}
