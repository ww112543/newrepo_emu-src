// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.model

import androidx.lifecycle.ViewModel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow

class EmulationViewModel : ViewModel() {
    val emulationStarted: StateFlow<Boolean> get() = _emulationStarted
    private val _emulationStarted = MutableStateFlow(false)

    val isEmulationStopping: StateFlow<Boolean> get() = _isEmulationStopping
    private val _isEmulationStopping = MutableStateFlow(false)

    val shaderProgress: StateFlow<Int> get() = _shaderProgress
    private val _shaderProgress = MutableStateFlow(0)

    val totalShaders: StateFlow<Int> get() = _totalShaders
    private val _totalShaders = MutableStateFlow(0)

    val shaderMessage: StateFlow<String> get() = _shaderMessage
    private val _shaderMessage = MutableStateFlow("")

    fun setEmulationStarted(started: Boolean) {
        _emulationStarted.value = started
    }

    fun setIsEmulationStopping(value: Boolean) {
        _isEmulationStopping.value = value
    }

    fun setShaderProgress(progress: Int) {
        _shaderProgress.value = progress
    }

    fun setTotalShaders(max: Int) {
        _totalShaders.value = max
    }

    fun setShaderMessage(msg: String) {
        _shaderMessage.value = msg
    }

    fun updateProgress(msg: String, progress: Int, max: Int) {
        setShaderMessage(msg)
        setShaderProgress(progress)
        setTotalShaders(max)
    }

    fun clear() {
        setEmulationStarted(false)
        setIsEmulationStopping(false)
        setShaderProgress(0)
        setTotalShaders(0)
        setShaderMessage("")
    }

    companion object {
        const val KEY_EMULATION_STARTED = "EmulationStarted"
        const val KEY_IS_EMULATION_STOPPING = "IsEmulationStarting"
        const val KEY_SHADER_PROGRESS = "ShaderProgress"
        const val KEY_TOTAL_SHADERS = "TotalShaders"
        const val KEY_SHADER_MESSAGE = "ShaderMessage"
    }
}
