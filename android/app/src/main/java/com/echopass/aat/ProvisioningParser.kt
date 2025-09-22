package com.echopass.aat

import android.util.Base64
import org.json.JSONObject

object ProvisioningParser {
    fun parse(raw: String): DeviceProfile? {
        val trimmed = raw.trim()
        if (trimmed.isEmpty()) {
            return null
        }

        val decodedJson = decodeBase64(trimmed) ?: return null

        return try {
            val json = JSONObject(decodedJson)
            val userId = json.getString("userId")
            val displayName = json.optString("displayName").takeIf { it.isNotBlank() }
            val kid = json.getLong("kid").toUInt()
            val scopeId = json.getLong("scopeId").toUInt()
            val scopeTypeValue = json.optInt("scopeType", 1).coerceIn(0, 255)
            val seedB64 = json.getString("seedB64")
            val seed = Base64.decode(seedB64, Base64.DEFAULT)

            DeviceProfile(
                userId = userId,
                displayName = displayName,
                kid = kid,
                scopeId = scopeId,
                scopeType = scopeTypeValue.toUByte(),
                seed = seed,
            )
        } catch (_: Throwable) {
            null
        }
    }

    private fun decodeBase64(value: String): String? {
        val variants = listOf(
            Base64.URL_SAFE or Base64.NO_WRAP or Base64.NO_PADDING,
            Base64.NO_WRAP,
        )

        for (flags in variants) {
            try {
                val decoded = Base64.decode(value, flags)
                return String(decoded, Charsets.UTF_8)
            } catch (_: IllegalArgumentException) {
                // try next variant
            }
        }
        return null
    }
}
