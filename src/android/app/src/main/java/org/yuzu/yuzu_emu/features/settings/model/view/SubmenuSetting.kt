// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.model.view

import org.yuzu.yuzu_emu.features.settings.model.AbstractSetting

class SubmenuSetting(
    titleId: Int,
    descriptionId: Int,
    val menuKey: String
) : SettingsItem(null, titleId, descriptionId) {
    override val type = TYPE_SUBMENU
}
