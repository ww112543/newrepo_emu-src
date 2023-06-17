// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.ui.viewholder

import android.view.View
import org.yuzu.yuzu_emu.databinding.ListItemSettingBinding
import org.yuzu.yuzu_emu.features.settings.model.view.SettingsItem
import org.yuzu.yuzu_emu.features.settings.model.view.SingleChoiceSetting
import org.yuzu.yuzu_emu.features.settings.model.view.StringSingleChoiceSetting
import org.yuzu.yuzu_emu.features.settings.ui.SettingsAdapter

class SingleChoiceViewHolder(val binding: ListItemSettingBinding, adapter: SettingsAdapter) :
    SettingViewHolder(binding.root, adapter) {
    private lateinit var setting: SettingsItem

    override fun bind(item: SettingsItem) {
        setting = item
        binding.textSettingName.setText(item.nameId)
        binding.textSettingDescription.visibility = View.VISIBLE
        if (item.descriptionId != 0) {
            binding.textSettingDescription.setText(item.descriptionId)
        } else if (item is SingleChoiceSetting) {
            val resMgr = binding.textSettingDescription.context.resources
            val values = resMgr.getIntArray(item.valuesId)
            for (i in values.indices) {
                if (values[i] == item.selectedValue) {
                    binding.textSettingDescription.text = resMgr.getStringArray(item.choicesId)[i]
                    return
                }
            }
        } else if (item is StringSingleChoiceSetting) {
            for (i in item.values!!.indices) {
                if (item.values[i] == item.selectedValue) {
                    binding.textSettingDescription.text = item.choices[i]
                    return
                }
            }
        } else {
            binding.textSettingDescription.visibility = View.GONE
        }
    }

    override fun onClick(clicked: View) {
        if (!setting.isEditable) {
            return
        }

        if (setting is SingleChoiceSetting) {
            adapter.onSingleChoiceClick(
                (setting as SingleChoiceSetting),
                bindingAdapterPosition
            )
        } else if (setting is StringSingleChoiceSetting) {
            adapter.onStringSingleChoiceClick(
                (setting as StringSingleChoiceSetting),
                bindingAdapterPosition
            )
        }
    }

    override fun onLongClick(clicked: View): Boolean {
        if (setting.isEditable) {
            return adapter.onLongClick(setting.setting!!, bindingAdapterPosition)
        }
        return false
    }
}
