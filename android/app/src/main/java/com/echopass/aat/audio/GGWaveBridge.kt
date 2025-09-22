package com.echopass.aat.audio

object GGWaveBridge {
    init {
        System.loadLibrary("aat_bridge")
    }

    const val PROTOCOL_AUDIBLE_FAST = 1

    external fun encodeToken(token: ByteArray, sampleRate: Int, protocolId: Int, volume: Int): ShortArray
}
