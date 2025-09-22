package com.echopass.aat

data class DeviceProfile(
    val userId: String,
    val displayName: String?,
    val kid: UInt,
    val scopeId: UInt,
    val scopeType: UByte,
    val seed: ByteArray,
)
