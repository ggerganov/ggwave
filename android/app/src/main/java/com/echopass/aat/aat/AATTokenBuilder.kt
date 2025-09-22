package com.echopass.aat.aat

import com.echopass.aat.DeviceProfile
import com.echopass.aat.crypto.Hkdf
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.security.SecureRandom
import kotlin.math.floor

class AATTokenBuilder(
    private val profile: DeviceProfile,
    private val slotSeconds: Int = 30,
    private val tagLength: Int = 12,
    private val random: SecureRandom = SecureRandom(),
) {
    fun buildToken(challengeId: UInt, timestampSeconds: Long = System.currentTimeMillis() / 1000): ByteArray {
        val buffer = ByteBuffer.allocate(TOKEN_LENGTH).order(ByteOrder.LITTLE_ENDIAN)
        val header = ((VERSION shl 5) or TYPE_WEB_LOGIN).toByte()
        buffer.put(header)
        buffer.putInt(profile.kid.toInt())
        val tslot = floor(timestampSeconds / slotSeconds.toDouble()).toUInt()
        buffer.putInt(tslot.toInt())
        buffer.put(profile.scopeType.toByte())
        buffer.putInt(profile.scopeId.toInt())
        val nonce = ByteArray(NONCE_LENGTH)
        random.nextBytes(nonce)
        buffer.put(nonce)
        buffer.putInt(challengeId.toInt())

        val payload = buffer.array().copyOfRange(0, PAYLOAD_LENGTH)
        val epochDay = floor(timestampSeconds / SECONDS_PER_DAY.toDouble()).toUInt()
        val key = Hkdf.deriveKey(profile.seed, profile.scopeId, epochDay, profile.scopeType)
        val tag = Hkdf.hmacSha256(key, payload).copyOf(tagLength)
        buffer.put(tag)
        return buffer.array()
    }

    companion object {
        private const val VERSION = 1
        private const val TYPE_WEB_LOGIN = 1
        private const val TOKEN_LENGTH = 36
        private const val PAYLOAD_LENGTH = 24
        private const val NONCE_LENGTH = 6
        private const val SECONDS_PER_DAY = 86_400
    }
}
