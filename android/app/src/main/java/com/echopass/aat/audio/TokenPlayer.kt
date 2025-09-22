package com.echopass.aat.audio

import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioTrack
import kotlin.math.roundToInt

class TokenPlayer(private val sampleRate: Int) {
    fun play(samples: ShortArray, volume: Float = 1f) {
        if (samples.isEmpty()) return
        val attributes = AudioAttributes.Builder()
            .setUsage(AudioAttributes.USAGE_ASSISTANCE_SONIFICATION)
            .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION)
            .build()

        val format = AudioFormat.Builder()
            .setSampleRate(sampleRate)
            .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
            .setChannelMask(AudioFormat.CHANNEL_OUT_MONO)
            .build()

        val track = AudioTrack.Builder()
            .setAudioAttributes(attributes)
            .setAudioFormat(format)
            .setTransferMode(AudioTrack.MODE_STATIC)
            .setBufferSizeInBytes(samples.size * 2)
            .build()

        val scaled = if (volume in 0f..1f && volume != 1f) {
            ShortArray(samples.size) { index ->
                (samples[index] * volume).roundToInt().coerceIn(Short.MIN_VALUE.toInt(), Short.MAX_VALUE.toInt()).toShort()
            }
        } else {
            samples
        }

        track.write(scaled, 0, scaled.size)
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.LOLLIPOP) {
            track.setVolume(1f)
        }
        track.setNotificationMarkerPosition(scaled.size)
        track.setPlaybackPositionUpdateListener(object : AudioTrack.OnPlaybackPositionUpdateListener {
            override fun onMarkerReached(track: AudioTrack?) {
                track?.stop()
                track?.release()
            }

            override fun onPeriodicNotification(track: AudioTrack?) = Unit
        })
        track.play()
    }
}
