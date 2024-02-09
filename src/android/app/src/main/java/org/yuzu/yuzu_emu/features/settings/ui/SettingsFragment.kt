// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.ui

import android.annotation.SuppressLint
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.updatePadding
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.lifecycleScope
import androidx.lifecycle.repeatOnLifecycle
import androidx.navigation.findNavController
import androidx.navigation.fragment.navArgs
import androidx.recyclerview.widget.LinearLayoutManager
import com.google.android.material.transition.MaterialSharedAxis
import kotlinx.coroutines.flow.collectLatest
import kotlinx.coroutines.launch
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.databinding.FragmentSettingsBinding
import org.yuzu.yuzu_emu.features.settings.model.Settings
import org.yuzu.yuzu_emu.model.SettingsViewModel
import org.yuzu.yuzu_emu.utils.ViewUtils.updateMargins

class SettingsFragment : Fragment() {
    private lateinit var presenter: SettingsFragmentPresenter
    private var settingsAdapter: SettingsAdapter? = null

    private var _binding: FragmentSettingsBinding? = null
    private val binding get() = _binding!!

    private val args by navArgs<SettingsFragmentArgs>()

    private val settingsViewModel: SettingsViewModel by activityViewModels()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enterTransition = MaterialSharedAxis(MaterialSharedAxis.X, true)
        returnTransition = MaterialSharedAxis(MaterialSharedAxis.X, false)
        reenterTransition = MaterialSharedAxis(MaterialSharedAxis.X, false)
        exitTransition = MaterialSharedAxis(MaterialSharedAxis.X, true)
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentSettingsBinding.inflate(layoutInflater)
        return binding.root
    }

    // This is using the correct scope, lint is just acting up
    @SuppressLint("UnsafeRepeatOnLifecycleDetector")
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        settingsAdapter = SettingsAdapter(this, requireContext())
        presenter = SettingsFragmentPresenter(
            settingsViewModel,
            settingsAdapter!!,
            args.menuTag
        )

        binding.toolbarSettingsLayout.title = if (args.menuTag == Settings.MenuTag.SECTION_ROOT &&
            args.game != null
        ) {
            args.game!!.title
        } else {
            getString(args.menuTag.titleId)
        }
        binding.listSettings.apply {
            adapter = settingsAdapter
            layoutManager = LinearLayoutManager(requireContext())
        }

        binding.toolbarSettings.setNavigationOnClickListener {
            settingsViewModel.setShouldNavigateBack(true)
        }

        viewLifecycleOwner.lifecycleScope.apply {
            launch {
                repeatOnLifecycle(Lifecycle.State.CREATED) {
                    settingsViewModel.shouldReloadSettingsList.collectLatest {
                        if (it) {
                            settingsViewModel.setShouldReloadSettingsList(false)
                            presenter.loadSettingsList()
                        }
                    }
                }
            }
        }

        if (args.menuTag == Settings.MenuTag.SECTION_ROOT) {
            binding.toolbarSettings.inflateMenu(R.menu.menu_settings)
            binding.toolbarSettings.setOnMenuItemClickListener {
                when (it.itemId) {
                    R.id.action_search -> {
                        view.findNavController()
                            .navigate(R.id.action_settingsFragment_to_settingsSearchFragment)
                        true
                    }

                    else -> false
                }
            }
        }

        presenter.onViewCreated()

        setInsets()
    }

    private fun setInsets() {
        ViewCompat.setOnApplyWindowInsetsListener(
            binding.root
        ) { _: View, windowInsets: WindowInsetsCompat ->
            val barInsets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())
            val cutoutInsets = windowInsets.getInsets(WindowInsetsCompat.Type.displayCutout())

            val leftInsets = barInsets.left + cutoutInsets.left
            val rightInsets = barInsets.right + cutoutInsets.right

            binding.listSettings.updateMargins(left = leftInsets, right = rightInsets)
            binding.listSettings.updatePadding(bottom = barInsets.bottom)

            binding.appbarSettings.updateMargins(left = leftInsets, right = rightInsets)
            windowInsets
        }
    }
}
