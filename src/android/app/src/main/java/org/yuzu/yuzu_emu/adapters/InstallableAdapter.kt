// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.adapters

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import org.yuzu.yuzu_emu.databinding.CardInstallableBinding
import org.yuzu.yuzu_emu.model.Installable
import org.yuzu.yuzu_emu.viewholder.AbstractViewHolder

class InstallableAdapter(installables: List<Installable>) :
    AbstractListAdapter<Installable, InstallableAdapter.InstallableViewHolder>(installables) {
    override fun onCreateViewHolder(
        parent: ViewGroup,
        viewType: Int
    ): InstallableAdapter.InstallableViewHolder {
        CardInstallableBinding.inflate(LayoutInflater.from(parent.context), parent, false)
            .also { return InstallableViewHolder(it) }
    }

    inner class InstallableViewHolder(val binding: CardInstallableBinding) :
        AbstractViewHolder<Installable>(binding) {
        override fun bind(model: Installable) {
            binding.title.setText(model.titleId)
            binding.description.setText(model.descriptionId)

            if (model.install != null) {
                binding.buttonInstall.visibility = View.VISIBLE
                binding.buttonInstall.setOnClickListener { model.install.invoke() }
            }
            if (model.export != null) {
                binding.buttonExport.visibility = View.VISIBLE
                binding.buttonExport.setOnClickListener { model.export.invoke() }
            }
        }
    }
}
