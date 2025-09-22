package com.echopass.aat.aat

import com.echopass.aat.DeviceProfile
import com.echopass.aat.crypto.Hkdf
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.security.SecureRandom
import java.util.Base64
import kotlin.math.floor
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import org.junit.Test

class AATTokenBuilderTest {
    @Test
    fun `buildToken produces deterministic output with fixed random`() {
        val seed = Base64.getDecoder().decode("pdtgqZMTREOrutZssKp6ugPBI9KEhLHDbVhDz2AdXWI=")
        val profile = DeviceProfile(
            userId = "demo-user",
            kid = 13371337u,
            scopeId = 1u,
            scopeType = 1u,
            seed = seed,
        )
        val fixedRandom = object : SecureRandom() {
            private val pattern = byteArrayOf(0x10, 0x20, 0x30, 0x40, 0x50, 0x60)
            override fun nextBytes(bytes: ByteArray) {
                for (i in bytes.indices) {
                    bytes[i] = pattern[i % pattern.size]
                }
            }
        }
        val timestampSeconds = 1_735_300_800L // 2024-12-01T00:00:00Z
        val challengeId = 0x44332211u
        val builder = AATTokenBuilder(profile, slotSeconds = 30, tagLength = 12, random = fixedRandom)

        val token = builder.buildToken(challengeId, timestampSeconds)
        assertEquals(36, token.size)

        val buffer = ByteBuffer.wrap(token).order(ByteOrder.LITTLE_ENDIAN)
        val header = buffer.get().toInt() and 0xFF
        assertEquals(0x21, header)
        assertEquals(13371337, buffer.int)
        val expectedTslot = floor(timestampSeconds / 30.0).toUInt().toInt()
        assertEquals(expectedTslot, buffer.int)
        assertEquals(1, buffer.get().toInt())
        assertEquals(1, buffer.int)
        val nonce = ByteArray(6)
        buffer.get(nonce)
        assertContentEquals(byteArrayOf(0x10, 0x20, 0x30, 0x40, 0x50, 0x60), nonce)
        assertEquals(challengeId.toInt(), buffer.int)

        val payload = token.copyOfRange(0, 24)
        val epochDay = floor(timestampSeconds / 86_400.0).toUInt()
        val key = Hkdf.deriveKey(seed, profile.scopeId, epochDay, profile.scopeType)
        val expectedTag = Hkdf.hmacSha256(key, payload).copyOf(12)
        val tag = token.copyOfRange(24, 36)
        assertContentEquals(expectedTag, tag)
    }
}
