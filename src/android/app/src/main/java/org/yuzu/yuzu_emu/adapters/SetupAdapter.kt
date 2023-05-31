// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.adapters

import android.text.Html
import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.res.ResourcesCompat
import androidx.recyclerview.widget.RecyclerView
import com.google.android.material.button.MaterialButton
import org.yuzu.yuzu_emu.databinding.PageSetupBinding
import org.yuzu.yuzu_emu.model.SetupPage

class SetupAdapter(val activity: AppCompatActivity, val pages: List<SetupPage>) :
    RecyclerView.Adapter<SetupAdapter.SetupPageViewHolder>() {
    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): SetupPageViewHolder {
        val binding = PageSetupBinding.inflate(LayoutInflater.from(parent.context), parent, false)
        return SetupPageViewHolder(binding)
    }

    override fun getItemCount(): Int = pages.size

    override fun onBindViewHolder(holder: SetupPageViewHolder, position: Int) =
        holder.bind(pages[position])

    inner class SetupPageViewHolder(val binding: PageSetupBinding) :
        RecyclerView.ViewHolder(binding.root) {
        lateinit var page: SetupPage

        init {
            itemView.tag = this
        }

        fun bind(page: SetupPage) {
            this.page = page
            binding.icon.setImageDrawable(
                ResourcesCompat.getDrawable(
                    activity.resources,
                    page.iconId,
                    activity.theme
                )
            )
            binding.textTitle.text = activity.resources.getString(page.titleId)
            binding.textDescription.text =
                Html.fromHtml(activity.resources.getString(page.descriptionId), 0)

            binding.buttonAction.apply {
                text = activity.resources.getString(page.buttonTextId)
                if (page.buttonIconId != 0) {
                    icon = ResourcesCompat.getDrawable(
                        activity.resources,
                        page.buttonIconId,
                        activity.theme
                    )
                }
                iconGravity =
                    if (page.leftAlignedIcon) {
                        MaterialButton.ICON_GRAVITY_START
                    } else {
                        MaterialButton.ICON_GRAVITY_END
                    }
                setOnClickListener {
                    page.buttonAction.invoke()
                }
            }
        }
    }
}
