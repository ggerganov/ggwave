package com.echopass.aat

import android.os.Bundle
import android.util.Base64
import android.view.inputmethod.EditorInfo
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import com.echopass.aat.aat.AATTokenBuilder
import com.echopass.aat.audio.GGWaveBridge
import com.echopass.aat.audio.TokenPlayer
import com.echopass.aat.databinding.ActivityMainBinding
import com.echopass.aat.databinding.DialogProvisionBinding
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.google.android.material.snackbar.Snackbar
import kotlin.math.roundToInt

class MainActivity : AppCompatActivity() {
    private lateinit var binding: ActivityMainBinding
    private lateinit var deviceRepository: DeviceRepository
    private lateinit var currentProfile: DeviceProfile
    private lateinit var tokenBuilder: AATTokenBuilder
    private lateinit var tokenPlayer: TokenPlayer

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        deviceRepository = DeviceRepository(this)
        currentProfile = deviceRepository.getActiveProfile()
        tokenBuilder = AATTokenBuilder(currentProfile)
        tokenPlayer = TokenPlayer(SAMPLE_RATE)

        updateProfileUi(currentProfile)

        binding.statusText.text = getString(R.string.challenge_hint)
        binding.volumeSlider.addOnChangeListener { _, value, _ ->
            binding.volumeLabel.text = "Громкость сигнала: ${value.roundToInt()}%"
        }
        binding.volumeSlider.value = 75f
        binding.volumeLabel.text = "Громкость сигнала: 75%"

        binding.playButton.setOnClickListener { playToken() }
        binding.manageProfileButton.setOnClickListener { showProvisioningDialog() }
        binding.challengeEditText.setOnEditorActionListener { _, actionId, _ ->
            if (actionId == EditorInfo.IME_ACTION_GO) {
                playToken()
                true
            } else {
                false
            }
        }
    }

    private fun playToken() {
        val challengeInput = binding.challengeEditText.text?.toString().orEmpty()
        val challenge = ChallengeParser.parse(challengeInput)
        if (challenge == null) {
            binding.challengeInputLayout.error = "Введите challenge (десятичный или HEX)"
            return
        }
        binding.challengeInputLayout.error = null

        binding.statusText.text = "Формируем токен..."
        val token = tokenBuilder.buildToken(challenge)
        val tokenB64 = Base64.encodeToString(token, Base64.NO_WRAP)
        binding.lastToken.text = getString(R.string.last_token_label) + "\n" + tokenB64

        try {
            val ggwaveVolume = binding.volumeSlider.value.roundToInt().coerceIn(20, 100)
            val samples = GGWaveBridge.encodeToken(
                token,
                SAMPLE_RATE,
                GGWaveBridge.PROTOCOL_AUDIBLE_FAST,
                ggwaveVolume,
            )
            if (samples.isEmpty()) {
                binding.statusText.text = "Не удалось сформировать аудиосигнал"
                return
            }
            binding.statusText.text = "Воспроизводим сигнал (~2 сек)"
            val gain = (binding.volumeSlider.value / 100f).coerceIn(0.2f, 1.0f)
            tokenPlayer.play(samples, gain)
            binding.statusText.postDelayed({
                binding.statusText.text = "Готово. Если сайт не поймал токен — повторите."
            }, 2500)
        } catch (t: Throwable) {
            binding.statusText.text = "Ошибка: ${t.message}"
            Snackbar.make(binding.root, "Ошибка: ${t.message}", Snackbar.LENGTH_LONG).show()
        }
    }

    private fun showProvisioningDialog() {
        val dialogBinding = DialogProvisionBinding.inflate(layoutInflater)
        val dialog = MaterialAlertDialogBuilder(this)
            .setTitle(R.string.profile_dialog_title)
            .setView(dialogBinding.root)
            .setNegativeButton(android.R.string.cancel, null)
            .setPositiveButton(R.string.profile_dialog_save, null)
            .create()

        dialog.setOnShowListener {
            val positiveButton = dialog.getButton(AlertDialog.BUTTON_POSITIVE)
            positiveButton.setOnClickListener {
                val rawCode = dialogBinding.provisionCodeEdit.text?.toString().orEmpty()
                val profile = ProvisioningParser.parse(rawCode)
                if (profile == null) {
                    dialogBinding.provisionCodeLayout.error = getString(R.string.profile_dialog_error)
                    return@setOnClickListener
                }
                dialogBinding.provisionCodeLayout.error = null
                applyProfile(profile)
                dialog.dismiss()
            }
        }

        dialog.show()
    }

    private fun applyProfile(profile: DeviceProfile) {
        currentProfile = profile
        tokenBuilder = AATTokenBuilder(profile)
        val seedB64 = Base64.encodeToString(profile.seed, Base64.NO_WRAP)
        deviceRepository.saveProfile(profile, seedB64)
        updateProfileUi(profile)
        binding.statusText.text = getString(R.string.challenge_hint)
        Snackbar.make(
            binding.root,
            getString(
                R.string.profile_updated,
                profile.displayName ?: profile.userId,
            ),
            Snackbar.LENGTH_LONG,
        ).show()
    }

    private fun updateProfileUi(profile: DeviceProfile) {
        val label = profile.displayName ?: profile.userId
        binding.profileTitle.text = getString(R.string.profile_active, label)
        binding.profileDetails.text = getString(
            R.string.profile_scope,
            profile.userId,
            profile.kid.toLong(),
            profile.scopeId.toLong(),
        )
    }

    companion object {
        private const val SAMPLE_RATE = 48_000
    }
}
