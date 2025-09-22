package com.echopass.aat

import android.content.Context
import android.util.Base64

class DeviceRepository(context: Context) {
    private val prefs = context.getSharedPreferences("aat_device", Context.MODE_PRIVATE)

    fun getActiveProfile(): DeviceProfile {
        val userId = prefs.getString(KEY_USER_ID, DEFAULT_USER_ID) ?: DEFAULT_USER_ID
        val displayName = prefs.getString(KEY_DISPLAY_NAME, DEFAULT_DISPLAY_NAME)
        val kid = prefs.getLong(KEY_KID, DEFAULT_KID.toLong()).toUInt()
        val scopeId = prefs.getLong(KEY_SCOPE_ID, DEFAULT_SCOPE_ID.toLong()).toUInt()
        val scopeType = prefs.getInt(KEY_SCOPE_TYPE, DEFAULT_SCOPE_TYPE.toInt()).toUByte()
        val seedB64 = prefs.getString(KEY_SEED, DEFAULT_SEED) ?: DEFAULT_SEED
        val seed = Base64.decode(seedB64, Base64.DEFAULT)
        return DeviceProfile(
            userId = userId,
            displayName = displayName,
            kid = kid,
            scopeId = scopeId,
            scopeType = scopeType,
            seed = seed,
        )
    }

    fun saveProfile(profile: DeviceProfile, seedB64: String) {
        prefs.edit()
            .putString(KEY_USER_ID, profile.userId)
            .putString(KEY_DISPLAY_NAME, profile.displayName)
            .putLong(KEY_KID, profile.kid.toLong())
            .putLong(KEY_SCOPE_ID, profile.scopeId.toLong())
            .putInt(KEY_SCOPE_TYPE, profile.scopeType.toInt())
            .putString(KEY_SEED, seedB64)
            .apply()
    }

    companion object {
        private const val KEY_USER_ID = "user_id"
        private const val KEY_DISPLAY_NAME = "display_name"
        private const val KEY_KID = "kid"
        private const val KEY_SCOPE_ID = "scope_id"
        private const val KEY_SCOPE_TYPE = "scope_type"
        private const val KEY_SEED = "seed_b64"
        private const val DEFAULT_USER_ID = "demo-user"
        private const val DEFAULT_DISPLAY_NAME = "EchoPass Demo"
        private val DEFAULT_KID = 13371337u
        private val DEFAULT_SCOPE_ID = 1u
        private const val DEFAULT_SCOPE_TYPE = 1
        private const val DEFAULT_SEED = "pdtgqZMTREOrutZssKp6ugPBI9KEhLHDbVhDz2AdXWI="
    }
}
