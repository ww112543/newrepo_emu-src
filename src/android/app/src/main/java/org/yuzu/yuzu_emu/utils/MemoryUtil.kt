// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.utils

import android.app.ActivityManager
import android.content.Context
import android.os.Build
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.YuzuApplication
import java.util.Locale
import kotlin.math.ceil

object MemoryUtil {
    private val context get() = YuzuApplication.appContext

    private val Float.hundredths: String
        get() = String.format(Locale.ROOT, "%.2f", this)

    // Required total system memory
    const val REQUIRED_MEMORY = 8

    const val Kb: Float = 1024F
    const val Mb = Kb * 1024
    const val Gb = Mb * 1024
    const val Tb = Gb * 1024
    const val Pb = Tb * 1024
    const val Eb = Pb * 1024

    private fun bytesToSizeUnit(size: Float): String =
        when {
            size < Kb -> {
                context.getString(
                    R.string.memory_formatted,
                    size.hundredths,
                    context.getString(R.string.memory_byte)
                )
            }
            size < Mb -> {
                context.getString(
                    R.string.memory_formatted,
                    (size / Kb).hundredths,
                    context.getString(R.string.memory_kilobyte)
                )
            }
            size < Gb -> {
                context.getString(
                    R.string.memory_formatted,
                    (size / Mb).hundredths,
                    context.getString(R.string.memory_megabyte)
                )
            }
            size < Tb -> {
                context.getString(
                    R.string.memory_formatted,
                    (size / Gb).hundredths,
                    context.getString(R.string.memory_gigabyte)
                )
            }
            size < Pb -> {
                context.getString(
                    R.string.memory_formatted,
                    (size / Tb).hundredths,
                    context.getString(R.string.memory_terabyte)
                )
            }
            size < Eb -> {
                context.getString(
                    R.string.memory_formatted,
                    (size / Pb).hundredths,
                    context.getString(R.string.memory_petabyte)
                )
            }
            else -> {
                context.getString(
                    R.string.memory_formatted,
                    (size / Eb).hundredths,
                    context.getString(R.string.memory_exabyte)
                )
            }
        }

    // Devices are unlikely to have 0.5GB increments of memory so we'll just round up to account for
    // the potential error created by memInfo.totalMem
    private val totalMemory: Float
        get() {
            val memInfo = ActivityManager.MemoryInfo()
            with(context.getSystemService(Context.ACTIVITY_SERVICE) as ActivityManager) {
                getMemoryInfo(memInfo)
            }

            return ceil(
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
                    memInfo.advertisedMem.toFloat()
                } else {
                    memInfo.totalMem.toFloat()
                }
            )
        }

    fun isLessThan(minimum: Int, size: Float): Boolean =
        when (size) {
            Kb -> totalMemory < Mb && totalMemory < minimum
            Mb -> totalMemory < Gb && (totalMemory / Mb) < minimum
            Gb -> totalMemory < Tb && (totalMemory / Gb) < minimum
            Tb -> totalMemory < Pb && (totalMemory / Tb) < minimum
            Pb -> totalMemory < Eb && (totalMemory / Pb) < minimum
            Eb -> totalMemory / Eb < minimum
            else -> totalMemory < Kb && totalMemory < minimum
        }

    fun getDeviceRAM(): String = bytesToSizeUnit(totalMemory)
}
