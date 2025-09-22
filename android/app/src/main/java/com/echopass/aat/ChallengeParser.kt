package com.echopass.aat

object ChallengeParser {
    fun parse(input: String): UInt? {
        val sanitized = input.trim().replace(" ", "")
        if (sanitized.isEmpty()) return null
        return try {
            when {
                sanitized.startsWith("0x", ignoreCase = true) ->
                    sanitized.drop(2).toULong(16).toUInt()
                sanitized.any { it in 'a'..'f' || it in 'A'..'F' } ->
                    sanitized.toULong(16).toUInt()
                else -> sanitized.toULong().toUInt()
            }
        } catch (_: NumberFormatException) {
            null
        }
    }
}
