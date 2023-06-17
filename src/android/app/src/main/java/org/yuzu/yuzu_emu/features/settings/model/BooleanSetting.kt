// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.model

enum class BooleanSetting(
    override val key: String,
    override val section: String,
    override val defaultValue: Boolean
) : AbstractBooleanSetting {
    CPU_DEBUG_MODE("cpu_debug_mode", Settings.SECTION_CPU, false),
    FASTMEM("cpuopt_fastmem", Settings.SECTION_CPU, true),
    FASTMEM_EXCLUSIVES("cpuopt_fastmem_exclusives", Settings.SECTION_CPU, true),
    PICTURE_IN_PICTURE("picture_in_picture", Settings.SECTION_GENERAL, true),
    USE_CUSTOM_RTC("custom_rtc_enabled", Settings.SECTION_SYSTEM, false);

    override var boolean: Boolean = defaultValue

    override val valueAsString: String
        get() = boolean.toString()

    override val isRuntimeEditable: Boolean
        get() {
            for (setting in NOT_RUNTIME_EDITABLE) {
                if (setting == this) {
                    return false
                }
            }
            return true
        }

    companion object {
        private val NOT_RUNTIME_EDITABLE = listOf(
            PICTURE_IN_PICTURE,
            USE_CUSTOM_RTC
        )

        fun from(key: String): BooleanSetting? =
            BooleanSetting.values().firstOrNull { it.key == key }

        fun clear() = BooleanSetting.values().forEach { it.boolean = it.defaultValue }
    }
}
