package com.echopass.aat.crypto

import java.nio.ByteBuffer
import java.nio.ByteOrder
import javax.crypto.Mac
import javax.crypto.spec.SecretKeySpec

object Hkdf {
    fun deriveKey(seed: ByteArray, scopeId: UInt, epochDay: UInt, scopeType: UByte, outputSize: Int = 32): ByteArray {
        val salt = ByteBuffer.allocate(8).order(ByteOrder.LITTLE_ENDIAN)
        salt.putInt(scopeId.toInt())
        salt.putInt(epochDay.toInt())
        val info = ByteBuffer.allocate("AAT-v1".length + 1)
        info.put("AAT-v1".toByteArray())
        info.put(scopeType.toByte())
        val prk = hmacSha256(salt.array(), seed)
        return hkdfExpand(prk, info.array(), outputSize)
    }

    fun hmacSha256(key: ByteArray, data: ByteArray): ByteArray {
        val mac = Mac.getInstance("HmacSHA256")
        mac.init(SecretKeySpec(key, "HmacSHA256"))
        return mac.doFinal(data)
    }

    private fun hkdfExpand(prk: ByteArray, info: ByteArray, size: Int): ByteArray {
        val mac = Mac.getInstance("HmacSHA256")
        mac.init(SecretKeySpec(prk, "HmacSHA256"))
        var previous = ByteArray(0)
        var generated = ByteArray(0)
        var counter = 1
        while (generated.size < size) {
            mac.reset()
            mac.init(SecretKeySpec(prk, "HmacSHA256"))
            mac.update(previous)
            mac.update(info)
            mac.update(counter.toByte())
            previous = mac.doFinal()
            generated += previous
            counter++
        }
        return generated.copyOf(size)
    }
}
