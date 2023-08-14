// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.adapters

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.app.AppCompatActivity
import androidx.recyclerview.widget.RecyclerView
import androidx.recyclerview.widget.RecyclerView.ViewHolder
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.databinding.ListItemSettingBinding
import org.yuzu.yuzu_emu.fragments.LicenseBottomSheetDialogFragment
import org.yuzu.yuzu_emu.model.License

class LicenseAdapter(private val activity: AppCompatActivity, var licenses: List<License>) :
    RecyclerView.Adapter<LicenseAdapter.LicenseViewHolder>(),
    View.OnClickListener {
    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): LicenseViewHolder {
        val binding =
            ListItemSettingBinding.inflate(LayoutInflater.from(parent.context), parent, false)
        binding.root.setOnClickListener(this)
        return LicenseViewHolder(binding)
    }

    override fun getItemCount(): Int = licenses.size

    override fun onBindViewHolder(holder: LicenseViewHolder, position: Int) {
        holder.bind(licenses[position])
    }

    override fun onClick(view: View) {
        val license = (view.tag as LicenseViewHolder).license
        LicenseBottomSheetDialogFragment.newInstance(license)
            .show(activity.supportFragmentManager, LicenseBottomSheetDialogFragment.TAG)
    }

    inner class LicenseViewHolder(val binding: ListItemSettingBinding) : ViewHolder(binding.root) {
        lateinit var license: License

        init {
            itemView.tag = this
        }

        fun bind(license: License) {
            this.license = license

            val context = YuzuApplication.appContext
            binding.textSettingName.text = context.getString(license.titleId)
            binding.textSettingDescription.text = context.getString(license.descriptionId)
            binding.textSettingValue.visibility = View.GONE
        }
    }
}
