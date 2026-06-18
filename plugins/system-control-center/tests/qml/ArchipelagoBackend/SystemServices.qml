import QtQuick
pragma Singleton

QtObject {
    signal tlpStateReady(bool available, string profile, string output, string errorString)
    signal tlpSetFinished(bool success, int exitCode, string output, string errorString)

    function requestTlpState() {
        tlpStateReady(false, "", "", "")
    }

    function setTlpMode(mode, sudoPassword) {
        tlpSetFinished(true, 0, mode, "")
    }
}
